#include <RE/Skyrim.h>
#include <REL/Version.h>
#include <SKSE/SKSE.h>
#include <SKSEMenuFramework.h>
#include <fmt/format.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <Windows.h>
#include "PPAInterface.h"
#include "PhysicsDecision.h"
#include "SettingsStore.h"
#include "SPSAPI.h"
#include "api/ExternalControlRegistry.h"
#include "controllers/ArousalController.h"
#include "controllers/ActorContext.h"
#include "controllers/SceneController.h"
#include "controllers/SceneState.h"
#include "diagnostics/ActivityLog.h"
#include "diagnostics/Diagnostics.h"
#include "runtime/Compatibility.h"
#include "runtime/PapyrusGateway.h"
#include "runtime/PhysicsOwnershipAdapter.h"
#include "runtime/PositionBackendAdapter.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace logger = SKSE::log;

namespace Mod {
namespace fs = std::filesystem;

constexpr auto kName = "Schlong Physics Swapper";
constexpr auto kVersion = "1.9.6";
constexpr auto kIni = "Data/SKSE/Plugins/SchlongPhysicsSwapper.ini";
constexpr auto kLegacyIni = "Data/SKSE/Plugins/UBEPhysicsSwitch.ini";
constexpr auto kReport = "Data/SKSE/Plugins/SchlongPhysicsSwapper_Diagnostics.txt";
constexpr auto kCaptureReport = "Data/SKSE/Plugins/SchlongPhysicsSwapper_DebugCapture.txt";
using SPS::Core::Settings;
using Diagnostics = SPS::Diagnostics::Snapshot;
using SPS::Runtime::ArousalProviderName;
using SPS::Runtime::ClassicArousedLoaded;
using SPS::Runtime::FsmpActorApiAvailable;
using SPS::Runtime::LegacySosLoaded;
using SPS::Runtime::LoadedDllVersion;
using SPS::Runtime::OslArousedLoaded;
using SPS::Runtime::PluginLoaded;
using SPS::Runtime::PositionBackendAvailable;
using SPS::Runtime::PositionBackendName;
using SPS::Runtime::SloArousedLoaded;
using SPS::Runtime::SosAeNativeLoaded;
using SPS::Runtime::SosAeNativeModuleName;
using SPS::Runtime::TngLoaded;

Settings settings;
Diagnostics diagnostics;
std::mutex settingsLock;
std::mutex diagnosticsLock;
SPS::Diagnostics::ActivityLog activityLog;
std::string runtimeVersion{ "unknown" };
std::string skseVersion{ "unknown" };
std::mutex debugCaptureLock;
std::vector<std::string> debugCaptureLines;
SPS::APIControl::ExternalControlRegistry apiRegistry;

SPS::Controllers::ArousalController arousalController;
SPS::Controllers::ActorContext playerContext{ 0x14 };
SPS::Controllers::SceneController sceneController;
std::atomic<bool> polling{ false };
std::atomic<std::int64_t> loadSMPResetDueMs{ 0 };
std::atomic<std::int64_t> loadSMPResetRestoreDueMs{ 0 };
std::atomic<std::int64_t> softHandoffResetDueMs{ 0 };
std::atomic<std::int64_t> softHandoffResetUntilMs{ 0 };
std::atomic<std::int64_t> softHandoffResetRestoreDueMs{ 0 };
std::atomic<std::int64_t> softAngleRefreshDueMs{ 0 };
std::atomic<std::int64_t> softAngleRefreshRestoreDueMs{ 0 };
std::atomic<std::int64_t> lastBendApplyMs{ 0 };
std::atomic<std::int64_t> bendSettleDueMs{ 0 };
std::atomic<std::int64_t> bendConfirmationDueMs{ 0 };
std::atomic<std::int64_t> bendRetryDueMs{ 0 };
std::atomic<std::int64_t> softConfirmationDueMs{ 0 };
std::atomic<std::int64_t> softConfirmationUntilMs{ 0 };
std::atomic<std::int64_t> cbpcConfirmationDueMs{ 0 };
std::atomic<std::int64_t> cbpcConfirmationUntilMs{ 0 };
std::atomic<std::int64_t> erectionAnimationStartMs{ 0 };
std::atomic<std::int64_t> bendGuardUntilMs{ 0 };
std::atomic<std::int64_t> bendGuardWindowStartMs{ 0 };
std::atomic<std::int64_t> nodeRefreshDueMs{ 0 };
std::atomic<std::int64_t> nodeRefreshFollowupDueMs{ 0 };
std::atomic<std::int64_t> nodeCBPCReacquireDueMs{ 0 };
std::atomic<std::int64_t> nodeCBPCReacquireUntilMs{ 0 };
std::atomic<std::int64_t> erectMeshReplayDueMs{ 0 };
std::atomic<std::int64_t> nodeSMPResetRestoreDueMs{ 0 };
std::atomic<std::int64_t> diagnosticsRefreshDueMs{ 0 };
std::atomic<std::int64_t> manualPhysicsTestUntilMs{ 0 };
std::atomic<int> activeManualPhysicsTest{ -1 };  // -1 none, 0 SMP, 1 CBPC
std::atomic<int> pendingQuickAction{ -1 };       // -1 none, 0 SMP test, 1 CBPC test, 2 repair
std::atomic<std::int64_t> pendingQuickActionUntilMs{ 0 };
std::atomic<std::int64_t> startupReconcileDueMs{ 0 };
std::atomic<std::int64_t> startupReconcileUntilMs{ 0 };
std::atomic<std::int64_t> postSwitchVerificationDueMs{ 0 };
std::atomic<std::int64_t> postSwitchVerificationUntilMs{ 0 };
std::atomic<std::int64_t> externalOwnerRepairDueMs{ 0 };
std::atomic<std::int64_t> externalOwnerRepairUntilMs{ 0 };
std::atomic<std::int64_t> lastOwnerRestorationMs{ 0 };
std::atomic<std::int64_t> ignoreNodeEventsUntilMs{ 0 };
std::atomic<std::int64_t> papyrusDispatchAllowedAfterMs{ 0 };
std::atomic<int> erectionAnimationTargetBend{ 0 };
std::atomic<int> erectionAnimationLastQueuedBend{ -1 };
std::atomic<int> bendGuardCount{ 0 };
std::atomic<int> bendConsecutiveFailures{ 0 };
std::atomic<bool> bendFailureReported{ false };
std::atomic<bool> erectionAnimating{ false };
std::atomic<std::int64_t> randomErectionNextMs{ 0 };
std::atomic<std::int64_t> randomErectionUntilMs{ 0 };
std::atomic<bool> randomErectionManualTest{ false };
std::atomic<bool> erectionRelaxing{ false };
std::atomic<std::int64_t> spontaneousRefractoryUntilMs{ 0 };
std::atomic<std::int64_t> morningErectionDueMs{ 0 };
std::atomic<bool> morningErectionActive{ false };
std::atomic<float> sleepWaitStartedHours{ -1.0F };
std::atomic<std::uint64_t> erectionAnimationGeneration{ 0 };
std::atomic<std::int64_t> debugCaptureStartedMs{ 0 };
std::atomic<std::int64_t> debugCaptureUntilMs{ 0 };
std::atomic<std::uint64_t> debugCaptureGeneration{ 0 };
std::atomic<unsigned> externalResetNotices{ 0 };
std::atomic<unsigned> ownerRestorations{ 0 };
std::atomic<bool> fsmpCompatibilityWarningShown{ false };
std::jthread pollThread;
std::jthread erectionAnimationThread;
std::jthread debugCaptureThread;
std::mutex randomLock;
std::mt19937 randomEngine{ std::random_device{}() };
const SPS::PPA::InterfaceV1* ppaAPI{ nullptr };
SPS::PPA::ListenerHandle ppaListener{ 0 };

void Evaluate(bool force = false);
void QuerySexLab();
void QuerySexLabRole();
void QueryOStimRole();
void RefreshDiagnostics();
void Save();
bool PapyrusReadyForDispatch();
bool SexLabHasPriority(const Settings& copy);
bool OStimHasPriority(const Settings& copy);
bool AnySceneHasPriority(const Settings& copy);
void ApplyRequestedBend(bool force, bool animate, bool automatic);
void ApplyRequestedSoftBend(bool force, bool animate);
void ScheduleSoftAngleRefresh(int delayMs = 350);
void RunSoftAngleRefresh();
void CancelErectionAnimation();
void StartGradualRelaxation(const Settings& copy);
bool ClearResolvedPositionError();
std::string BuildReport();
std::int64_t NowMs();
void Record(std::string message, bool error = false);
std::optional<SPS::APIControl::Request> ActiveAPIRequest();
void ClearAPIRequests();
void ScheduleExternalOwnerRepair(int delayMs, std::string_view reason);
bool SetOwner(bool cbpc, bool force = false);
void ProcessPendingQuickAction();

const char* SexLabRoleName(int role) {
    switch (role) {
    case 1: return "Receiving / bottom";
    case 2: return "Penetrating / top";
    default: return "Not identified";
    }
}

bool PPAOwnsPosition() {
    if (::GetModuleHandleW(SPS::PPA::kPluginDLL) == nullptr) return false;
    // P+ knows the player's directional role. A receiving player keeps the
    // state captured when the scene started; PPA owns scene alignment only
    // when that locked state is CBPC/erect. Penetrating is always CBPC.
    if (sceneController.State().sexLab.active.load() && sceneController.State().sexLab.roleValid.load()) {
        if (sceneController.State().sexLab.role.load() == 1) return playerContext.physics.usingCBPC.load();
        if (sceneController.State().sexLab.role.load() == 2) return true;
        return playerContext.physics.usingCBPC.load();
    }
    // The listener gives exact scene state on current PPA builds. Retain the
    // conservative SexLab fallback for older PPA versions without the export.
    const bool recentPPAUpdate = sceneController.State().ppa.sceneActive.load() && NowMs() - sceneController.State().ppa.lastUpdateMs.load() < 3000;
    return recentPPAUpdate || (sceneController.State().sexLab.valid.load() && sceneController.State().sexLab.active.load());
}

std::int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void ScheduleExternalOwnerRepair(int delayMs, std::string_view reason) {
    if (!playerContext.physics.known.load()) return;
    const auto due = NowMs() + std::clamp(delayMs, 100, 5000);
    externalOwnerRepairDueMs.store(due);
    externalOwnerRepairUntilMs.store(due + 10000);
    ++externalResetNotices;
    logger::info("Physics owner re-check scheduled after {}", reason);
}

std::optional<SPS::APIControl::Request> ActiveAPIRequest() {
    return apiRegistry.Active(NowMs());
}

void ClearAPIRequests() {
    const auto cleared = apiRegistry.Clear();
    if (cleared > 0) {
        Record(fmt::format("Compatibility API released {} request(s) for the new game session", cleared));
    }
}

SPS::API::ControlSource CurrentAPIControlSource() {
    if (ActiveAPIRequest()) return SPS::API::ControlSource::ExternalAPI;
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (AnySceneHasPriority(copy)) return SPS::API::ControlSource::Scene;
    if (playerContext.physics.known.load()) return SPS::API::ControlSource::SPS;
    return SPS::API::ControlSource::None;
}

void QueueAPIEvaluation(bool force) {
    if (auto* tasks = SKSE::GetTaskInterface()) {
        tasks->AddTask([force] {
            if (force) {
                const auto request = ActiveAPIRequest();
                const bool alreadyApplied = request && playerContext.physics.known.load() &&
                    playerContext.physics.usingCBPC.load() == (request->state == SPS::API::PhysicsState::CBPC);
                if (alreadyApplied) {
                    Evaluate(false);
                    return;
                }
            }
            Evaluate(force);
        });
    }
}

void NotifyAPIStateChanged(SPS::API::PhysicsState previousState, SPS::API::PhysicsState currentState) {
    const auto listeners = apiRegistry.Listeners();
    if (listeners.empty()) return;
    const auto request = ActiveAPIRequest();
    SPS::API::StateChangedEvent event;
    event.previousState = previousState;
    event.currentState = currentState;
    event.source = request ? SPS::API::ControlSource::ExternalAPI : CurrentAPIControlSource();
    event.activeRequest = request ? request->handle : 0;
    for (const auto& listener : listeners) {
        if (!listener.callback) continue;
        try {
            listener.callback(&event, listener.userData);
        } catch (...) {
            Record("SPS-API-001: A compatibility API state listener threw an exception", true);
        }
    }
}

std::uint32_t __cdecl APIGetCapabilities() {
    using SPS::API::Capability;
    return static_cast<std::uint32_t>(Capability::Player) |
        static_cast<std::uint32_t>(Capability::TimedRequests) |
        static_cast<std::uint32_t>(Capability::StateListeners) |
        static_cast<std::uint32_t>(Capability::ActorAwareABI) |
        static_cast<std::uint32_t>(Capability::ResetNotification);
}

bool __cdecl APIIsActorSupported(std::uint32_t actorFormID) {
    return actorFormID == SPS::API::kPlayerFormID;
}

SPS::API::Result __cdecl APIGetState(std::uint32_t actorFormID, SPS::API::StateSnapshot* snapshot) {
    if (!snapshot || snapshot->apiVersion != SPS::API::kVersion ||
        snapshot->size < sizeof(SPS::API::StateSnapshot))
        return SPS::API::Result::InvalidArgument;
    if (!APIIsActorSupported(actorFormID)) return SPS::API::Result::UnsupportedActor;
    const auto request = ActiveAPIRequest();
    snapshot->actorFormID = actorFormID;
    snapshot->state = playerContext.physics.known.load() ?
        (playerContext.physics.usingCBPC.load() ? SPS::API::PhysicsState::CBPC : SPS::API::PhysicsState::SMP) :
        SPS::API::PhysicsState::Unknown;
    snapshot->source = request ? SPS::API::ControlSource::ExternalAPI : CurrentAPIControlSource();
    snapshot->arousal = arousalController.Valid() ? arousalController.Value() : 0.0F;
    snapshot->activeRequest = request ? request->handle : 0;
    snapshot->activeRequestCount = static_cast<std::uint32_t>(apiRegistry.Count());
    return playerContext.physics.known.load() ? SPS::API::Result::Success : SPS::API::Result::NotReady;
}

SPS::API::RequestHandle __cdecl APIRequestPhysics(const SPS::API::PhysicsRequest* request) {
    if (!request || request->apiVersion != SPS::API::kVersion ||
        request->size < sizeof(SPS::API::PhysicsRequest) ||
        !APIIsActorSupported(request->actorFormID) ||
        (request->state != SPS::API::PhysicsState::SMP && request->state != SPS::API::PhysicsState::CBPC))
        return 0;
    {
        std::scoped_lock lock(settingsLock);
        if (!settings.enabled) return 0;
    }

    const auto stored = apiRegistry.Add(*request, NowMs());
    Record(fmt::format("Compatibility API: {} requested {} for the player{}",
        stored.requester,
        stored.state == SPS::API::PhysicsState::CBPC ? "CBPC" : "SMP",
        request->durationMilliseconds > 0 ? fmt::format(" for {} ms", request->durationMilliseconds) : " until released"));
    QueueAPIEvaluation(true);
    return stored.handle;
}

SPS::API::Result __cdecl APIReleasePhysics(SPS::API::RequestHandle request) {
    if (request == 0) return SPS::API::Result::InvalidArgument;
    const auto requester = apiRegistry.Release(request);
    if (!requester) return SPS::API::Result::NotFound;
    Record(fmt::format("Compatibility API: {} released player physics control", *requester));
    QueueAPIEvaluation(false);
    return SPS::API::Result::Success;
}

SPS::API::Result __cdecl APINotifyPhysicsReset(std::uint32_t actorFormID) {
    if (!APIIsActorSupported(actorFormID)) return SPS::API::Result::UnsupportedActor;
    {
        std::scoped_lock lock(settingsLock);
        if (!settings.enabled) return SPS::API::Result::NotReady;
    }
    if (!playerContext.physics.known.load()) return SPS::API::Result::NotReady;
    ScheduleExternalOwnerRepair(750, "a compatibility API reset notification");
    return SPS::API::Result::Success;
}

SPS::API::ListenerHandle __cdecl APIRegisterStateListener(
    SPS::API::StateChangedCallback callback, void* userData) {
    return apiRegistry.RegisterListener(callback, userData);
}

SPS::API::Result __cdecl APIUnregisterStateListener(SPS::API::ListenerHandle listener) {
    if (listener == 0) return SPS::API::Result::InvalidArgument;
    return apiRegistry.UnregisterListener(listener) ?
        SPS::API::Result::Success : SPS::API::Result::NotFound;
}

void ResetPPASceneTracking(int ignoreMs = 0) {
    sceneController.ResetPPA(ignoreMs);
}

void __cdecl OnPPAUpdate(const SPS::PPA::AnimationUpdateEvent* event, void*) {
    if (!event || event->apiVersion != SPS::PPA::kVersion ||
        event->size < sizeof(SPS::PPA::AnimationUpdateEvent)) return;
    if (event->actorCount > 0 && !event->actors) return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    const auto now = NowMs();
    // PPA can publish the tail of its previous update cycle after a load or
    // framework cleanup. Do not let that stale frame reclaim position.
    if (!event->ending && now < sceneController.State().ppa.ignoreUntilMs.load()) return;

    const auto receiver = event->receiver.get();
    const bool playerIsReceiver = receiver && receiver.get() == player;
    bool playerIsPartner = false;
    bool receiverHasPartner = false;
    for (std::uint32_t i = 0; i < event->actorCount; ++i) {
        if (playerIsReceiver && event->actors[i].actor) receiverHasPartner = true;
        const auto partner = event->actors[i].actor.get();
        if (partner && partner.get() == player) {
            playerIsPartner = true;
            break;
        }
    }
    const bool playerSelfInteraction = playerIsReceiver && event->selfInteraction;
    // A top actor can also receive an empty per-receiver update. Ignore that
    // record so it cannot overwrite the meaningful partner update as bottom.
    if (!playerIsPartner && !receiverHasPartner && !playerSelfInteraction) return;

    const int observedRole = playerIsPartner || playerSelfInteraction ? 2 : 1;
    bool changed = false;
    if (event->ending) {
        // PPA sends cleanup per receiver. A bottom-side cleanup must not clear
        // a still-active penetrating role (and vice versa) in multi-actor scenes.
        if (sceneController.State().ppa.sceneRoleValid.load() && sceneController.State().ppa.sceneRole.load() != observedRole) return;
        sceneController.State().ppa.bottomCandidateSinceMs.store(0);
        sceneController.State().ppa.lastUpdateMs.store(now);
        changed = sceneController.State().ppa.sceneActive.exchange(false) || sceneController.State().ppa.sceneRoleValid.load();
        sceneController.State().ppa.sceneRole.store(0);
        sceneController.State().ppa.sceneRoleValid.store(false);
    } else {
        if (observedRole == 2) {
            // One PPA update pass can describe the player as both a receiver
            // and a penetrating partner. Penetrating wins, and suppresses the
            // companion receiver record so SPS cannot alternate every frame.
            sceneController.State().ppa.lastTopMs.store(now);
            sceneController.State().ppa.bottomCandidateSinceMs.store(0);
        } else {
            if (now - sceneController.State().ppa.lastTopMs.load() < 500) return;
            const auto candidateSince = sceneController.State().ppa.bottomCandidateSinceMs.load();
            if (candidateSince == 0) {
                sceneController.State().ppa.bottomCandidateSinceMs.store(now);
                return;
            }
            // Require a short run of receiver-only updates before switching to
            // SMP. This filters paired PPA records without delaying real scenes.
            if (now - candidateSince < 100) return;
        }

        sceneController.State().ppa.lastUpdateMs.store(now);
        const int previous = sceneController.State().ppa.sceneRole.exchange(observedRole);
        const bool wasValid = sceneController.State().ppa.sceneRoleValid.exchange(true);
        sceneController.State().ppa.sceneActive.store(true);
        changed = !wasValid || previous != observedRole;
        if (changed)
            Record(fmt::format("PPA role update: {}", SexLabRoleName(observedRole)));
    }
    // PPA can publish on every animation update. Only schedule work when the
    // effective role changes, avoiding a per-frame SKSE/Papyrus task flood.
    if (changed)
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { Evaluate(); });
}

void RegisterPPAAPI() {
    const auto module = ::GetModuleHandleW(SPS::PPA::kPluginDLL);
    const auto getAPI = module ? reinterpret_cast<SPS::PPA::GetAPIFn>(
        ::GetProcAddress(module, SPS::PPA::kGetAPIFunctionNameV1)) : nullptr;
    const auto* api = getAPI ? getAPI() : nullptr;
    if (!api || api->version != SPS::PPA::kVersion || api->size < sizeof(SPS::PPA::InterfaceV1) ||
        !api->RegisterAnimationUpdateListener || !api->UnregisterAnimationUpdateListener) {
        sceneController.State().ppa.apiConnected.store(false);
        return;
    }
    ppaAPI = api;
    ppaListener = ppaAPI->RegisterAnimationUpdateListener(OnPPAUpdate, nullptr);
    sceneController.State().ppa.apiConnected.store(ppaListener != 0);
    if (ppaListener) Record("PPA V1 listener connected; live scene hand-off enabled");
}

const char* OStimRoleName(int role) {
    return SexLabRoleName(role);
}

bool DebugCaptureActive() {
    return debugCaptureUntilMs.load() > NowMs();
}

void AppendCaptureLine(std::string_view type, const std::string& message) {
    if (!DebugCaptureActive()) return;
    const auto elapsed = std::max<std::int64_t>(0, NowMs() - debugCaptureStartedMs.load());
    std::scoped_lock lock(debugCaptureLock);
    debugCaptureLines.push_back(fmt::format("[+{:.1f}s] {}: {}", elapsed / 1000.0, type, message));
}

void Record(std::string message, bool error) {
    if (error) logger::error("{}", message);
    else logger::info("{}", message);
    AppendCaptureLine(error ? "ERROR" : "EVENT", message);
    activityLog.Record(std::move(message), error);
}

void ClearArousalError() {
    activityLog.ClearErrorWithPrefixes({ "SPS-002:" });
}

void ConfigureControllers() {
    arousalController.Configure(
        [] { return PapyrusReadyForDispatch(); },
        [](std::string message, bool error) { Record(std::move(message), error); },
        [] { ClearArousalError(); },
        [] { Evaluate(); });
    sceneController.Configure(
        [] { return PapyrusReadyForDispatch(); },
        [](std::string message, bool error) { Record(std::move(message), error); },
        [] { Evaluate(); },
        [] {
            return SPS::Controllers::OwnerSnapshot{
                playerContext.physics.known.load(),
                playerContext.physics.usingCBPC.load()
            };
        },
        [](bool previous, bool active, std::int64_t now) {
            if (previous && !active && playerContext.physics.known.load() &&
                !playerContext.physics.usingCBPC.load()) {
                softConfirmationDueMs.store(now + 250);
            }
            if (previous != active && playerContext.physics.known.load() &&
                playerContext.physics.usingCBPC.load()) {
                CancelErectionAnimation();
                Settings copy;
                { std::scoped_lock lock(settingsLock); copy = settings; }
                if ((copy.useSexLabBend || PPAOwnsPosition() ||
                    ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr) &&
                    copy.positionControl) {
                    playerContext.position.appliedBend.store(-1);
                    bendSettleDueMs.store(now + copy.settleDelayMs);
                }
            }
        });
}

void ClearResolvedOwnerError() {
    activityLog.ClearErrorWithPrefixes({ "SPS-010:", "SPS-020:" });
}

void ClearResolvedRefreshError() {
    activityLog.ClearErrorWithPrefixes({ "SPS-017:", "SPS-018:", "SPS-019:" });
}

bool CoreReady(const Diagnostics& d, const Settings& copy) {
    const bool arousalReady = copy.mode != 0 || (d.oslModuleLoaded && d.oslPluginLoaded);
    return d.menuFrameworkLoaded && arousalReady && d.fsmpModuleLoaded && d.fsmpActorApiAvailable && d.fsmpBridgePresent && d.cbpcModuleLoaded &&
        d.playerBonesFound == 6 && d.compatibleXmlFiles > 0 && d.compatibleCbpcMaps > 0 &&
        d.compatibleCbpcParameters > 0 && !d.sosPhysicsManagerLoaded;
}

void CaptureState(std::string_view reason) {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const auto line = fmt::format(
        "{} | engine={} arousal={:.1f}/{} provider={} connected={} SexLab={} SexLabRole={} OStim={} OStimRole={} SMP={} CBPC={} bend={}/{} method={} animating={} guard={} suspended={}",
        reason, playerContext.physics.known.load() ? (playerContext.physics.usingCBPC.load() ? "CBPC" : "SMP") : "unknown",
        arousalController.Value(), arousalController.Valid(), ArousalProviderName(), arousalController.Connected(), sceneController.State().sexLab.active.load(),
        SexLabRoleName(sceneController.State().sexLab.role.load()), sceneController.State().ostim.active.load(), OStimRoleName(sceneController.State().ostim.role.load()),
        playerContext.physics.smpConnected.load(), playerContext.physics.cbpcConnected.load(), playerContext.position.requestedBend.load(), playerContext.position.appliedBend.load(),
        playerContext.position.lastMethod.load(), erectionAnimating.load(), NowMs() < bendGuardUntilMs.load(),
        playerContext.position.automaticSuspended.load());
    if (copy.verboseLogging || DebugCaptureActive()) logger::debug("{}", line);
    AppendCaptureLine("STATE", line);
}

std::vector<std::pair<std::string, std::string>> SuggestedFixes(const Diagnostics& d) {
    std::vector<std::pair<std::string, std::string>> fixes;
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!d.menuFrameworkLoaded)
        fixes.emplace_back("SPS-001", "Install or update SKSE Menu Framework 3, then fully restart Skyrim.");
    if (copy.mode == 0 && (!d.oslModuleLoaded || !d.oslPluginLoaded))
        fixes.emplace_back("SPS-002", "Install OSL Aroused, SLO Aroused NG, or classic SexLab Aroused and enable its plugin.");
    if (!d.fsmpModuleLoaded)
        fixes.emplace_back("SPS-003", "Install Faster HDT-SMP and its requirements.");
    else if (!d.fsmpActorApiAvailable)
        fixes.emplace_back("SPS-003", "Update Faster HDT-SMP to 4.0.1 or newer. FSMP 4.1.1 or newer is recommended.");
    else if (!d.fsmpBridgePresent)
        fixes.emplace_back("SPS-021", "The SPS FSMP bridge is missing. Reinstall SPS and let it replace the previous version.");
    if (!d.cbpcModuleLoaded)
        fixes.emplace_back("SPS-004", "Install or update CBPC, then fully restart Skyrim.");
    if (d.playerBonesFound != 6)
        fixes.emplace_back("SPS-005", fmt::format("Only {}/6 required physics bones were found. Rebuild or reinstall the compatible schlong addon.", d.playerBonesFound));
    if (d.compatibleXmlFiles == 0)
        fixes.emplace_back("SPS-006", "No compatible SMP setup was found. Reinstall the schlong's SMP files and check which mod wins conflicts.");
    if (d.compatibleCbpcMaps == 0)
        fixes.emplace_back("SPS-007", "SPS's CBPC bone file is missing or overwritten. Let SPS win this file conflict.");
    if (d.compatibleCbpcParameters == 0)
        fixes.emplace_back("SPS-008", "SPS's CBPC movement file is missing or overwritten. Reinstall SPS and let it win the conflict.");
    if (!PositionBackendAvailable())
        fixes.emplace_back("SPS-009", "No supported position backend was found. Install SOS AE-NG, legacy SOS, or The New Gentleman.");
    if (d.sexLabModuleLoaded && d.sexLabPluginLoaded && !d.sexLabRoleBridgePresent)
        fixes.emplace_back("SPS-013", "The SPS SexLab role bridge is missing. Reinstall SPS 1.9 so bottom/top scene switching can work.");
    if (d.ostimPluginLoaded && !d.ostimRoleBridgePresent)
        fixes.emplace_back("SPS-015", "OStim is installed but the optional SPS OStim bridge is missing. Reinstall SPS and select OStim support if you want SPS to follow OStim roles.");
    if (d.sosPhysicsManagerLoaded)
        fixes.emplace_back("SPS-016", "SOS Physics Manager is enabled and can fight SPS for control. Disable SOSPhysicsManager.esp while using SPS.");
    if (d.physicsEditorLoaded)
        fixes.emplace_back("SPS-014", "Physics Editor is loaded. It can stay installed, but disable its schlong controls if SPS changes unexpectedly.");
    if (playerContext.physics.failures.load() > 0)
        fixes.emplace_back("SPS-010", "A physics handoff failed. Check SchlongPhysicsSwapper.log and confirm both FSMP and CBPC load correctly.");
    if (PositionBackendAvailable() &&
        (playerContext.position.automaticSuspended.load() || (!playerContext.position.lastSucceeded.load() && playerContext.position.requestedBend.load() >= 0)))
        fixes.emplace_back("SPS-011", "The erect angle could not be applied. Use Repair current physics, then check for another mod controlling the angle.");
    if (fixes.empty())
        fixes.emplace_back("SPS-000", "Everything appears ready.");
    return fixes;
}

auto VM() { return SPS::Runtime::VM(); }

bool PapyrusReadyForDispatch() {
    return SPS::Runtime::PapyrusReady(papyrusDispatchAllowedAfterMs.load(), NowMs());
}

void InvalidatePapyrusQueries(int delayMs) {
    papyrusDispatchAllowedAfterMs.store(NowMs() + std::max(delayMs, 0));
    arousalController.Invalidate();
    sceneController.InvalidateQueries();
}

template <class... Args>
bool Call(const char* script, const char* function, Args... values) {
    return SPS::Runtime::DispatchStatic(
        script, function, papyrusDispatchAllowedAfterMs.load(), NowMs(),
        std::move(values)...);
}

bool ResetSMPPhysics(RE::Actor* actor, bool full) {
    return SPS::Runtime::ResetPlayerPhysics(
        actor, full, FsmpActorApiAvailable(),
        papyrusDispatchAllowedAfterMs.load(), NowMs());
}

bool SetPlayerPhysicsOwner(RE::Actor* actor, bool useCBPC) {
    return SPS::Runtime::SetPlayerPhysicsOwner(
        actor, useCBPC, FsmpActorApiAvailable(),
        papyrusDispatchAllowedAfterMs.load(), NowMs());
}

bool ReleasePlayerPhysics(RE::Actor* actor) {
    return SPS::Runtime::ReleasePlayerPhysics(
        actor, FsmpActorApiAvailable(),
        papyrusDispatchAllowedAfterMs.load(), NowMs());
}

bool CallSosAeBend(RE::Actor* actor, int bend) {
    return SPS::Runtime::SetNativeBend(
        actor, bend, papyrusDispatchAllowedAfterMs.load(), NowMs());
}

bool ConfirmCurrentPhysicsOwner(std::string_view reason) {
    if (!PapyrusReadyForDispatch()) return false;
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.enabled || !playerContext.physics.known.load()) return false;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;

    auto* actor = static_cast<RE::Actor*>(player);
    const bool expectCBPC = playerContext.physics.usingCBPC.load();
    const bool handoffQueued = SetPlayerPhysicsOwner(actor, expectCBPC);
    playerContext.physics.smpConnected.store(handoffQueued);
    playerContext.physics.cbpcConnected.store(handoffQueued);
    if (!handoffQueued) {
        logger::warn("Could not queue {} restoration after {}",
            expectCBPC ? "CBPC" : "SMP", reason);
        return false;
    }

    const auto now = NowMs();
    lastOwnerRestorationMs.store(now);
    ignoreNodeEventsUntilMs.store(now + 2000);
    ++ownerRestorations;
    ClearResolvedOwnerError();
    Record(fmt::format("{} ownership restored after {}",
        expectCBPC ? "CBPC" : "SMP", reason));
    return true;
}

void Load() {
    const auto result = SPS::Core::LoadSettings(kIni, kLegacyIni);
    {
        std::scoped_lock lock(settingsLock);
        settings = result.settings;
    }
    spdlog::set_level(settings.verboseLogging ? spdlog::level::debug : spdlog::level::info);
    if (result.shouldWriteCurrent) {
        Save();
    }
    if (result.migratedLegacy) {
        Record("Existing UBE Physics Switch settings migrated to Schlong Physics Swapper");
    }
}

void Save() {
    Settings copy;
    {
        std::scoped_lock lock(settingsLock);
        copy = settings;
    }
    if (!SPS::Core::SaveSettings(kIni, copy)) {
        logger::error("Failed to save SPS settings to {}", kIni);
    }
}

void QueryArousal(bool force = false) {
    arousalController.Query(force);
}

void QuerySexLab() {
    sceneController.QuerySexLab();
}

bool RandomErectionActive() {
    const auto until = randomErectionUntilMs.load();
    return until > 0 && NowMs() < until;
}

void StartRefractoryPeriod(const Settings& copy, std::int64_t now = 0) {
    if (!copy.spontaneousRefractory) {
        spontaneousRefractoryUntilMs.store(0);
        return;
    }
    if (now == 0) now = NowMs();
    spontaneousRefractoryUntilMs.store(now +
        static_cast<std::int64_t>(std::clamp(copy.refractoryMinutes, 1, 60)) * 60000);
}

bool RandomErectionActivityBlocked() {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || player->IsDead(false) || player->IsInCombat() ||
        player->IsInKillMove() || player->IsOnMount() || player->IsSwimming())
        return true;

    auto* ui = RE::UI::GetSingleton();
    return ui && (ui->GameIsPaused() ||
        ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
        ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME));
}

void ScheduleNextRandomErection(const Settings& copy, std::int64_t now = 0) {
    if (!copy.randomErections || copy.mode != 0) {
        randomErectionNextMs.store(0);
        return;
    }
    const int minimum = std::clamp(copy.randomErectionMinMinutes, 1, 240);
    const int maximum = std::clamp(std::max(minimum, copy.randomErectionMaxMinutes), minimum, 240);
    std::uniform_int_distribution<int> delayMinutes(minimum, maximum);
    int chosenMinutes;
    {
        std::scoped_lock lock(randomLock);
        chosenMinutes = delayMinutes(randomEngine);
    }
    if (now == 0) now = NowMs();
    const auto chosenTime = now + static_cast<std::int64_t>(chosenMinutes) * 60000;
    randomErectionNextMs.store(std::max(chosenTime, spontaneousRefractoryUntilMs.load()));
    logger::info("Next random erection check scheduled in {} minutes", chosenMinutes);
}

void UpdateRandomErection(const Settings& copy) {
    const auto now = NowMs();
    const bool spontaneousEnabled = copy.randomErections || copy.morningErections;
    const bool blocked = !spontaneousEnabled || copy.mode != 0 ||
        AnySceneHasPriority(copy) || ActiveAPIRequest().has_value();
    if (blocked) {
        const bool wasActive = randomErectionUntilMs.exchange(0) > now;
        if (wasActive) {
            StartRefractoryPeriod(copy, now);
            Record("Spontaneous erection stopped because another SPS control took priority");
        }
        if (!copy.randomErections) randomErectionNextMs.store(0);
        morningErectionActive.store(false);
        randomErectionManualTest.store(false);
        return;
    }

    if ((copy.randomErectionSafeMoments || morningErectionActive.load()) &&
        !randomErectionManualTest.load() &&
        RandomErectionActivityBlocked()) {
        const bool wasActive = randomErectionUntilMs.exchange(0) > now;
        if (wasActive) StartRefractoryPeriod(copy, now);
        morningErectionActive.store(false);
        const auto retryAt = now + 60000;
        const auto next = randomErectionNextMs.load();
        if (copy.randomErections && (next == 0 || next < retryAt))
            randomErectionNextMs.store(retryAt);
        if (wasActive)
            Record("Spontaneous erection ended because gameplay was interrupted");
        return;
    }

    // Keep CBPC in control until the downward movement has completed. Without
    // this hold, the normal arousal decision immediately hands the bones back
    // to SMP and the random erection visibly snaps down.
    if (erectionRelaxing.load()) return;

    const auto until = randomErectionUntilMs.load();
    if (until > 0) {
        if (now < until) return;
        randomErectionUntilMs.store(0);
        randomErectionManualTest.store(false);
        morningErectionActive.store(false);
        StartRefractoryPeriod(copy, now);
        if (copy.randomErections) ScheduleNextRandomErection(copy, now);
        if (copy.gradualErection && copy.positionControl && playerContext.physics.known.load() &&
            playerContext.physics.usingCBPC.load() && !AnySceneHasPriority(copy) && !PPAOwnsPosition()) {
            StartGradualRelaxation(copy);
        } else {
            Record("Spontaneous erection finished; normal arousal control resumed");
        }
        return;
    }

    const auto morningDue = morningErectionDueMs.load();
    if (copy.morningErections && morningDue > 0 && now >= morningDue &&
        now >= spontaneousRefractoryUntilMs.load()) {
        morningErectionDueMs.store(0);
        if (playerContext.physics.known.load() && !playerContext.physics.usingCBPC.load()) {
            morningErectionActive.store(true);
            randomErectionUntilMs.store(now +
                static_cast<std::int64_t>(copy.morningErectionDurationSeconds) * 1000);
            Record("A morning erection started after resting");
            return;
        }
    }

    const auto next = randomErectionNextMs.load();
    if (!copy.randomErections) return;
    if (next == 0) {
        ScheduleNextRandomErection(copy, now);
        return;
    }
    if (now < next || now < spontaneousRefractoryUntilMs.load()) return;

    // Do not waste a random event while the player is already erect. Pick a
    // fresh interval and wait for a future soft state instead.
    if (!playerContext.physics.known.load() || playerContext.physics.usingCBPC.load()) {
        ScheduleNextRandomErection(copy, now);
        return;
    }

    randomErectionNextMs.store(0);
    randomErectionManualTest.store(false);
    randomErectionUntilMs.store(now +
        static_cast<std::int64_t>(std::clamp(copy.randomErectionDurationSeconds, 5, 600)) * 1000);
    Record("A random erection started");
}

int DesiredBend(const Settings& copy) {
    if (copy.useSexLabBend && AnySceneHasPriority(copy)) return copy.sexLabBend;
    if (!copy.arousalBasedErection || copy.mode != 0 || !arousalController.Valid() ||
        AnySceneHasPriority(copy) || RandomErectionActive())
        return copy.erectBend;

    const float start = std::clamp(copy.erectionStartArousal, 0.0F, 99.0F);
    const float fullyErect = std::max(start + 1.0F, copy.threshold);
    const float progress = std::clamp((arousalController.Value() - start) / (fullyErect - start), 0.0F, 1.0F);
    const int softBend = copy.flaccidAngleControl && SosAeNativeLoaded() ? copy.flaccidBend : 0;
    return std::clamp(static_cast<int>(std::lround(
        softBend + (copy.erectBend - softBend) * progress)), 0, 20);
}

const char* BendMethodName(int method) {
    switch (method) {
    case 0: return "SOS AE native";
    case 1: return "SOS animation event";
    case 2: return "Compatibility (event + native)";
    default: return "Not applied";
    }
}

void ResetPositionRecovery() {
    playerContext.position.automaticSuspended.store(false);
    bendConsecutiveFailures.store(0);
    bendFailureReported.store(false);
    bendRetryDueMs.store(0);
    bendGuardCount.store(0);
    bendGuardUntilMs.store(0);
    bendGuardWindowStartMs.store(0);
    ClearResolvedPositionError();
}

bool AutomaticBendAllowed(const Settings& copy) {
    const auto now = NowMs();
    if (playerContext.position.automaticSuspended.load()) {
        if (now < bendGuardUntilMs.load()) return false;
        playerContext.position.automaticSuspended.store(false);
    }
    if (!copy.bounceGuard) return true;
    if (now < bendGuardUntilMs.load()) return false;
    auto windowStart = bendGuardWindowStartMs.load();
    if (windowStart == 0 || now - windowStart > 3000) {
        bendGuardWindowStartMs.store(now);
        bendGuardCount.store(0);
    }
    if (bendGuardCount.fetch_add(1) + 1 > 3) {
        bendGuardUntilMs.store(now + 5000);
        bendGuardCount.store(0);
        Record("Bounce guard paused automatic position repairs for 5 seconds");
        return false;
    }
    return true;
}

int AnimationEventBend(int bend, bool tngBackend) {
    return SPS::Runtime::AnimationEventBend(bend, tngBackend);
}

bool SendPositionEvent(RE::Actor* actor, const RE::BSFixedString& eventName, bool tngBackend) {
    return SPS::Runtime::SendPositionEvent(
        actor, eventName, tngBackend,
        papyrusDispatchAllowedAfterMs.load(), NowMs());
}

bool ApplyBend(RE::Actor* actor, int bend, bool flaccid = false, bool animate = false, bool automatic = false) {
    bend = std::clamp(bend, 0, 20);
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    playerContext.position.requestedBend.store(bend);
    if (!copy.positionControl) {
        return true;
    }
    if (automatic && !AutomaticBendAllowed(copy)) {
        return false;
    }

    const auto dispatch = SPS::Runtime::ApplyPosition(
        actor, bend, flaccid, animate, copy.bendMethod, copy.animatePosition,
        papyrusDispatchAllowedAfterMs.load(), NowMs());
    if (dispatch.method < 0) {
        return true;
    }
    playerContext.position.lastMethod.store(dispatch.method);
    if (dispatch.nativeAccepted) {
        playerContext.physics.sosConnected.store(true);
    }
    if (dispatch.accepted) {
        playerContext.position.appliedBend.store(bend);
        const auto now = NowMs();
        lastBendApplyMs.store(now);
        // SOS bend updates can emit a NiNode update. Ignore that echo so it
        // cannot be mistaken for a rebuilt skeleton and start a repair loop.
        ignoreNodeEventsUntilMs.store(now + 2000);
        playerContext.position.lastSucceeded.store(true);
        const int previousFailures = bendConsecutiveFailures.exchange(0);
        const bool failureWasReported = bendFailureReported.exchange(false);
        bendRetryDueMs.store(0);
        playerContext.position.automaticSuspended.store(false);
        const bool clearedError = ClearResolvedPositionError();
        if (previousFailures > 0 || failureWasReported || clearedError)
            Record("SOS angle control recovered");
    } else {
        playerContext.position.lastSucceeded.store(false);
        const auto now = NowMs();
        bendRetryDueMs.store(now + 1500);
        if (automatic) {
            const int failures = bendConsecutiveFailures.fetch_add(1) + 1;
            if (failures >= copy.maxBendFailures) {
                bendConsecutiveFailures.store(0);
                bendGuardUntilMs.store(now + 5000);
                playerContext.position.automaticSuspended.store(true);
                bendRetryDueMs.store(now + 5000);
                if (!bendFailureReported.exchange(true))
                    Record("SPS-011: SOS is not ready for the angle yet; automatic retry paused for 5 seconds", true);
            }
        }
    }
    return dispatch.accepted;
}

void CancelErectionAnimation() {
    erectionAnimating.store(false);
    erectionAnimationGeneration.fetch_add(1);
    erectionRelaxing.store(false);
}

void StartBendAnimation(int startBend, int targetBend, int durationMs, bool relaxing) {
    startBend = std::clamp(startBend, 0, 20);
    targetBend = std::clamp(targetBend, 0, 20);
    durationMs = std::clamp(durationMs, 500, 15000);
    CancelErectionAnimation();
    erectionRelaxing.store(relaxing);
    const auto generation = erectionAnimationGeneration.load();
    erectionAnimationTargetBend.store(targetBend);
    erectionAnimationLastQueuedBend.store(-1);
    erectionAnimationStartMs.store(NowMs());
    erectionAnimating.store(true);
    playerContext.position.requestedBend.store(targetBend);
    playerContext.position.appliedBend.store(-1);
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const bool nativeBackend = SosAeNativeLoaded();
    const bool tngBackend = TngLoaded();
    const bool useGraphEvents = (tngBackend || LegacySosLoaded()) &&
        (!nativeBackend || copy.bendMethod == 1);
    if (!nativeBackend && !useGraphEvents) {
        erectionAnimating.store(false);
        erectionRelaxing.store(false);
        playerContext.position.appliedBend.store(-1);
        playerContext.position.lastMethod.store(-1);
        playerContext.position.lastSucceeded.store(false);
        return;
    }

    erectionAnimationThread = std::jthread([generation, startBend, targetBend, durationMs, useGraphEvents, tngBackend, relaxing](std::stop_token token) {
        while (!token.stop_requested() && erectionAnimating.load() &&
            generation == erectionAnimationGeneration.load()) {
            const auto elapsed = std::max<std::int64_t>(0, NowMs() - erectionAnimationStartMs.load());
            const float t = std::clamp(static_cast<float>(elapsed) / static_cast<float>(durationMs), 0.0F, 1.0F);
            const float eased = t * t * (3.0F - 2.0F * t);
            const int bend = std::clamp(static_cast<int>(std::lround(
                startBend + (targetBend - startBend) * eased)), 0, 20);
            const int eventBend = AnimationEventBend(bend, tngBackend);
            const int queueKey = useGraphEvents ? eventBend : bend;
            const int previousQueueKey = erectionAnimationLastQueuedBend.exchange(queueKey);
            if (queueKey != previousQueueKey || t >= 1.0F) {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([generation, bend, eventBend, targetBend, useGraphEvents, tngBackend, relaxing] {
                        if (!erectionAnimating.load() || generation != erectionAnimationGeneration.load() ||
                            !playerContext.physics.known.load() || !playerContext.physics.usingCBPC.load()) return;
                        auto* player = RE::PlayerCharacter::GetSingleton();
                        if (!player) return;
                        const bool ok = useGraphEvents
                            ? SendPositionEvent(static_cast<RE::Actor*>(player),
                                RE::BSFixedString(fmt::format("SOSBend{}", eventBend)), tngBackend)
                            : CallSosAeBend(static_cast<RE::Actor*>(player), bend);
                        if (!ok) {
                            // Some SOS AE builds stop accepting native bend values just
                            // above their visible flaccid limit. During relaxation that
                            // means the animation has already gone as low as this backend
                            // can display. Finish the owner handoff instead of cancelling
                            // and restarting the same 5 -> 0 animation every poll.
                            if (relaxing) {
                                const bool flaccidEventAccepted =
                                    SendPositionEvent(static_cast<RE::Actor*>(player),
                                        RE::BSFixedString("SOSFlaccid"), tngBackend);
                                CancelErectionAnimation();
                                // Keep the transition marked as relaxing until
                                // SMP actually accepts ownership. Otherwise the
                                // ordinary CBPC repair pass can see CBPC still
                                // active for one Papyrus tick and replay the
                                // erect angle immediately before the soft
                                // handoff.
                                erectionRelaxing.store(true);
                                playerContext.position.requestedBend.store(targetBend);
                                playerContext.position.appliedBend.store(targetBend);
                                playerContext.position.lastSucceeded.store(flaccidEventAccepted);
                                bendRetryDueMs.store(0);
                                bendConsecutiveFailures.store(0);
                                bendFailureReported.store(false);
                                if (flaccidEventAccepted) {
                                    playerContext.position.lastMethod.store(1);
                                    lastBendApplyMs.store(NowMs());
                                    ignoreNodeEventsUntilMs.store(NowMs() + 2000);
                                }
                                Record(flaccidEventAccepted
                                    ? "SOS accepted its flaccid event; normal soft physics resumed"
                                    : "SOS reached its lowest available angle; normal soft physics resumed");
                                SetOwner(false, true);
                                return;
                            }
                            CancelErectionAnimation();
                            playerContext.position.appliedBend.store(-1);
                            playerContext.position.lastSucceeded.store(false);
                            bendRetryDueMs.store(NowMs() + 1500);
                            Record("Gradual erection is waiting for SOS; the normal angle was queued instead");
                            return;
                        }
                        if (!useGraphEvents) playerContext.physics.sosConnected.store(true);
                        playerContext.position.appliedBend.store(bend);
                        playerContext.position.lastMethod.store(useGraphEvents ? 1 : 0);
                        playerContext.position.lastSucceeded.store(true);
                        bendRetryDueMs.store(0);
                        bendConsecutiveFailures.store(0);
                        bendFailureReported.store(false);
                        playerContext.position.automaticSuspended.store(false);
                        ClearResolvedPositionError();
                        lastBendApplyMs.store(NowMs());
                        ignoreNodeEventsUntilMs.store(NowMs() + 2000);
                        // Animation-event backends expose discrete visible SOS
                        // positions (signed -9..9 in TNG, 0..9 in legacy SOS).
                        // Finish the handoff as soon as the visible stage reaches
                        // the target instead of holding CBPC for the remainder
                        // of the internal 0-20 easing curve.
                        const int targetEventBend = AnimationEventBend(targetBend, tngBackend);
                        const bool reachedVisibleTarget = useGraphEvents ?
                            eventBend == targetEventBend : bend == targetBend;
                        if (reachedVisibleTarget) {
                            erectionAnimating.store(false);
                            playerContext.position.appliedBend.store(targetBend);
                            // SOS AE can rebuild or finish its graph transition
                            // just after the gradual native updates complete.
                            // Replay the final value once through compatibility
                            // mode so the visible angle cannot remain at zero.
                            if (!relaxing && SosAeNativeLoaded())
                                bendConfirmationDueMs.store(NowMs() + 500);
                            ++playerContext.position.repairs;
                            Record(relaxing
                                ? "Erection lowered gradually; normal soft physics resumed"
                                : fmt::format("Gradual erection completed: {}/20", targetBend));
                            if (relaxing) Evaluate(true);
                        }
                    });
                }
            }
            if (t >= 1.0F) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    Record(relaxing
        ? fmt::format("Erection is lowering gradually: {} to {}/20 over {:.1f} seconds",
            startBend, targetBend, durationMs / 1000.0F)
        : fmt::format("Gradual erection started: {} to {}/20 over {:.1f} seconds",
            startBend, targetBend, durationMs / 1000.0F));
}

void StartGradualErection(int targetBend, int durationMs) {
    StartBendAnimation(0, targetBend, durationMs, false);
}

void StartGradualRelaxation(const Settings& copy) {
    const int startBend = playerContext.position.appliedBend.load() >= 0 ? playerContext.position.appliedBend.load() : copy.erectBend;
    const int targetBend = copy.flaccidAngleControl && SosAeNativeLoaded() ? copy.flaccidBend : 0;
    if (erectionAnimating.load() && erectionRelaxing.load() &&
        erectionAnimationTargetBend.load() == targetBend) return;
    // From this point onward the requested state is soft. Any delayed erect
    // confirmation, retry or replacement-mesh replay belongs to the previous
    // state and must not be allowed to race the CBPC -> SMP handoff.
    bendSettleDueMs.store(0);
    bendConfirmationDueMs.store(0);
    bendRetryDueMs.store(0);
    erectMeshReplayDueMs.store(0);
    StartBendAnimation(startBend, targetBend, copy.softeningDurationMs, true);
}

void ApplyRequestedBend(bool force = false, bool animate = false, bool automatic = false) {
    if (!playerContext.physics.known.load() || !playerContext.physics.usingCBPC.load()) return;
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.positionControl) return;
    // PPA directly rotates the same genital chain during an active scene.
    // Let it own position then restore the user's SOS angle after the scene.
    if (PPAOwnsPosition()) return;
    const auto now = NowMs();
    const int desired = DesiredBend(copy);
    playerContext.position.requestedBend.store(desired);
    if (erectionAnimating.load()) return;
    if (!force && bendSettleDueMs.load() > now) return;
    // There is no read-back API for the live SOS bend. Once the requested value
    // was accepted, repeatedly sending it is not verification; it only restarts
    // SOS/CBPC movement and produces visible bouncing.
    if (!force && playerContext.position.appliedBend.load() == desired) return;
    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        const int previous = playerContext.position.appliedBend.load();
        const bool ok = ApplyBend(static_cast<RE::Actor*>(player), desired, false, animate, automatic);
        if (playerContext.position.lastMethod.load() < 0) return;
        if (ok) {
            ++playerContext.position.repairs;
            if (previous != desired)
                Record(fmt::format("Erect vertical bend applied: {}/20", desired));
        } else if (!automatic) {
            Record("SPS-011: SOS bend API did not accept the position update", true);
        }
    }
}

bool ClearResolvedPositionError() {
    return activityLog.ClearErrorWithPrefixes({ "SPS-009:", "SPS-011:" });
}

void ApplyRequestedSoftBend(bool force = false, bool animate = false) {
    if (!playerContext.physics.known.load() || playerContext.physics.usingCBPC.load()) return;
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.positionControl || PPAOwnsPosition()) return;

    // Only SOS AE exposes a safe native bend call that can be combined with
    // SOSFlaccid. Legacy SOS and TNG still receive their normal flaccid event.
    const bool customSoftAngle = copy.flaccidAngleControl && SosAeNativeLoaded();
    const int desired = customSoftAngle ? copy.flaccidBend : 0;
    playerContext.position.requestedBend.store(desired);
    if (!force && playerContext.position.appliedBend.load() == desired) return;

    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        const int previous = playerContext.position.appliedBend.load();
        const bool ok = ApplyBend(static_cast<RE::Actor*>(player), desired, true, animate, false);
        if (playerContext.position.lastMethod.load() < 0) return;
        if (ok && customSoftAngle && previous != desired)
            Record(fmt::format("Soft vertical bend applied: {}/20", desired));
    }
}

bool ConfirmSoftState() {
    if (!playerContext.physics.known.load() || playerContext.physics.usingCBPC.load()) return true;
    if (!PapyrusReadyForDispatch()) return false;
    // A live PPA scene owns these transforms. Retry instead of consuming the
    // only confirmation, otherwise the shaft can stay erect with SMP enabled.
    if (PPAOwnsPosition()) {
        return false;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;
    auto* actor = static_cast<RE::Actor*>(player);
    const bool handoffQueued = SetPlayerPhysicsOwner(actor, false);
    playerContext.physics.cbpcConnected.store(handoffQueued);
    playerContext.physics.smpConnected.store(handoffQueued);
    if (!handoffQueued) {
        logger::warn("Soft-state confirmation could not be queued");
        return false;
    }
    ApplyRequestedSoftBend(true, true);
    ClearResolvedOwnerError();
    Record("Soft state confirmed after physics handoff");
    return true;
}

bool ConfirmCBPCState() {
    if (!playerContext.physics.known.load() || !playerContext.physics.usingCBPC.load()) return true;
    if (!PapyrusReadyForDispatch()) return false;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;

    // Repeat both halves of the handoff on a later game tick. A successful
    // Papyrus dispatch only means the calls were queued; during a new game or
    // skeleton rebuild either physics engine can finish after the other one.
    // Reasserting SMP-off before CBPC-on makes the final owner deterministic.
    auto* actor = static_cast<RE::Actor*>(player);
    const bool handoffQueued = SetPlayerPhysicsOwner(actor, true);
    playerContext.physics.smpConnected.store(handoffQueued);
    playerContext.physics.cbpcConnected.store(handoffQueued);
    if (!handoffQueued) {
        logger::warn("Erect-state confirmation could not be queued");
        return false;
    }
    ClearResolvedOwnerError();
    Record("CBPC state confirmed after physics handoff");
    return true;
}

void RunLoadSMPReset() {
    if (!PapyrusReadyForDispatch()) {
        loadSMPResetDueMs.store(NowMs() + 1000);
        return;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        Record("SPS-017: Delayed SMP reset skipped because the player was unavailable", true);
        return;
    }
    if (::GetModuleHandleW(L"hdtsmp64.dll") == nullptr) {
        Record("SPS-017: Delayed SMP reset skipped because Faster HDT-SMP was not loaded", true);
        return;
    }

    // FSMP 4 exposes an actor-scoped native reset. This avoids opening the
    // console and does not reload every SMP actor in the current cell.
    const bool dispatched = ResetSMPPhysics(static_cast<RE::Actor*>(player), true);
    if (!dispatched) {
        Record("SPS-017: Faster HDT-SMP did not accept the delayed player reset", true);
        return;
    }

    const auto now = NowMs();
    playerContext.recovery.lastLoadSmpResetMs.store(now);
    ++playerContext.recovery.loadSmpResets;
    // ResetPhysics is executed by Papyrus and FSMP queues its mesh rebuild on
    // the game thread. Give it time to finish before restoring SPS ownership.
    loadSMPResetRestoreDueMs.store(now + 750);
    ClearResolvedRefreshError();
    Record("Player SMP reset completed after loading; physics state re-check queued");
}

void ScheduleLoadSMPReset() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    loadSMPResetRestoreDueMs.store(0);
    if (!copy.enabled || !copy.resetSMPAfterLoad || !FsmpActorApiAvailable()) {
        loadSMPResetDueMs.store(0);
        return;
    }
    loadSMPResetDueMs.store(NowMs() + copy.loadResetDelayMs);
    Record(fmt::format("Player SMP reset scheduled in {:.1f} seconds", copy.loadResetDelayMs / 1000.0F));
}

void ScheduleSoftAngleRefresh(int delayMs) {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    softAngleRefreshRestoreDueMs.store(0);
    if (!copy.enabled || !copy.positionControl || !SosAeNativeLoaded() ||
        !playerContext.physics.known.load() || playerContext.physics.usingCBPC.load() || PPAOwnsPosition()) {
        softAngleRefreshDueMs.store(0);
        return;
    }
    softAngleRefreshDueMs.store(NowMs() + std::clamp(delayMs, 0, 2000));
}

void RunSoftAngleRefresh() {
    if (!playerContext.physics.known.load() || playerContext.physics.usingCBPC.load() || PPAOwnsPosition()) return;
    if (!PapyrusReadyForDispatch()) {
        softAngleRefreshDueMs.store(NowMs() + 1000);
        return;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || ::GetModuleHandleW(L"hdtsmp64.dll") == nullptr) return;

    // FSMP keeps the old soft transform until its actor data is rebuilt. Apply
    // the requested value first, rebuild only the player, then apply it once
    // more after FSMP has settled.
    ApplyRequestedSoftBend(true, true);
    const bool dispatched = ResetSMPPhysics(static_cast<RE::Actor*>(player), true);
    if (!dispatched) {
        Record("SPS-018: Faster HDT-SMP did not accept the soft-angle refresh", true);
        return;
    }
    softAngleRefreshRestoreDueMs.store(NowMs() + 750);
    ClearResolvedRefreshError();
    Record("Soft angle changed; refreshing the player's SMP pose");
}

bool RefreshSMPAfterPlayerMeshChange() {
    if (!playerContext.physics.known.load() || playerContext.physics.usingCBPC.load() || PPAOwnsPosition()) return false;
    if (!PapyrusReadyForDispatch()) {
        nodeRefreshDueMs.store(NowMs() + 1000);
        return false;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || ::GetModuleHandleW(L"hdtsmp64.dll") == nullptr) return false;

    const auto now = NowMs();
    ignoreNodeEventsUntilMs.store(now + 3000);
    const bool dispatched = ResetSMPPhysics(static_cast<RE::Actor*>(player), true);
    if (!dispatched) return false;

    nodeSMPResetRestoreDueMs.store(now + 750);
    Record("Player mesh changed; refreshing its SMP physics");
    return true;
}

void RunSoftHandoffSMPReset() {
    if (!playerContext.physics.known.load() || playerContext.physics.usingCBPC.load() || PPAOwnsPosition()) {
        softHandoffResetDueMs.store(0);
        softHandoffResetUntilMs.store(0);
        softHandoffResetRestoreDueMs.store(0);
        return;
    }
    if (!PapyrusReadyForDispatch()) {
        const auto now = NowMs();
        if (now < softHandoffResetUntilMs.load())
            softHandoffResetDueMs.store(now + 1000);
        else
            Record("SPS-019: Soft-handoff refresh stopped because Papyrus did not become ready", true);
        return;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || ::GetModuleHandleW(L"hdtsmp64.dll") == nullptr) return;

    // TogglePhysics can report success while FSMP keeps the previous simulated
    // shape. Rebuilding just the player after a real CBPC -> SMP handoff is the
    // actor-scoped equivalent of the FSMP "SMP reset" button that repairs it.
    // ResetPhysics(..., true) already snaps the actor to the reference pose and
    // clears velocity. Freezing the six bones immediately before that reset can
    // make FSMP rebuild from the frozen transitional shape, leaving the soft
    // mesh visibly stretched even though the ownership calls all succeeded.
    const auto now = NowMs();
    ignoreNodeEventsUntilMs.store(now + 3000);
    auto* actor = static_cast<RE::Actor*>(player);
    const bool dispatched = ResetSMPPhysics(actor, true);
    if (!dispatched) {
        if (now < softHandoffResetUntilMs.load()) {
            softHandoffResetDueMs.store(now + 1000);
            logger::warn("Faster HDT-SMP did not accept the soft-handoff refresh; bounded retry queued");
        } else {
            Record("SPS-019: Faster HDT-SMP did not accept the soft-handoff refresh", true);
        }
        return;
    }

    softHandoffResetUntilMs.store(0);
    softConfirmationDueMs.store(0);
    softHandoffResetRestoreDueMs.store(now + 750);
    ClearResolvedRefreshError();
    Record("Soft physics handoff rebuilt the player's SMP pose");
}

bool SetOwner(bool cbpc, bool force) {
    const auto now = NowMs();
    if (!force && playerContext.physics.known.load() && playerContext.physics.usingCBPC.load() == cbpc) {
        // Mesh changes, API reset notices and the bounded post-switch check
        // schedule targeted repairs. Repeating an FSMP call on every ordinary
        // poll only adds Papyrus traffic without proving who owns the bones.
        return true;
    }
    if (!FsmpActorApiAvailable()) {
        playerContext.physics.smpConnected.store(false);
        playerContext.physics.known.store(false);
        playerContext.physics.retryAfterMs.store(now + 10000);
        return false;
    }
    if (!PapyrusReadyForDispatch()) {
        playerContext.physics.retryAfterMs.store(now + 1000);
        return false;
    }
    if (!force && now < playerContext.physics.retryAfterMs.load()) return false;

    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!force && playerContext.physics.known.load() && now - playerContext.physics.lastSwitchMs.load() < copy.switchCooldownMs) return false;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;
    const bool softTransitionPending = !cbpc && erectionRelaxing.load();
    const auto previousState = playerContext.physics.known.load() ?
        (playerContext.physics.usingCBPC.load() ? SPS::API::PhysicsState::CBPC : SPS::API::PhysicsState::SMP) :
        SPS::API::PhysicsState::Unknown;
    CancelErectionAnimation();
    auto* actor = static_cast<RE::Actor*>(player);

    const bool handoffQueued = SetPlayerPhysicsOwner(actor, cbpc);
    playerContext.physics.smpConnected.store(handoffQueued);
    playerContext.physics.cbpcConnected.store(handoffQueued);
    if (!handoffQueued) {
        // Best-effort rollback queues one ordered transaction for the previous
        // owner instead of another set of independently scheduled calls.
        if (playerContext.physics.known.load()) {
            SetPlayerPhysicsOwner(actor, playerContext.physics.usingCBPC.load());
        }
        ++playerContext.physics.failures;
        if (softTransitionPending)
            erectionRelaxing.store(true);
        playerContext.physics.retryAfterMs.store(now + 1000);
        Record(fmt::format("SPS-010: Ordered physics handoff to {} could not be queued; retry queued",
            cbpc ? "CBPC" : "SMP"), true);
        return false;
    }

    playerContext.physics.usingCBPC.store(cbpc);
    playerContext.physics.known.store(true);
    if (cbpc) {
        softHandoffResetDueMs.store(0);
        softHandoffResetUntilMs.store(0);
        softHandoffResetRestoreDueMs.store(0);
        softAngleRefreshDueMs.store(0);
        softAngleRefreshRestoreDueMs.store(0);
    }
    playerContext.physics.lastSwitchMs.store(now);
    ignoreNodeEventsUntilMs.store(now + 4000);
    playerContext.physics.retryAfterMs.store(0);
    ++playerContext.physics.successes;
    ClearResolvedOwnerError();
    ResetPositionRecovery();
    playerContext.position.appliedBend.store(-1);
    const bool realSoftHandoff = !cbpc && previousState == SPS::API::PhysicsState::CBPC;
    if (realSoftHandoff) {
        softConfirmationDueMs.store(0);
        softHandoffResetRestoreDueMs.store(0);
        // FSMP's Papyrus call can accept a full actor reset before the newly
        // enabled physics system has finished attaching to the mesh. That is
        // most visible after repeated high/low arousal stress tests: SPS says
        // soft/SMP, but the last erect simulated shape remains on screen until
        // the user runs FSMP's global reset. Keep the ownership switch
        // immediate, but let the safer player-only rebuild reach a stable mesh.
        softHandoffResetDueMs.store(now + 1500);
        softHandoffResetUntilMs.store(now + 10000);
    } else {
        softConfirmationDueMs.store(cbpc ? 0 : now + 750);
    }
    softConfirmationUntilMs.store(cbpc ? 0 : now + 15000);
    cbpcConfirmationDueMs.store(cbpc ? now + 750 : 0);
    cbpcConfirmationUntilMs.store(cbpc ? now + 15000 : 0);
    if (copy.positionControl) {
        if (cbpc) {
            playerContext.position.requestedBend.store(DesiredBend(copy));
            bendSettleDueMs.store(now + copy.settleDelayMs);
            const bool timedGradual = copy.gradualErection &&
                (!copy.arousalBasedErection || RandomErectionActive());
            bendConfirmationDueMs.store(timedGradual ? 0 : now + copy.settleDelayMs + 1500);
            if (copy.settleDelayMs == 0) {
                if (timedGradual && !AnySceneHasPriority(copy) && !PPAOwnsPosition())
                    StartGradualErection(DesiredBend(copy), copy.erectionDurationMs);
                else
                    ApplyRequestedBend(true, true, true);
            }
        } else {
            bendSettleDueMs.store(0);
            bendConfirmationDueMs.store(0);
            if (!PPAOwnsPosition()) ApplyRequestedSoftBend(true, true);
        }
    }
    // ResetPhysics is useful for rebuilding a soft SMP mesh after loading, but
    // running it after the first successful erect handoff gives FSMP the bones
    // back and creates a second, visibly late correction. An initially erect
    // state already has its own bounded CBPC confirmation and needs no reset.
    if (cbpc && previousState == SPS::API::PhysicsState::Unknown) {
        loadSMPResetDueMs.store(0);
        loadSMPResetRestoreDueMs.store(0);
    }
    Record(fmt::format("Physics switched to {} ({})", cbpc ? "CBPC" : "SMP", cbpc ? "erect" : "soft"));
    const auto currentState = cbpc ? SPS::API::PhysicsState::CBPC : SPS::API::PhysicsState::SMP;
    if (previousState != currentState)
        NotifyAPIStateChanged(previousState, currentState);
    return true;
}

bool SexLabHasPriority(const Settings& copy) {
    if (!copy.sexLabOverride || !sceneController.State().sexLab.valid.load()) return false;
    if (sceneController.State().sexLab.active.load()) return true;
    return sceneController.State().sexLab.endedMs.load() > 0 && NowMs() - sceneController.State().sexLab.endedMs.load() < copy.sceneEndDelayMs;
}

bool OStimHasPriority(const Settings& copy) {
    if (!copy.ostimOverride) return false;
    if (sceneController.State().ostim.active.load()) return true;
    return sceneController.State().ostim.endedMs.load() > 0 && NowMs() - sceneController.State().ostim.endedMs.load() < copy.sceneEndDelayMs;
}

bool AnySceneHasPriority(const Settings& copy) {
    return SexLabHasPriority(copy) || OStimHasPriority(copy);
}

bool NormalSettingsWantCBPC(const Settings& copy) {
    const SPS::Core::NormalDecisionState state{
        arousalController.Value(),
        arousalController.Valid(),
        playerContext.physics.known.load(),
        playerContext.physics.usingCBPC.load(),
        RandomErectionActive()
    };
    return SPS::Core::NormalSettingsWantCBPC(copy, state);
}

bool SexLabSceneWantsCBPC(const Settings& copy) {
    const bool recentPPAUpdate = sceneController.State().ppa.sceneRoleValid.load() &&
        NowMs() - sceneController.State().ppa.lastUpdateMs.load() < 3000;
    const SPS::Core::SexLabDecisionState state{
        {
            sceneController.State().sexLab.active.load(),
            playerContext.physics.usingCBPC.load(),
            sceneController.State().sexLab.entryStateValid.load(),
            sceneController.State().sexLab.entryCBPC.load(),
            sceneController.State().sexLab.roleValid.load(),
            static_cast<SPS::Core::SceneRole>(sceneController.State().sexLab.role.load()),
            {
                arousalController.Value(), arousalController.Valid(), playerContext.physics.known.load(),
                playerContext.physics.usingCBPC.load(), RandomErectionActive()
            }
        },
        recentPPAUpdate,
        static_cast<SPS::Core::SceneRole>(sceneController.State().ppa.sceneRole.load())
    };
    return SPS::Core::SexLabSceneWantsCBPC(copy, state);
}

bool OStimSceneWantsCBPC(const Settings& copy) {
    const SPS::Core::SceneDecisionState state{
        sceneController.State().ostim.active.load(),
        playerContext.physics.usingCBPC.load(),
        sceneController.State().ostim.entryStateValid.load(),
        sceneController.State().ostim.entryCBPC.load(),
        sceneController.State().ostim.roleValid.load(),
        static_cast<SPS::Core::SceneRole>(sceneController.State().ostim.role.load()),
        {
            arousalController.Value(), arousalController.Valid(), playerContext.physics.known.load(),
            playerContext.physics.usingCBPC.load(), RandomErectionActive()
        }
    };
    return SPS::Core::OStimSceneWantsCBPC(copy, state);
}

void QuerySexLabRole() {
    sceneController.QuerySexLabRole();
}

void QueryOStimRole() {
    sceneController.QueryOStimRole();
}

void Evaluate(bool force) {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.enabled) {
        if (force && playerContext.physics.known.load()) SetOwner(false, true);
        return;
    }

    if (const auto request = ActiveAPIRequest()) {
        if (erectionRelaxing.load()) {
            CancelErectionAnimation();
            playerContext.position.appliedBend.store(-1);
        }
        SetOwner(request->state == SPS::API::PhysicsState::CBPC, force);
        return;
    }

    bool cbpc = playerContext.physics.usingCBPC.load();
    bool normalControl = false;
    if (OStimHasPriority(copy)) {
        cbpc = OStimSceneWantsCBPC(copy);
    } else if (SexLabHasPriority(copy)) {
        cbpc = SexLabSceneWantsCBPC(copy);
    } else {
        normalControl = true;
        cbpc = NormalSettingsWantCBPC(copy);
    }

    // A manual troubleshooting test should remain visible long enough for the
    // user to inspect it. Scene requests still take control immediately.
    if (normalControl && !force && NowMs() < manualPhysicsTestUntilMs.load()) return;
    if (!normalControl) manualPhysicsTestUntilMs.store(0);

    // Scene integrations take priority over an everyday/spontaneous softening
    // animation. Cancel it here, where the new owner is known, instead of in
    // the random-erection scheduler. The old scheduler-side cancellation also
    // stopped ordinary softening every poll when random erections were off,
    // causing an endless restart at the last accepted bend (commonly 5/20).
    if (!normalControl && erectionRelaxing.load()) {
        CancelErectionAnimation();
        playerContext.position.appliedBend.store(-1);
    }

    if (normalControl && erectionRelaxing.load()) {
        if (!cbpc) {
            // A backend can reach its lowest visible angle before Papyrus is
            // ready to accept the owner switch. The animation has finished,
            // but the soft transition remains pending; retry only the handoff
            // and never restore the erect angle in this gap.
            if (!erectionAnimating.load() && playerContext.physics.known.load() && playerContext.physics.usingCBPC.load())
                SetOwner(false, force);
            return;
        }
        CancelErectionAnimation();
        playerContext.position.appliedBend.store(-1);
    }

    // Lower the angle while CBPC still owns the bones, then return ownership
    // to SMP. Handing the bones to SMP first makes the erection snap down.
    if (normalControl && !cbpc && playerContext.physics.known.load() && playerContext.physics.usingCBPC.load() &&
        !erectionRelaxing.load() && copy.gradualErection && copy.positionControl &&
        (playerContext.position.appliedBend.load() < 0 ||
            playerContext.position.appliedBend.load() > (copy.flaccidAngleControl && SosAeNativeLoaded() ? copy.flaccidBend : 0)) &&
        !PPAOwnsPosition()) {
        StartGradualRelaxation(copy);
        return;
    }
    SetOwner(cbpc, force);
}

void Tick() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const auto now = NowMs();

    ProcessPendingQuickAction();
    if (!copy.enabled) return;

    if (copy.mode == 0)
        QueryArousal();
    else
        ClearArousalError();
    if (copy.sexLabOverride) QuerySexLab();
    if (copy.sexLabOverride && copy.sexLabRoleSwitching && sceneController.State().sexLab.active.load()) QuerySexLabRole();
    if (copy.ostimOverride && copy.ostimRoleSwitching && sceneController.State().ostim.active.load()) QueryOStimRole();
    UpdateRandomErection(copy);
    bool forceStartupDecision = false;
    auto startupDue = startupReconcileDueMs.load();
    const bool arousalProviderPresent =
        OslArousedLoaded() || SloArousedLoaded() || ClassicArousedLoaded();
    const bool waitingForInitialArousal = startupDue > 0 && !playerContext.physics.known.load() &&
        copy.mode == 0 && arousalProviderPresent && !arousalController.Valid() &&
        !RandomErectionActive() && !AnySceneHasPriority(copy) &&
        !ActiveAPIRequest().has_value();
    if (startupDue > 0 && now >= startupDue) {
        if (now >= startupReconcileUntilMs.load()) {
            startupReconcileDueMs.store(0);
            startupReconcileUntilMs.store(0);
            Record("SPS-020: Startup physics check is still waiting for the game; use Repair current physics once loading finishes", true);
        } else if (!playerContext.physics.known.load()) {
            // In Automatic mode, do not force the temporary soft fallback while
            // a detected arousal provider is still returning its first value.
            // That old path produced soft -> erect during every high-arousal
            // load and doubled the number of Papyrus physics handoffs precisely
            // while the VM and player mesh were busiest.
            forceStartupDecision = !waitingForInitialArousal;
            startupReconcileDueMs.store(now + (waitingForInitialArousal ? 500 : 1000));
        } else {
            startupReconcileDueMs.store(0);
            startupReconcileUntilMs.store(0);
            Record("Startup physics state selected");
        }
    }

    // The ordinary poll uses soft as its safe fallback when no arousal value is
    // known. During startup that fallback is not a decision: it merely creates
    // a needless soft handoff before the already-dispatched arousal query can
    // return. Leave the current mesh alone until a provider replies or the
    // bounded startup window expires.
    if (!waitingForInitialArousal)
        Evaluate(forceStartupDecision);
    CaptureState("poll");

    if (startupReconcileDueMs.load() > 0 && playerContext.physics.known.load()) {
        startupReconcileDueMs.store(0);
        startupReconcileUntilMs.store(0);
        Record("Startup physics state selected");
    }

    if (activeManualPhysicsTest.load() >= 0 && now >= manualPhysicsTestUntilMs.load()) {
        activeManualPhysicsTest.store(-1);
        Record("Physics test finished; normal control resumed");
    }

    auto postSwitchDue = postSwitchVerificationDueMs.load();
    if (postSwitchDue > 0 && now >= postSwitchDue &&
        postSwitchVerificationDueMs.compare_exchange_strong(postSwitchDue, 0)) {
        if (ConfirmCurrentPhysicsOwner("the completed physics switch")) {
            postSwitchVerificationUntilMs.store(0);
        } else if (now < postSwitchVerificationUntilMs.load()) {
            postSwitchVerificationDueMs.store(now + 1000);
        } else {
            postSwitchVerificationUntilMs.store(0);
            Record("SPS-010: Final physics handoff check failed after several bounded attempts", true);
        }
    }

    auto diagnosticsDue = diagnosticsRefreshDueMs.load();
    if (diagnosticsDue > 0 && now >= diagnosticsDue &&
        diagnosticsRefreshDueMs.compare_exchange_strong(diagnosticsDue, 0)) {
        RefreshDiagnostics();
    }

    auto ownerRepairDue = externalOwnerRepairDueMs.load();
    if (ownerRepairDue > 0 && now >= ownerRepairDue &&
        externalOwnerRepairDueMs.compare_exchange_strong(ownerRepairDue, 0)) {
        if (ConfirmCurrentPhysicsOwner("an external physics reset")) {
            externalOwnerRepairUntilMs.store(0);
        } else if (now < externalOwnerRepairUntilMs.load()) {
            externalOwnerRepairDueMs.store(now + 1000);
        } else {
            externalOwnerRepairUntilMs.store(0);
            Record("SPS-010: Physics ownership could not be restored after an external reset", true);
        }
    }

    auto resetDue = loadSMPResetDueMs.load();
    if (resetDue > 0 && now >= resetDue &&
        loadSMPResetDueMs.compare_exchange_strong(resetDue, 0)) {
        RunLoadSMPReset();
    }

    auto resetRestoreDue = loadSMPResetRestoreDueMs.load();
    if (resetRestoreDue > 0 && now >= resetRestoreDue &&
        loadSMPResetRestoreDueMs.compare_exchange_strong(resetRestoreDue, 0)) {
        // A full FSMP rebuild can restore the XML's default dynamic state.
        // Reapply the current arousal/scene decision and position exactly once.
        playerContext.position.appliedBend.store(-1);
        Evaluate(true);
        if (playerContext.physics.known.load() && !playerContext.physics.usingCBPC.load())
            softConfirmationDueMs.store(now + 250);
        else if (playerContext.physics.known.load() && playerContext.physics.usingCBPC.load() && copy.positionControl)
            erectMeshReplayDueMs.store(now + 2250);
        Record("Physics state restored after the delayed SMP reset");
    }

    auto softHandoffResetDue = softHandoffResetDueMs.load();
    if (!playerContext.physics.usingCBPC.load() && softHandoffResetDue > 0 && now >= softHandoffResetDue &&
        softHandoffResetDueMs.compare_exchange_strong(softHandoffResetDue, 0)) {
        RunSoftHandoffSMPReset();
    }

    auto softHandoffRestoreDue = softHandoffResetRestoreDueMs.load();
    if (!playerContext.physics.usingCBPC.load() && softHandoffRestoreDue > 0 && now >= softHandoffRestoreDue &&
        softHandoffResetRestoreDueMs.compare_exchange_strong(softHandoffRestoreDue, 0)) {
        playerContext.position.appliedBend.store(-1);
        ConfirmCurrentPhysicsOwner("the completed soft handoff refresh");
        softConfirmationDueMs.store(now + 250);
        Record("Soft physics restored after the handoff refresh");
    }

    auto softAngleDue = softAngleRefreshDueMs.load();
    if (!playerContext.physics.usingCBPC.load() && softAngleDue > 0 && now >= softAngleDue &&
        softAngleRefreshDueMs.compare_exchange_strong(softAngleDue, 0)) {
        RunSoftAngleRefresh();
    }

    auto softAngleRestoreDue = softAngleRefreshRestoreDueMs.load();
    if (!playerContext.physics.usingCBPC.load() && softAngleRestoreDue > 0 && now >= softAngleRestoreDue &&
        softAngleRefreshRestoreDueMs.compare_exchange_strong(softAngleRestoreDue, 0)) {
        playerContext.position.appliedBend.store(-1);
        ApplyRequestedSoftBend(true, true);
        softConfirmationDueMs.store(now + 250);
        Record("Soft angle restored after the SMP refresh");
    }

    auto nodeResetRestoreDue = nodeSMPResetRestoreDueMs.load();
    if (!playerContext.physics.usingCBPC.load() && nodeResetRestoreDue > 0 && now >= nodeResetRestoreDue &&
        nodeSMPResetRestoreDueMs.compare_exchange_strong(nodeResetRestoreDue, 0)) {
        playerContext.position.appliedBend.store(-1);
        ConfirmCurrentPhysicsOwner("the completed player mesh refresh");
        softConfirmationDueMs.store(now + 250);
        Record("Soft physics restored after the player mesh change");
    }

    auto nodeCBPCDue = nodeCBPCReacquireDueMs.load();
    if (playerContext.physics.usingCBPC.load() && nodeCBPCDue > 0 && now >= nodeCBPCDue &&
        nodeCBPCReacquireDueMs.compare_exchange_strong(nodeCBPCDue, 0)) {
        if (ConfirmCurrentPhysicsOwner("the rebuilt player mesh")) {
            nodeCBPCReacquireUntilMs.store(0);
            playerContext.position.appliedBend.store(-1);
            if (copy.positionControl) {
                bendSettleDueMs.store(now + copy.settleDelayMs);
                bendConfirmationDueMs.store(now + copy.settleDelayMs + 1500);
                erectMeshReplayDueMs.store(now + 2500);
            }
        } else if (now < nodeCBPCReacquireUntilMs.load()) {
            nodeCBPCReacquireDueMs.store(now + 1000);
        } else {
            nodeCBPCReacquireUntilMs.store(0);
            Record("SPS-010: Erect physics could not reconnect after the player equipment change", true);
        }
    }

    auto softDue = softConfirmationDueMs.load();
    if (!playerContext.physics.usingCBPC.load() && softDue > 0 && now >= softDue &&
        softConfirmationDueMs.compare_exchange_strong(softDue, 0)) {
        auto until = softConfirmationUntilMs.load();
        if (until == 0) {
            until = now + 10000;
            softConfirmationUntilMs.store(until);
        }
        if (ConfirmSoftState()) {
            softConfirmationUntilMs.store(0);
        } else if (now < until) {
            softConfirmationDueMs.store(now + 1000);
        } else {
            softConfirmationUntilMs.store(0);
            Record("SPS-010: Soft-state confirmation stopped after several bounded attempts", true);
        }
    }

    auto cbpcDue = cbpcConfirmationDueMs.load();
    if (playerContext.physics.usingCBPC.load() && cbpcDue > 0 && now >= cbpcDue &&
        cbpcConfirmationDueMs.compare_exchange_strong(cbpcDue, 0)) {
        auto until = cbpcConfirmationUntilMs.load();
        if (until == 0) {
            until = now + 10000;
            cbpcConfirmationUntilMs.store(until);
        }
        if (ConfirmCBPCState()) {
            cbpcConfirmationUntilMs.store(0);
        } else if (now < until) {
            cbpcConfirmationDueMs.store(now + 1000);
        } else {
            cbpcConfirmationUntilMs.store(0);
            Record("SPS-010: Erect-state confirmation stopped after several bounded attempts", true);
        }
    }

    bool targetStillWantsCBPC = false;
    if (const auto request = ActiveAPIRequest())
        targetStillWantsCBPC = request->state == SPS::API::PhysicsState::CBPC;
    else if (OStimHasPriority(copy))
        targetStillWantsCBPC = OStimSceneWantsCBPC(copy);
    else if (SexLabHasPriority(copy))
        targetStillWantsCBPC = SexLabSceneWantsCBPC(copy);
    else
        targetStillWantsCBPC = NormalSettingsWantCBPC(copy);

    if (!targetStillWantsCBPC) {
        // No delayed erect-position callback may survive a decision to soften.
        bendSettleDueMs.store(0);
        bendConfirmationDueMs.store(0);
        bendRetryDueMs.store(0);
    }

    bool settledNow = false;
    auto settleDue = bendSettleDueMs.load();
    if (playerContext.physics.usingCBPC.load() && targetStillWantsCBPC && settleDue > 0 && now >= settleDue &&
        bendSettleDueMs.compare_exchange_strong(settleDue, 0)) {
        const bool timedGradual = copy.gradualErection &&
            (!copy.arousalBasedErection || RandomErectionActive());
        if (timedGradual && !AnySceneHasPriority(copy) && !PPAOwnsPosition())
            StartGradualErection(DesiredBend(copy), copy.erectionDurationMs);
        else
            ApplyRequestedBend(true, true, true);
        settledNow = true;
    }

    // CBPC starts through Papyrus and may finish after the first bend request.
    // Confirm the final value once, after it has settled, without replaying an
    // animation or creating the old continuous repair loop.
    auto confirmDue = bendConfirmationDueMs.load();
    if (playerContext.physics.usingCBPC.load() && targetStillWantsCBPC && confirmDue > 0 && now >= confirmDue &&
        bendConfirmationDueMs.compare_exchange_strong(confirmDue, 0)) {
        ApplyRequestedBend(true, true, true);
    }

    // During a failed or delayed CBPC -> SMP handoff the confirmed owner can
    // still be CBPC for another tick even though the requested state is soft.
    // The old maintenance pass interpreted that gap as a lost erect angle and
    // started raising 0 back to 17/20 at zero arousal. Only maintain the erect
    // angle while the current control source still actually wants CBPC.
    if (!settledNow && playerContext.physics.usingCBPC.load() && targetStillWantsCBPC &&
        copy.positionControl && !erectionRelaxing.load()) {
        const int desired = DesiredBend(copy);
        const auto retryDue = bendRetryDueMs.load();
        if (playerContext.position.appliedBend.load() != desired && bendSettleDueMs.load() == 0 &&
            (retryDue == 0 || now >= retryDue))
            ApplyRequestedBend(true, copy.animatePosition, true);
    }

    auto nodeDue = nodeRefreshDueMs.load();
    if (nodeDue > 0 && now >= nodeDue && nodeRefreshDueMs.compare_exchange_strong(nodeDue, 0)) {
        // A real later rebuild may discard the bend, but it does not justify a
        // state decision. Re-confirm the existing owner once, then restore only
        // the position data that the rebuilt skeleton may have discarded.
        if (playerContext.physics.known.load() && !playerContext.physics.usingCBPC.load() && RefreshSMPAfterPlayerMeshChange()) {
            // The player-only reset restores the owner and angle after FSMP
            // finishes rebuilding the newly equipped schlong mesh.
        } else if (playerContext.physics.known.load() && playerContext.physics.usingCBPC.load()) {
            // CBPC can remain attached to the old genital nodes when armour or
            // the equipped schlong replaces the player mesh. Stop the stale
            // bindings first, then reacquire the newly-created bones on a later
            // tick. A direct equipment listener also reaches this path when a
            // mod does not emit an SKSE NiNode update event.
            CancelErectionAnimation();
            ResetPositionRecovery();
            playerContext.position.appliedBend.store(-1);
            bendSettleDueMs.store(0);
            bendConfirmationDueMs.store(0);
            cbpcConfirmationDueMs.store(0);
            if (!PapyrusReadyForDispatch()) {
                nodeCBPCReacquireDueMs.store(now + 1000);
                nodeCBPCReacquireUntilMs.store(now + 10000);
            } else if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                auto* actor = static_cast<RE::Actor*>(player);
                const bool released = ReleasePlayerPhysics(actor);
                nodeCBPCReacquireDueMs.store(now + (released ? 350 : 1000));
                nodeCBPCReacquireUntilMs.store(now + 10000);
                if (released)
                    Record("Player mesh changed; reconnecting erect physics to its new bones");
                else
                    logger::warn("Could not release the old erect-physics bindings; bounded retry queued");
            }
        } else if (playerContext.physics.known.load() && !playerContext.physics.usingCBPC.load() && nodeSMPResetRestoreDueMs.load() == 0) {
            softConfirmationDueMs.store(now + 750);
        }
        nodeRefreshFollowupDueMs.store(now + 1500);
    }

    auto nodeFollowupDue = nodeRefreshFollowupDueMs.load();
    if (nodeFollowupDue > 0 && now >= nodeFollowupDue &&
        nodeRefreshFollowupDueMs.compare_exchange_strong(nodeFollowupDue, 0)) {
        // Some armour managers rebuild the genital node twice. This quiet
        // second confirmation catches the late rebuild without another reset.
        ConfirmCurrentPhysicsOwner("the completed equipment change");
        playerContext.position.appliedBend.store(-1);
        if (playerContext.physics.known.load() && playerContext.physics.usingCBPC.load() && copy.positionControl)
            ApplyRequestedBend(true, true, true);
        else if (playerContext.physics.known.load() && !playerContext.physics.usingCBPC.load())
            ApplyRequestedSoftBend(true, false);
    }

    auto erectReplayDue = erectMeshReplayDueMs.load();
    if (erectReplayDue > 0 && now >= erectReplayDue &&
        erectMeshReplayDueMs.compare_exchange_strong(erectReplayDue, 0)) {
        // Armour and schlong changes can replace the live skeleton after both
        // the CBPC handoff and the first bend request have already succeeded.
        // Replay the saved angle once after the replacement mesh is stable.
        if (playerContext.physics.known.load() && playerContext.physics.usingCBPC.load() && copy.positionControl) {
            CancelErectionAnimation();
            playerContext.position.appliedBend.store(-1);
            ApplyRequestedBend(true, true, true);
            Record("Erect angle restored after the replacement mesh settled");
        }
    }
}

void StartPolling() {
    if (polling.exchange(true)) return;
    pollThread = std::jthread([](std::stop_token token) {
        while (!token.stop_requested()) {
            int wait;
            { std::scoped_lock lock(settingsLock); wait = settings.pollMs; }
            std::this_thread::sleep_for(std::chrono::milliseconds(wait));
            if (!token.stop_requested())
                if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask(Tick);
        }
    });
}

void RefreshDiagnostics() {
    Diagnostics result = SPS::Diagnostics::Scan(NowMs());
    const bool incompatibleFsmp = result.fsmpModuleLoaded && !result.fsmpActorApiAvailable;
    const bool missingFsmpBridge = result.fsmpActorApiAvailable && !result.fsmpBridgePresent;
    { std::scoped_lock lock(diagnosticsLock); diagnostics = std::move(result); }
    Record("Compatibility health check completed");
    if (incompatibleFsmp && !fsmpCompatibilityWarningShown.exchange(true))
        Record("SPS-003: Faster HDT-SMP is too old for SPS. Update to FSMP 4.0.1 or newer.", true);
    else if (missingFsmpBridge && !fsmpCompatibilityWarningShown.exchange(true))
        Record("SPS-021: The SPS FSMP bridge is missing. Reinstall SPS and let it replace the previous version.", true);
}

void StatusLine(const char* label, const char* status, int level) {
    ImGuiMCP::Text("%s", label);
    ImGuiMCP::SameLine(245.0F);
    const ImGuiMCP::ImVec4 color = level == 2 ? ImGuiMCP::ImVec4(0.35F, 1.0F, 0.45F, 1.0F)
        : level == 1 ? ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F)
        : ImGuiMCP::ImVec4(1.0F, 0.35F, 0.35F, 1.0F);
    ImGuiMCP::TextColored(color, "%s", status);
}

void PageHeading(const char* title, const char* description) {
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "%s", title);
    if (description && description[0] != '\0')
        ImGuiMCP::TextWrapped("%s", description);
    ImGuiMCP::Separator();
}

void SectionHeading(const char* title) {
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "%s", title);
}

void UseRecommendedSettings(Settings& value) {
    value.hysteresis = 5.0F;
    value.pollMs = 1000;
    value.switchCooldownMs = 750;
    value.resetSMPAfterLoad = true;
    value.loadResetDelayMs = 10000;
    value.sexLabOverride = true;
    value.sexLabRoleSwitching = true;
    value.sexLabBottomBehavior = 0;
    value.sexLabUnknownRole = 0;
    value.sceneEndDelayMs = 1500;
    value.ostimOverride = true;
    value.ostimRoleSwitching = true;
    value.positionControl = true;
    value.flaccidAngleControl = false;
    value.flaccidBend = 0;
    value.bendMethod = 2;
    value.animatePosition = true;
    value.gradualErection = true;
    value.arousalBasedErection = false;
    value.erectionStartArousal = 20.0F;
    value.erectionDurationMs = 3000;
    value.softeningDurationMs = 5000;
    value.randomErections = false;
    value.randomErectionMinMinutes = 15;
    value.randomErectionMaxMinutes = 45;
    value.randomErectionDurationSeconds = 60;
    value.randomErectionSafeMoments = true;
    value.spontaneousRefractory = true;
    value.refractoryMinutes = 5;
    value.morningErections = false;
    value.morningErectionDurationSeconds = 90;
    value.equipmentChangeRecovery = true;
    value.bounceGuard = true;
    value.settleDelayMs = 350;
    value.maxBendFailures = 3;
}

void SaveSettingsAndApply(const Settings& copy, const Settings& previous) {
    const bool softPositionChanged = copy.flaccidAngleControl != previous.flaccidAngleControl ||
        copy.flaccidBend != previous.flaccidBend;
    const bool positionChanged = copy.erectBend != previous.erectBend || softPositionChanged ||
        copy.sexLabBend != previous.sexLabBend || copy.useSexLabBend != previous.useSexLabBend ||
        copy.positionControl != previous.positionControl || copy.bendMethod != previous.bendMethod ||
        copy.animatePosition != previous.animatePosition || copy.gradualErection != previous.gradualErection ||
        copy.arousalBasedErection != previous.arousalBasedErection ||
        copy.erectionStartArousal != previous.erectionStartArousal ||
        copy.threshold != previous.threshold || copy.erectionDurationMs != previous.erectionDurationMs ||
        copy.softeningDurationMs != previous.softeningDurationMs;
    const bool randomSettingsChanged = copy.randomErections != previous.randomErections ||
        copy.randomErectionMinMinutes != previous.randomErectionMinMinutes ||
        copy.randomErectionMaxMinutes != previous.randomErectionMaxMinutes ||
        copy.randomErectionDurationSeconds != previous.randomErectionDurationSeconds ||
        copy.randomErectionSafeMoments != previous.randomErectionSafeMoments ||
        copy.spontaneousRefractory != previous.spontaneousRefractory ||
        copy.refractoryMinutes != previous.refractoryMinutes ||
        copy.morningErections != previous.morningErections ||
        copy.morningErectionDurationSeconds != previous.morningErectionDurationSeconds;
    { std::scoped_lock lock(settingsLock); settings = copy; }
    if (!copy.equipmentChangeRecovery) {
        nodeRefreshDueMs.store(0);
        nodeRefreshFollowupDueMs.store(0);
    }
    if (previous.enabled && !copy.enabled) {
        ClearAPIRequests();
        externalOwnerRepairDueMs.store(0);
    }
    spdlog::set_level(copy.verboseLogging ? spdlog::level::debug : spdlog::level::info);
    Save();
    if (positionChanged) {
        CancelErectionAnimation();
        ResetPositionRecovery();
        playerContext.position.appliedBend.store(-1);
        if (playerContext.physics.known.load() && playerContext.physics.usingCBPC.load())
            bendConfirmationDueMs.store(NowMs() + 1000);
    }
    if (randomSettingsChanged) {
        CancelErectionAnimation();
        randomErectionNextMs.store(0);
        randomErectionUntilMs.store(0);
        randomErectionManualTest.store(false);
        erectionRelaxing.store(false);
        spontaneousRefractoryUntilMs.store(0);
        morningErectionDueMs.store(0);
        morningErectionActive.store(false);
    }
    if (auto* tasks = SKSE::GetTaskInterface()) {
        tasks->AddTask([positionChanged, softPositionChanged,
            enabled = copy.enabled, positionEnabled = copy.positionControl] {
            if (positionChanged && positionEnabled) {
                if (playerContext.physics.known.load() && playerContext.physics.usingCBPC.load()) ApplyRequestedBend(true, true, false);
                else if (playerContext.physics.known.load() && softPositionChanged) ScheduleSoftAngleRefresh();
                else if (playerContext.physics.known.load()) ApplyRequestedSoftBend(true, true);
            }
            if (enabled) Evaluate();
            else SetOwner(false, true);
        });
    }
}

void __stdcall RenderMain() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const Settings previous = copy;
    bool changed = false;

    Diagnostics d;
    { std::scoped_lock lock(diagnosticsLock); d = diagnostics; }
    const bool healthChecked = d.checkedAtMs > 0;
    const bool coreReady = CoreReady(d, copy);

    PageHeading("HOME", "See what SPS is doing and change its everyday behaviour.");

    SectionHeading("CURRENT STATUS");
    StatusLine("SPS", !healthChecked ? "Checking setup..." :
        (coreReady ? (playerContext.physics.known.load() ? "Ready" : "Waiting for the player") : "Needs attention"),
        !healthChecked || !playerContext.physics.known.load() ? 1 : (coreReady ? 2 : 0));
    StatusLine("Physics", playerContext.physics.known.load() ?
        (playerContext.physics.usingCBPC.load() ? "Erect (CBPC)" : "Soft (SMP)") : "Not decided yet",
        playerContext.physics.known.load() ? 2 : 1);
    const char* modeStatus[]{ "Automatic - follows arousal", "Always soft (SMP)", "Always erect (CBPC)" };
    StatusLine("Mode", modeStatus[std::clamp(copy.mode, 0, 2)], copy.enabled ? 2 : 1);
    if (copy.mode == 0) {
        const auto arousalText = arousalController.Valid() ?
            fmt::format("Arousal: {:.0f} / 100", arousalController.Value()) :
            std::string("Waiting for the selected arousal mod");
        ImGuiMCP::ProgressBar(arousalController.Valid() ? arousalController.Value() / 100.0F : 0.0F,
            ImGuiMCP::ImVec2(-1.0F, 0.0F), arousalText.c_str());
    } else {
        StatusLine("Arousal", "Not used in manual mode", 2);
    }
    if (sceneController.State().ostim.active.load())
        StatusLine("OStim role", sceneController.State().ostim.roleValid.load() ? OStimRoleName(sceneController.State().ostim.role.load()) : "Checking...", sceneController.State().ostim.roleValid.load() ? 2 : 1);
    else if (sceneController.State().sexLab.active.load())
        StatusLine("SexLab role", sceneController.State().sexLab.roleValid.load() ? SexLabRoleName(sceneController.State().sexLab.role.load()) : "Checking...", sceneController.State().sexLab.roleValid.load() ? 2 : 1);
    if (healthChecked && !coreReady)
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F),
            "Open Troubleshooting to see what needs attention.");

    ImGuiMCP::Separator();
    SectionHeading("MAIN CONTROLS");
    changed |= ImGuiMCP::Checkbox("Enable SPS", &copy.enabled);
    const char* modes[]{ "Automatic - follow arousal", "Always soft - use SMP", "Always erect - use CBPC" };
    changed |= ImGuiMCP::Combo("Physics mode", &copy.mode, modes, 3);

    if (copy.mode == 0) {
        if (ImGuiMCP::SliderFloat("Become erect at arousal", &copy.threshold, 0, 100, "%.0f")) {
            if (copy.arousalBasedErection && copy.threshold <= copy.erectionStartArousal)
                copy.erectionStartArousal = std::max(0.0F, copy.threshold - 1.0F);
            changed = true;
        }
        if (copy.arousalBasedErection)
            ImGuiMCP::TextWrapped("The angle begins rising at %.0f arousal and reaches the selected erect angle at %.0f.",
                copy.erectionStartArousal, copy.threshold);
        else
            ImGuiMCP::TextWrapped("SPS becomes erect at %.0f arousal and returns to soft below %.0f.",
                copy.threshold, std::max(0.0F, copy.threshold - copy.hysteresis));
    } else if (copy.mode == 1) {
        ImGuiMCP::TextWrapped("SPS will keep the player soft with SMP. An arousal mod is not required.");
    } else {
        ImGuiMCP::TextWrapped("SPS will keep the player erect with CBPC. An arousal mod is not required.");
    }
    ImGuiMCP::TextWrapped("The soft and erect positions below define the two states SPS switches between.");

    ImGuiMCP::Separator();
    SectionHeading("POSITION");
    if (!copy.positionControl)
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F),
            "Angle control is disabled in Advanced settings. Physics switching still works.");

    ImGuiMCP::Text("Soft");
    const bool softAngleAvailable = SosAeNativeLoaded();
    ImGuiMCP::BeginDisabled(!copy.positionControl || !softAngleAvailable);
    changed |= ImGuiMCP::Checkbox("Use a custom soft angle", &copy.flaccidAngleControl);
    ImGuiMCP::BeginDisabled(!copy.flaccidAngleControl);
    changed |= ImGuiMCP::SliderInt("Soft angle", &copy.flaccidBend, 0, 20);
    ImGuiMCP::EndDisabled();
    ImGuiMCP::EndDisabled();
    if (softAngleAvailable)
        ImGuiMCP::TextWrapped("0 is the normal hanging position. SPS refreshes SMP after you finish moving the slider.");
    else
        ImGuiMCP::TextWrapped("Custom soft angles need SOS AE-NG. Legacy SOS and TNG keep their normal floppy pose.");
    ImGuiMCP::BeginDisabled(!copy.positionControl || !copy.flaccidAngleControl ||
        !softAngleAvailable || !playerContext.physics.known.load() || playerContext.physics.usingCBPC.load());
    if (ImGuiMCP::Button("Refresh soft angle now")) {
        ResetPositionRecovery();
        playerContext.position.appliedBend.store(-1);
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask([] { ScheduleSoftAngleRefresh(0); });
    }
    ImGuiMCP::EndDisabled();

    ImGuiMCP::Text("Erect");
    ImGuiMCP::BeginDisabled(!copy.positionControl);
    changed |= ImGuiMCP::SliderInt("Erect angle", &copy.erectBend, 0, 20);
    ImGuiMCP::EndDisabled();
    ImGuiMCP::TextWrapped("0 points straight out. 20 is the highest position.");
    ImGuiMCP::BeginDisabled(!copy.positionControl || !playerContext.physics.known.load() || !playerContext.physics.usingCBPC.load());
    if (ImGuiMCP::Button("Apply erect angle now")) {
        ResetPositionRecovery();
        playerContext.position.appliedBend.store(-1);
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask([] { ApplyRequestedBend(true, true, false); });
    }
    ImGuiMCP::EndDisabled();

    ImGuiMCP::Separator();
    SectionHeading("QUICK ACTIONS");
    if (ImGuiMCP::Button("Refresh status"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { QueryArousal(true); QuerySexLab(); QuerySexLabRole(); QueryOStimRole(); Evaluate(); });
    if (ImGuiMCP::CollapsingHeader("Reset options")) {
        ImGuiMCP::TextWrapped("This restores the recommended SPS settings. It does not uninstall anything or change other mods.");
        if (ImGuiMCP::Button("Restore recommended settings")) { UseRecommendedSettings(copy); changed = true; }
    }
    if (changed) SaveSettingsAndApply(copy, previous);
}

void __stdcall RenderLooks() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const Settings previous = copy;
    bool changed = false;

    PageHeading("APPEARANCE", "Control how SPS transitions between the soft and erect positions selected on Home.");
    SectionHeading("TRANSITION");
    ImGuiMCP::BeginDisabled(copy.mode != 0);
    const bool arousalRiseChanged = ImGuiMCP::Checkbox(
        "Rise gradually as arousal increases", &copy.arousalBasedErection);
    changed |= arousalRiseChanged;
    ImGuiMCP::BeginDisabled(!copy.arousalBasedErection);
    if (ImGuiMCP::SliderFloat("Arousal where rising starts", &copy.erectionStartArousal,
        0.0F, 99.0F, "%.0f")) {
        if (copy.erectionStartArousal >= copy.threshold)
            copy.threshold = std::min(100.0F, copy.erectionStartArousal + 1.0F);
        changed = true;
    }
    ImGuiMCP::EndDisabled();
    ImGuiMCP::EndDisabled();
    if (copy.arousalBasedErection)
        ImGuiMCP::TextWrapped("The angle follows arousal from %.0f to %.0f instead of jumping straight to fully erect.",
            copy.erectionStartArousal, copy.threshold);

    changed |= ImGuiMCP::Checkbox("Use a smooth timed rise when switching", &copy.gradualErection);
    ImGuiMCP::BeginDisabled(!copy.gradualErection);
    float erectionSeconds = copy.erectionDurationMs / 1000.0F;
    if (ImGuiMCP::SliderFloat("Timed rise length", &erectionSeconds, 0.5F, 10.0F, "%.1f seconds")) {
        copy.erectionDurationMs = static_cast<int>(std::lround(erectionSeconds * 1000.0F));
        changed = true;
    }
    float softeningSeconds = copy.softeningDurationMs / 1000.0F;
    if (ImGuiMCP::SliderFloat("Time to soften again", &softeningSeconds, 0.5F, 15.0F, "%.1f seconds")) {
        copy.softeningDurationMs = static_cast<int>(std::lround(softeningSeconds * 1000.0F));
        changed = true;
    }
    ImGuiMCP::EndDisabled();
    if (copy.arousalBasedErection)
        ImGuiMCP::TextWrapped("The timed rise is still used for random erections. Normal arousal changes follow the live arousal value.");
    ImGuiMCP::TextWrapped("Erect physics stays stable with a little natural movement. Soft physics provides the looser sway.");

    ImGuiMCP::Separator();
    if (ImGuiMCP::CollapsingHeader("Spontaneous erections (optional)")) {
        ImGuiMCP::TextWrapped("These options only run in Automatic mode and never override an active scene.");
        ImGuiMCP::BeginDisabled(copy.mode != 0);
        changed |= ImGuiMCP::Checkbox("Allow random erections", &copy.randomErections);
        ImGuiMCP::BeginDisabled(!copy.randomErections);
        if (ImGuiMCP::SliderInt("Shortest interval (minutes)",
            &copy.randomErectionMinMinutes, 1, 120)) {
            copy.randomErectionMaxMinutes = std::max(copy.randomErectionMinMinutes,
                copy.randomErectionMaxMinutes);
            changed = true;
        }
        if (ImGuiMCP::SliderInt("Longest interval (minutes)",
            &copy.randomErectionMaxMinutes, 1, 240)) {
            copy.randomErectionMinMinutes = std::min(copy.randomErectionMinMinutes,
                copy.randomErectionMaxMinutes);
            changed = true;
        }
        changed |= ImGuiMCP::SliderInt("Duration (seconds)",
            &copy.randomErectionDurationSeconds, 5, 600);
        changed |= ImGuiMCP::Checkbox("Wait for a suitable moment",
            &copy.randomErectionSafeMoments);
        ImGuiMCP::EndDisabled();

        changed |= ImGuiMCP::Checkbox("Use a recovery break", &copy.spontaneousRefractory);
        ImGuiMCP::BeginDisabled(!copy.spontaneousRefractory);
        changed |= ImGuiMCP::SliderInt("Recovery time (minutes)", &copy.refractoryMinutes, 1, 60);
        ImGuiMCP::EndDisabled();
        changed |= ImGuiMCP::Checkbox("Allow morning erections after resting", &copy.morningErections);
        ImGuiMCP::BeginDisabled(!copy.morningErections);
        changed |= ImGuiMCP::SliderInt("Morning duration (seconds)",
            &copy.morningErectionDurationSeconds, 10, 600);
        ImGuiMCP::EndDisabled();
        ImGuiMCP::EndDisabled();

        ImGuiMCP::TextWrapped("Suitable moments exclude combat, dialogue, loading screens, paused menus and similar interruptions. Recovery prevents spontaneous erections from happening back to back.");
        if (RandomErectionActive())
            StatusLine("Current status", morningErectionActive.load() ? "Morning erection active" : "Random erection active", 2);
        else if (copy.randomErections)
            StatusLine("Current status", "Waiting for a random interval", 1);

        const bool randomTestBlocked = !copy.randomErections || copy.mode != 0 ||
            AnySceneHasPriority(copy) || ActiveAPIRequest().has_value() || playerContext.physics.usingCBPC.load();
        ImGuiMCP::BeginDisabled(randomTestBlocked);
        if (ImGuiMCP::Button("Test now")) {
            randomErectionNextMs.store(0);
            randomErectionUntilMs.store(NowMs() +
                static_cast<std::int64_t>(copy.randomErectionDurationSeconds) * 1000);
            randomErectionManualTest.store(true);
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { Evaluate(true); });
            Record("Random erection test started");
        }
        ImGuiMCP::EndDisabled();
    }
    if (changed) SaveSettingsAndApply(copy, previous);
}

void __stdcall RenderScenes() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const Settings previous = copy;
    bool changed = false;

    Diagnostics d;
    { std::scoped_lock lock(diagnosticsLock); d = diagnostics; }
    const bool sexLabLoaded = d.sexLabModuleLoaded && d.sexLabPluginLoaded;
    const bool ostimLoaded = d.ostimPluginLoaded;
    const bool ppaLoaded = ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr;

    PageHeading("SCENES", "Choose how SPS behaves during SexLab or OStim scenes. These settings do not affect normal gameplay.");

    SectionHeading("SCENE FRAMEWORKS");
    changed |= ImGuiMCP::Checkbox("Manage SexLab scenes", &copy.sexLabOverride);
    ImGuiMCP::BeginDisabled(!copy.sexLabOverride);
    changed |= ImGuiMCP::Checkbox("Follow the player's SexLab role", &copy.sexLabRoleSwitching);
    ImGuiMCP::EndDisabled();

    changed |= ImGuiMCP::Checkbox("Manage OStim scenes", &copy.ostimOverride);
    ImGuiMCP::BeginDisabled(!copy.ostimOverride);
    changed |= ImGuiMCP::Checkbox("Follow the player's OStim role", &copy.ostimRoleSwitching);
    ImGuiMCP::EndDisabled();
    if (ostimLoaded && !d.ostimRoleBridgePresent)
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.35F, 0.35F, 1.0F),
            "OStim is installed, but the optional SPS OStim bridge is missing. Rerun the FOMOD to add it.");

    ImGuiMCP::Separator();
    SectionHeading("SCENE BEHAVIOUR");
    const bool anyRoleSwitching = (copy.sexLabOverride && copy.sexLabRoleSwitching) ||
        (copy.ostimOverride && copy.ostimRoleSwitching);
    ImGuiMCP::BeginDisabled(!anyRoleSwitching);
    const char* bottomBehaviors[]{
        "Keep whatever state I had (recommended)",
        "Follow live arousal",
        "Always stay soft (SMP)",
        "Always stay erect (CBPC)"
    };
    changed |= ImGuiMCP::Combo("When receiving / bottom", &copy.sexLabBottomBehavior, bottomBehaviors, 4);
    ImGuiMCP::EndDisabled();
    ImGuiMCP::BeginDisabled(!copy.sexLabOverride && !copy.ostimOverride);
    float returnDelaySeconds = copy.sceneEndDelayMs / 1000.0F;
    if (ImGuiMCP::SliderFloat("Wait before returning to normal", &returnDelaySeconds, 0.0F, 10.0F, "%.1f seconds")) {
        copy.sceneEndDelayMs = static_cast<int>(std::lround(returnDelaySeconds * 1000.0F));
        changed = true;
    }
    ImGuiMCP::EndDisabled();
    ImGuiMCP::TextWrapped("Recommended: receiving keeps the state from before the scene, while penetrating uses erect physics. OStim support is experimental.");

    ImGuiMCP::Separator();
    SectionHeading("ANGLE DURING SCENES");
    const bool sexLabPositionChanged = ImGuiMCP::Checkbox("Use a different erect angle in scenes", &copy.useSexLabBend);
    changed |= sexLabPositionChanged;
    if (sexLabPositionChanged && copy.useSexLabBend) copy.sexLabOverride = true;
    ImGuiMCP::BeginDisabled(!copy.useSexLabBend || !copy.positionControl);
    changed |= ImGuiMCP::SliderInt("Scene erect angle", &copy.sexLabBend, 0, 20);
    ImGuiMCP::EndDisabled();
    if (ppaLoaded)
        ImGuiMCP::TextWrapped("PPA controls the live angle during its scenes. SPS only decides whether physics should be soft or erect.");

    if (ImGuiMCP::CollapsingHeader("Unrecognised scene roles")) {
        const char* unknownRoles[]{ "Keep the current state (recommended)", "Use soft physics", "Use erect physics" };
        changed |= ImGuiMCP::Combo("If SPS cannot identify the role", &copy.sexLabUnknownRole, unknownRoles, 3);
        ImGuiMCP::TextWrapped("Keeping the current state avoids a sudden visible change when a scene does not report a clear role.");
    }

    ImGuiMCP::Separator();
    if (ImGuiMCP::CollapsingHeader("Scene status and compatibility")) {
        StatusLine("SexLab", sexLabLoaded ? (sceneController.State().sexLab.connected.load() ? "Ready" : "Still loading") :
            "Not installed (optional)", sexLabLoaded ? (sceneController.State().sexLab.connected.load() ? 2 : 1) : 1);
        StatusLine("OStim", ostimLoaded ? (d.ostimRoleBridgePresent ? "Ready (experimental)" :
            "Bridge missing") : "Not installed (optional)", ostimLoaded ? (d.ostimRoleBridgePresent ? 2 : 0) : 1);
        if (sceneController.State().ostim.active.load())
            StatusLine("Current role", sceneController.State().ostim.roleValid.load() ? OStimRoleName(sceneController.State().ostim.role.load()) :
                "OStim running - checking", sceneController.State().ostim.roleValid.load() ? 2 : 1);
        else
            StatusLine("Current role", sceneController.State().sexLab.active.load() ?
                (sceneController.State().sexLab.roleValid.load() ? SexLabRoleName(sceneController.State().sexLab.role.load()) : "SexLab running - checking") :
                "No scene running", sceneController.State().sexLab.active.load() ? (sceneController.State().sexLab.roleValid.load() ? 2 : 1) : 2);
        StatusLine("PPA", ppaLoaded ? (PPAOwnsPosition() ? "Controlling the live angle" : "Ready") :
            "Not installed (optional)", ppaLoaded ? 2 : 1);
    }

    if (changed) SaveSettingsAndApply(copy, previous);
}

void __stdcall RenderAdvanced() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const Settings previous = copy;
    bool changed = false;

    PageHeading("ADVANCED", "Reliability and compatibility controls. The recommended values should suit most setups.");

    SectionHeading("AUTOMATIC RECOVERY");
    changed |= ImGuiMCP::Checkbox("Refresh player physics after loading", &copy.resetSMPAfterLoad);
    ImGuiMCP::BeginDisabled(!copy.resetSMPAfterLoad);
    float loadResetSeconds = copy.loadResetDelayMs / 1000.0F;
    if (ImGuiMCP::SliderFloat("Delay after loading", &loadResetSeconds, 1.0F, 30.0F, "%.0f seconds")) {
        copy.loadResetDelayMs = static_cast<int>(std::lround(loadResetSeconds * 1000.0F));
        changed = true;
    }
    ImGuiMCP::EndDisabled();
    ImGuiMCP::TextWrapped("SPS resets the player's SMP once, then restores the correct soft or erect state.");
    changed |= ImGuiMCP::Checkbox("Refresh after changing armour or schlong",
        &copy.equipmentChangeRecovery);
    ImGuiMCP::TextWrapped("Recommended. These player-only refreshes restore the selected physics and angle after the mesh changes.");

    if (ImGuiMCP::CollapsingHeader("Automatic switching timing (advanced)")) {
        ImGuiMCP::BeginDisabled(copy.mode != 0);
        changed |= ImGuiMCP::SliderFloat("Soft return gap", &copy.hysteresis, 0, 25, "%.0f arousal");
        ImGuiMCP::TextWrapped("After becoming erect, SPS waits until arousal falls below %.0f before returning to soft. This prevents rapid switching.", std::max(0.0F, copy.threshold - copy.hysteresis));
        float pollSeconds = copy.pollMs / 1000.0F;
        if (ImGuiMCP::SliderFloat("How often SPS checks", &pollSeconds, 0.25F, 5.0F, "%.2f seconds")) {
            copy.pollMs = static_cast<int>(std::lround(pollSeconds * 1000.0F));
            changed = true;
        }
        float cooldownSeconds = copy.switchCooldownMs / 1000.0F;
        if (ImGuiMCP::SliderFloat("Minimum time between changes", &cooldownSeconds, 0.0F, 5.0F, "%.2f seconds")) {
            copy.switchCooldownMs = static_cast<int>(std::lround(cooldownSeconds * 1000.0F));
            changed = true;
        }
        ImGuiMCP::EndDisabled();
        if (copy.mode != 0)
            ImGuiMCP::TextWrapped("These values are only used in Automatic mode.");
    }

    ImGuiMCP::Separator();
    SectionHeading("ANGLE CONTROL");
    changed |= ImGuiMCP::Checkbox("Let SPS control the angle", &copy.positionControl);
    ImGuiMCP::TextWrapped("Turn this off only if another mod should own the angle. Soft/erect physics switching will continue to work.");
    ImGuiMCP::BeginDisabled(!copy.positionControl);
    const char* bendMethods[]{ "SOS AE direct control", "SOS / TNG animation stages", "Choose automatically (recommended)" };
    changed |= ImGuiMCP::Combo("How SPS sets the angle", &copy.bendMethod, bendMethods, 3);
    changed |= ImGuiMCP::Checkbox("Smooth angle changes", &copy.animatePosition);
    changed |= ImGuiMCP::Checkbox("Stop repeated bouncing", &copy.bounceGuard);
    ImGuiMCP::EndDisabled();

    if (playerContext.position.automaticSuspended.load()) {
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F),
            "Automatic angle recovery paused after repeated failures.");
        if (ImGuiMCP::Button("Try angle recovery again")) {
            ResetPositionRecovery();
            playerContext.position.appliedBend.store(-1);
        }
    }

    if (ImGuiMCP::CollapsingHeader("Angle recovery timing (advanced)")) {
        ImGuiMCP::BeginDisabled(!copy.positionControl);
        float settleSeconds = copy.settleDelayMs / 1000.0F;
        if (ImGuiMCP::SliderFloat("Wait before setting the angle", &settleSeconds, 0.0F, 5.0F, "%.2f seconds")) {
            copy.settleDelayMs = static_cast<int>(std::lround(settleSeconds * 1000.0F));
            changed = true;
        }
        changed |= ImGuiMCP::SliderInt("Failures before repairs pause", &copy.maxBendFailures, 1, 10);
        ImGuiMCP::EndDisabled();
        ImGuiMCP::TextWrapped("Only change these if the angle repeatedly fails or bounces.");
    }

    ImGuiMCP::Separator();
    if (ImGuiMCP::CollapsingHeader("Reset advanced settings")) {
        ImGuiMCP::TextWrapped("This restores every SPS recommendation, including the settings on the other pages.");
        if (ImGuiMCP::Button("Restore all recommended settings")) { UseRecommendedSettings(copy); changed = true; }
    }

    if (changed) SaveSettingsAndApply(copy, previous);
}

std::string BuildReport() {
    Diagnostics d;
    { std::scoped_lock lock(diagnosticsLock); d = diagnostics; }
    Settings s;
    { std::scoped_lock lock(settingsLock); s = settings; }
    const auto activity = activityLog.Read();
    std::string fixes;
    const auto activeAPIRequest = ActiveAPIRequest();
    const auto apiStats = apiRegistry.GetStats();
    for (const auto& [code, suggestion] : SuggestedFixes(d))
        fixes += fmt::format("{}: {}\n", code, suggestion);
    if (OslArousedLoaded())
        fixes += "Compatibility: With OSL Aroused 2.9.3+, turn off Enable SOS so it does not fight SPS for the player's angle. OSL 2.9.0 through 2.9.2 may use the legacy SPS FOMOD option instead.\n";
    else if (SloArousedLoaded())
        fixes += "Compatibility: Turn off Use SOS in SLO Aroused NG so it does not fight SPS for the player's angle.\n";
    else if (d.classicArousedPluginLoaded)
        fixes += "Compatibility: Turn off Enable SOS in SexLab Aroused Redux so it does not fight SPS for the player's angle.\n";
    return fmt::format(
        "Schlong Physics Swapper {} diagnostics\n"
        "SkyrimRuntime={} SKSE={}\n"
        "DLLs: MenuFramework={} OSL={} SLO={} FSMP={} CBPC={} SexLab={} OStim={} SOS={}\n"
        "Compatibility: TNG={} PositionBackend={} ClassicSexLabAroused={} SexLabRoleBridge={} OStimRoleBridge={} PPA={} PhysicsEditor={} SOSPhysicsManager={} AutoPhysicsReset={} CrashLogger={}\n"
        "Engine={} StateKnown={} Arousal={:.1f} Provider={} ProviderConnected={} SexLabActive={} SexLabConnected={} SexLabRole={} SexLabRoleValid={} OStimActive={} OStimConnected={} OStimRole={} OStimRoleValid={}\n"
        "CompatibilityAPI=V{} ActiveRequests={} ActiveRequester={} Accepted={} Released={} ResetNotices={} OwnerRepairs={} LastOwnerRepairMs={}\n"
        "MenuFramework={} ArousalProvider={} FSMP={} FSMPActorAPI={} FSMPBridge={} CBPC={} SexLabPPlus={} PositionBackendReady={} SupportedAddon={}\n"
        "PlayerBones={}/6 XML={}/{} [{}]\nCBPCMap={}/{} [{}]\nCBPCParameters={}/{} [{}]\n"
        "SwitchSuccesses={} SwitchFailures={} BendApplies={} LoadSMPResets={} LastLoadSMPResetMs={} LastAction={} LastError={}\n"
        "Position: Enabled={} Requested={} Applied={} LastMethod={} LastSucceeded={} AutoSuspended={} GuardRemainingMs={}\n"
        "Settings: Enabled={} Mode={} Threshold={:.0f} Hysteresis={:.0f} ErectBend={} SoftAngle={} SoftBend={} PollMs={} SexLabOverride={} SexLabRoleSwitching={} OStimOverride={} OStimRoleSwitching={} BottomBehavior={} UnknownRole={} EndDelayMs={} CooldownMs={} ResetSMPAfterLoad={} LoadResetDelayMs={} EquipmentRecovery={} BendMethod={} Animate={} Gradual={} ArousalBased={} ErectionStart={:.0f} ErectionMs={} SofteningMs={} RandomErections={} RandomMinMinutes={} RandomMaxMinutes={} RandomDurationSeconds={} RandomSafeMoments={} Refractory={} RefractoryMinutes={} MorningErections={} MorningDurationSeconds={} BounceGuard={} SettleMs={} SeparateSexLabBend={} SexLabBend={} MaxFailures={} PPA={} VerboseLogging={}\n"
        "\nSuggested fixes\n{}",
        kVersion, runtimeVersion, skseVersion,
        LoadedDllVersion(L"SKSEMenuFramework.dll"), LoadedDllVersion(L"OSLAroused.dll"),
        LoadedDllVersion(L"SexlabArousedNG.dll"),
        LoadedDllVersion(L"hdtsmp64.dll"), LoadedDllVersion(L"cbp.dll"),
        LoadedDllVersion(L"SexLabUtil.dll"), LoadedDllVersion(L"OStim.dll"), SosAeNativeModuleName() ? LoadedDllVersion(SosAeNativeModuleName()) : "not loaded",
        d.tngPluginLoaded, PositionBackendName(), d.classicArousedPluginLoaded, d.sexLabRoleBridgePresent, d.ostimRoleBridgePresent,
        ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr, d.physicsEditorLoaded, d.sosPhysicsManagerLoaded,
        d.autoPhysicsResetLoaded, d.crashLoggerLoaded,
        playerContext.physics.known.load() ? (playerContext.physics.usingCBPC.load() ? "CBPC" : "SMP") : "unknown", playerContext.physics.known.load(), arousalController.Value(), ArousalProviderName(), arousalController.Connected(), sceneController.State().sexLab.active.load(), sceneController.State().sexLab.connected.load(), SexLabRoleName(sceneController.State().sexLab.role.load()), sceneController.State().sexLab.roleValid.load(),
        sceneController.State().ostim.active.load(), sceneController.State().ostim.connected.load(), OStimRoleName(sceneController.State().ostim.role.load()), sceneController.State().ostim.roleValid.load(),
        SPS::API::kVersion, apiStats.active, activeAPIRequest ? activeAPIRequest->requester : "none", apiStats.accepted, apiStats.released,
        externalResetNotices.load(), ownerRestorations.load(), lastOwnerRestorationMs.load(),
        d.menuFrameworkLoaded, d.oslModuleLoaded && d.oslPluginLoaded, d.fsmpModuleLoaded, d.fsmpActorApiAvailable, d.fsmpBridgePresent,
        d.cbpcModuleLoaded, d.sexLabModuleLoaded && d.sexLabPluginLoaded,
        PositionBackendAvailable(), d.supportedAddonLoaded,
        d.playerBonesFound, d.compatibleXmlFiles, d.xmlFiles, d.xmlSummary, d.compatibleCbpcMaps, d.cbpcMapFiles, d.cbpcMapSummary,
        d.compatibleCbpcParameters, d.cbpcParameterFiles, d.cbpcParameterSummary, playerContext.physics.successes.load(), playerContext.physics.failures.load(), playerContext.position.repairs.load(), playerContext.recovery.loadSmpResets.load(), playerContext.recovery.lastLoadSmpResetMs.load(), activity.lastAction, activity.lastError.empty() ? "none" : activity.lastError,
        s.positionControl, playerContext.position.requestedBend.load(), playerContext.position.appliedBend.load(), BendMethodName(playerContext.position.lastMethod.load()), playerContext.position.lastSucceeded.load(), playerContext.position.automaticSuspended.load(), std::max<std::int64_t>(0, bendGuardUntilMs.load() - NowMs()),
        s.enabled, s.mode, s.threshold, s.hysteresis, s.erectBend, s.flaccidAngleControl, s.flaccidBend, s.pollMs, s.sexLabOverride, s.sexLabRoleSwitching, s.ostimOverride, s.ostimRoleSwitching, s.sexLabBottomBehavior, s.sexLabUnknownRole, s.sceneEndDelayMs, s.switchCooldownMs, s.resetSMPAfterLoad, s.loadResetDelayMs, s.equipmentChangeRecovery,
        s.bendMethod, s.animatePosition, s.gradualErection, s.arousalBasedErection, s.erectionStartArousal,
        s.erectionDurationMs, s.softeningDurationMs, s.randomErections, s.randomErectionMinMinutes, s.randomErectionMaxMinutes,
        s.randomErectionDurationSeconds, s.randomErectionSafeMoments, s.spontaneousRefractory, s.refractoryMinutes,
        s.morningErections, s.morningErectionDurationSeconds, s.bounceGuard, s.settleDelayMs, s.useSexLabBend, s.sexLabBend, s.maxBendFailures,
        ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr, s.verboseLogging, fixes);
}

bool WriteTextFile(const fs::path& path, const std::string& text) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) return false;
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream << text;
    return stream.good();
}

void FinishDebugCapture(std::uint64_t generation) {
    if (generation != debugCaptureGeneration.load()) return;
    debugCaptureUntilMs.store(0);
    std::string events;
    {
        std::scoped_lock lock(debugCaptureLock);
        for (const auto& line : debugCaptureLines) events += line + "\n";
    }
    const auto text = BuildReport() + "\n30-second debug capture\n" +
        (events.empty() ? "No events were recorded.\n" : events);
    if (WriteTextFile(kCaptureReport, text))
        Record(fmt::format("Debug capture saved to {}", kCaptureReport));
    else
        Record("SPS-012: Could not save the debug capture file", true);
}

void StartDebugCapture() {
    const auto now = NowMs();
    const auto generation = debugCaptureGeneration.fetch_add(1) + 1;
    debugCaptureStartedMs.store(now);
    debugCaptureUntilMs.store(now + 30000);
    {
        std::scoped_lock lock(debugCaptureLock);
        debugCaptureLines.clear();
        debugCaptureLines.emplace_back("[+0.0s] CAPTURE: Started. Reproduce the problem now.");
    }
    CaptureState("capture start");
    debugCaptureThread = std::jthread([generation](std::stop_token token) {
        for (int i = 0; i < 30 && !token.stop_requested(); ++i)
            std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!token.stop_requested()) {
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([generation] { FinishDebugCapture(generation); });
        }
    });
    Record("30-second debug capture started; reproduce the problem now");
}

void TestPhysicsState(bool cbpc) {
    if (!SetOwner(cbpc, true)) {
        const int action = cbpc ? 1 : 0;
        const bool newlyQueued = pendingQuickAction.exchange(action) != action;
        if (newlyQueued || pendingQuickActionUntilMs.load() == 0)
            pendingQuickActionUntilMs.store(NowMs() + 15000);
        activeManualPhysicsTest.store(-1);
        manualPhysicsTestUntilMs.store(0);
        if (newlyQueued)
            Record(fmt::format("{} physics test queued; waiting for the player and Papyrus to finish loading",
                cbpc ? "Erect" : "Soft"));
        return;
    }
    pendingQuickAction.store(-1);
    pendingQuickActionUntilMs.store(0);
    activeManualPhysicsTest.store(cbpc ? 1 : 0);
    // Start the visible test period after the Papyrus handoff completes. On a
    // busy new game that handoff can take several seconds, so using the time
    // captured before SetOwner made the advertised ten-second test much shorter.
    const auto testStartedMs = NowMs();
    manualPhysicsTestUntilMs.store(testStartedMs + 10000);
    if (!cbpc) softConfirmationDueMs.store(testStartedMs + 250);
    Record(fmt::format("{} physics test active for 10 seconds",
        cbpc ? "Erect" : "Soft"));
    CaptureState(cbpc ? "manual erect test" : "manual soft test");
}

void RepairPhysics() {
    const bool retryingQueuedRepair = pendingQuickAction.load() == 2;
    manualPhysicsTestUntilMs.store(0);
    activeManualPhysicsTest.store(-1);
    ResetPositionRecovery();
    playerContext.physics.retryAfterMs.store(0);
    playerContext.position.appliedBend.store(-1);
    if (!retryingQueuedRepair) Record("Physics repair requested");
    if (!PapyrusReadyForDispatch()) {
        const bool newlyQueued = pendingQuickAction.exchange(2) != 2;
        if (newlyQueued || pendingQuickActionUntilMs.load() == 0)
            pendingQuickActionUntilMs.store(NowMs() + 15000);
        if (newlyQueued)
            Record("Repair queued; waiting for the player and Papyrus to finish loading");
        return;
    }
    Evaluate(true);
    if (!playerContext.physics.known.load()) {
        const bool newlyQueued = pendingQuickAction.exchange(2) != 2;
        if (newlyQueued || pendingQuickActionUntilMs.load() == 0)
            pendingQuickActionUntilMs.store(NowMs() + 15000);
        if (newlyQueued)
            Record("Repair is waiting for a usable player physics state");
        return;
    }
    pendingQuickAction.store(-1);
    pendingQuickActionUntilMs.store(0);
    postSwitchVerificationDueMs.store(NowMs() + 1000);
    Record("Repair applied; final handoff check queued");
    RefreshDiagnostics();
    CaptureState("manual repair");
}

void ProcessPendingQuickAction() {
    const int action = pendingQuickAction.load();
    if (action < 0) return;
    const auto now = NowMs();
    if (now >= pendingQuickActionUntilMs.load()) {
        pendingQuickAction.store(-1);
        pendingQuickActionUntilMs.store(0);
        Record("SPS-020: The requested quick fix could not run while the game was still loading", true);
        return;
    }
    if (!PapyrusReadyForDispatch()) return;

    // Leave the action marked as pending during the attempt so a failure keeps
    // the original 15-second deadline instead of extending it forever.
    if (action == 2)
        RepairPhysics();
    else
        TestPhysicsState(action == 1);
}

void __stdcall RenderDebug() {
    Diagnostics d;
    { std::scoped_lock lock(diagnosticsLock); d = diagnostics; }
    Settings debugSettings;
    { std::scoped_lock lock(settingsLock); debugSettings = settings; }
    const Settings previousDebugSettings = debugSettings;

    const bool coreReady = CoreReady(d, debugSettings);
    PageHeading("TROUBLESHOOTING", "Check the setup, test each physics state and create a report if something still goes wrong.");
    SectionHeading("SETUP CHECK");
    StatusLine("Setup", d.checkedAtMs == 0 ? "Not checked yet" : (coreReady ? "Everything looks good" : "Something needs attention"), d.checkedAtMs == 0 ? 1 : (coreReady ? 2 : 0));
    if (ImGuiMCP::Button("Check setup again"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask(RefreshDiagnostics);
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Restore recommended settings")) {
        Settings fixed;
        { std::scoped_lock lock(settingsLock); fixed = settings; }
        const Settings previous = fixed;
        UseRecommendedSettings(fixed);
        SaveSettingsAndApply(fixed, previous);
    }
    ImGuiMCP::TextWrapped("Green is ready, yellow is optional or still checking, and red needs attention. Restoring settings cannot install a missing requirement.");

    const auto suggestions = SuggestedFixes(d);
    if (suggestions.size() == 1 && suggestions.front().first == "SPS-000")
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.35F, 1.0F, 0.45F, 1.0F), "No setup fixes are currently needed.");
    else {
        SectionHeading("WHAT TO FIX");
        for (const auto& item : suggestions)
            ImGuiMCP::TextWrapped("- %s", item.second.c_str());
    }
    const auto currentActivity = activityLog.Read();
    if (!currentActivity.lastError.empty())
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.45F, 0.35F, 1.0F),
            "Most recent problem: %s", currentActivity.lastError.c_str());
    ImGuiMCP::Separator();

    SectionHeading("TEST AND REPAIR");
    if (ImGuiMCP::Button("Test soft (10 seconds)"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { TestPhysicsState(false); });
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Test erect (10 seconds)"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { TestPhysicsState(true); });
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Repair current physics"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask(RepairPhysics);
    const int queuedAction = pendingQuickAction.load();
    if (queuedAction >= 0) {
        const char* queuedName = queuedAction == 0 ? "soft test" : (queuedAction == 1 ? "erect test" : "repair");
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F),
            "Waiting to run the %s as soon as loading finishes...", queuedName);
    } else if (activeManualPhysicsTest.load() >= 0 && NowMs() < manualPhysicsTestUntilMs.load()) {
        const auto remaining = std::max<std::int64_t>(0, manualPhysicsTestUntilMs.load() - NowMs());
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.35F, 1.0F, 0.45F, 1.0F),
            "%s physics test is active (%lld seconds left)",
            activeManualPhysicsTest.load() == 1 ? "Erect" : "Soft", (remaining + 999) / 1000);
    }
    ImGuiMCP::TextWrapped("Tests temporarily force one state, then normal control resumes. Repair reapplies the state SPS currently expects. Actions wait briefly if the game is still loading.");
    ImGuiMCP::Separator();

    SectionHeading("DETECTED COMPONENTS");
    const auto providerName = ArousalProviderName();
    if (debugSettings.mode != 0 && (!d.oslModuleLoaded || !d.oslPluginLoaded))
        StatusLine("Arousal mod", "Not installed - not needed in manual mode", 2);
    else
        StatusLine("Arousal mod", d.oslModuleLoaded && d.oslPluginLoaded ? (arousalController.Connected() ? fmt::format("{} - ready", providerName).c_str() : fmt::format("{} - still checking", providerName).c_str()) : "Missing", d.oslModuleLoaded && d.oslPluginLoaded ? (arousalController.Connected() ? 2 : 1) : 0);
    StatusLine("Soft physics", !d.fsmpModuleLoaded ? "Faster HDT-SMP is missing" :
        (!d.fsmpActorApiAvailable ? "FSMP is too old - update to 4.0.1+" :
            (!d.fsmpBridgePresent ? "SPS FSMP bridge is missing - reinstall SPS" :
                (playerContext.physics.smpConnected.load() ? "SMP - ready" : "SMP found - not tested yet"))),
        !d.fsmpModuleLoaded || !d.fsmpActorApiAvailable || !d.fsmpBridgePresent ? 0 : (playerContext.physics.smpConnected.load() ? 2 : 1));
    StatusLine("Erect physics", d.cbpcModuleLoaded ? (playerContext.physics.cbpcConnected.load() ? "CBPC - ready" : "CBPC found - not tested yet") : "CBPC is missing", d.cbpcModuleLoaded ? (playerContext.physics.cbpcConnected.load() ? 2 : 1) : 0);
    StatusLine("Compatible schlong", d.playerBonesFound == 6 ? "All 6 physics bones found" : fmt::format("Only {}/6 physics bones found", d.playerBonesFound).c_str(), d.playerBonesFound == 6 ? 2 : 0);
    const bool positionBackendFound = PositionBackendAvailable();
    StatusLine("Erect angle control", positionBackendFound ? fmt::format("{} - ready", PositionBackendName()).c_str() : "Not available - physics switching still works", positionBackendFound ? 2 : 1);

    if (OslArousedLoaded())
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F),
            "OSL 2.9.3+ users: turn off Enable SOS so OSL does not fight SPS for the player's angle. The legacy FOMOD option is only for OSL 2.9.0 through 2.9.2.");
    else if (SloArousedLoaded())
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F),
            "SLO Aroused NG users: turn off Use SOS so SLO does not fight SPS for the player's angle.");
    else if (d.classicArousedPluginLoaded)
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F),
            "SexLab Aroused Redux users: turn off Enable SOS so Redux does not fight SPS for the player's angle.");

    if (d.physicsEditorLoaded)
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F), "Physics Editor is installed. It can stay installed, but disable its schlong controls if SPS changes unexpectedly.");
    if (d.sosPhysicsManagerLoaded)
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.35F, 0.35F, 1.0F), "SOS Physics Manager is enabled. Disable it because it controls the same schlong physics as SPS.");
    if (d.autoPhysicsResetLoaded)
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F), "Auto Physics Reset is also running. Turn off its load, cell or scene resets if the state changes unexpectedly.");

    if (ImGuiMCP::CollapsingHeader("Optional scene mods")) {
        StatusLine("SexLab P+", d.sexLabModuleLoaded && d.sexLabPluginLoaded ? (sceneController.State().sexLab.connected.load() ? "Ready" : "Found - still checking") : "Not installed", d.sexLabModuleLoaded && d.sexLabPluginLoaded ? (sceneController.State().sexLab.connected.load() ? 2 : 1) : 1);
        if (sceneController.State().sexLab.active.load())
            StatusLine("Current scene role", sceneController.State().sexLab.roleValid.load() ? SexLabRoleName(sceneController.State().sexLab.role.load()) : "Checking...", sceneController.State().sexLab.roleValid.load() ? 2 : 1);
        const bool ppaLoaded = ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr;
        StatusLine("PPA", ppaLoaded ? (PPAOwnsPosition() ? "Controlling the scene angle" : "Ready") : "Not installed", ppaLoaded ? 2 : 1);
        if (d.sexLabModuleLoaded && d.sexLabPluginLoaded)
            StatusLine("Scene role support", d.sexLabRoleBridgePresent ? "Ready" : "Missing - reinstall SPS", d.sexLabRoleBridgePresent ? 2 : 0);
        StatusLine("OStim Standalone", d.ostimPluginLoaded ? (d.ostimRoleBridgePresent ? "Ready - experimental" : "Bridge missing - rerun the FOMOD") : "Not installed", d.ostimPluginLoaded ? (d.ostimRoleBridgePresent ? 2 : 0) : 1);
        if (sceneController.State().ostim.active.load())
            StatusLine("Current OStim role", sceneController.State().ostim.roleValid.load() ? OStimRoleName(sceneController.State().ostim.role.load()) : "Checking...", sceneController.State().ostim.roleValid.load() ? 2 : 1);
    }

    if (ImGuiMCP::CollapsingHeader("Physics file details")) {
        StatusLine("SMP XML", d.compatibleXmlFiles > 0 ? fmt::format("Ready - {} compatible", d.compatibleXmlFiles).c_str() : "No compatible file found", d.compatibleXmlFiles > 0 ? 2 : 0);
        ImGuiMCP::TextWrapped("Found: %s", d.xmlSummary.c_str());
        StatusLine("CBPC bone list", d.compatibleCbpcMaps > 0 ? "Ready" : "Missing Gen01-Gen06 bones", d.compatibleCbpcMaps > 0 ? 2 : 0);
        ImGuiMCP::TextWrapped("Found: %s", d.cbpcMapSummary.c_str());
        StatusLine("CBPC movement settings", d.compatibleCbpcParameters > 0 ? "Ready" : "Missing", d.compatibleCbpcParameters > 0 ? 2 : 0);
        ImGuiMCP::TextWrapped("Found: %s", d.cbpcParameterSummary.c_str());
        if (d.compatibleXmlFiles > 1)
            ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F), "More than one compatible SMP XML is visible. The schlong mesh decides which one is used.");
    }

    if (ImGuiMCP::CollapsingHeader("Technical activity")) {
        const auto activeRequests = apiRegistry.Count();
        StatusLine("Mod compatibility API", "V1 - ready", 2);
        ImGuiMCP::Text("Other mods currently controlling physics: %zu", activeRequests);
        ImGuiMCP::Text("Confirmed owner restorations: %u", ownerRestorations.load());
        ImGuiMCP::Text("Successful physics changes: %u", playerContext.physics.successes.load());
        ImGuiMCP::Text("Failed physics changes: %u", playerContext.physics.failures.load());
        ImGuiMCP::Text("Erect angle applications: %u", playerContext.position.repairs.load());
        ImGuiMCP::Text("Requested / applied angle: %d / %d", playerContext.position.requestedBend.load(), playerContext.position.appliedBend.load());
        ImGuiMCP::Text("Last angle method: %s", BendMethodName(playerContext.position.lastMethod.load()));
        StatusLine("Gradual erection", erectionAnimating.load() ? fmt::format("Moving: {}/{}", playerContext.position.appliedBend.load(), erectionAnimationTargetBend.load()).c_str() : "Not moving", erectionAnimating.load() ? 1 : 2);
        StatusLine("Angle repair", playerContext.position.automaticSuspended.load() ? "Paused after failures" : (NowMs() < bendGuardUntilMs.load() ? "Waiting for bouncing to stop" : "Ready"), playerContext.position.automaticSuspended.load() ? 0 : (NowMs() < bendGuardUntilMs.load() ? 1 : 2));
    }

    ImGuiMCP::Separator();
    if (ImGuiMCP::CollapsingHeader("Recent activity")) {
        const auto recentActivity = activityLog.Read();
        ImGuiMCP::TextWrapped("Last action: %s", recentActivity.lastAction.c_str());
        for (const auto& entry : recentActivity.recent)
            ImGuiMCP::TextWrapped("- %s", entry.c_str());
    }

    ImGuiMCP::Separator();
    SectionHeading("CREATE A SUPPORT REPORT");
    ImGuiMCP::TextWrapped("For a repeatable problem, start the recording, close the menu, reproduce it, then return here and save the report.");
    if (DebugCaptureActive()) {
        const auto remaining = std::max<std::int64_t>(0, debugCaptureUntilMs.load() - NowMs());
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F), "Recording... reproduce the problem now (%lld seconds left)", (remaining + 999) / 1000);
    } else if (ImGuiMCP::Button("Record 30 seconds")) {
        StartDebugCapture();
    }
    if (ImGuiMCP::Button("Copy report")) {
        const auto report = BuildReport();
        ImGuiMCP::SetClipboardText(report.c_str());
        Record("Diagnostic report copied to the clipboard");
    }
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Save report")) {
        if (WriteTextFile(kReport, BuildReport()))
            Record(fmt::format("Diagnostic report saved to {}", kReport));
        else
            Record("SPS-012: Could not save the diagnostic report file", true);
    }
    ImGuiMCP::TextWrapped("The report contains SPS settings, detected mods and relevant filenames. It does not include your Windows username, save name or full computer paths.");

    if (ImGuiMCP::CollapsingHeader("Extra logging (advanced)")) {
        if (ImGuiMCP::Checkbox("Write more detail to the log", &debugSettings.verboseLogging)) {
            SaveSettingsAndApply(debugSettings, previousDebugSettings);
            Record(debugSettings.verboseLogging ? "Verbose logging enabled" : "Verbose logging disabled");
        }
        ImGuiMCP::TextWrapped("Only enable this while investigating a problem. It creates a larger log file.");
    }
}

void RegisterMenu() {
    if (!SKSEMenuFramework::IsInstalled()) { Record("SKSE Menu Framework not found", true); return; }
    SKSEMenuFramework::SetSection(kName);
    SKSEMenuFramework::AddSectionItem("Home", RenderMain);
    SKSEMenuFramework::AddSectionItem("Appearance", RenderLooks);
    SKSEMenuFramework::AddSectionItem("Scenes", RenderScenes);
    SKSEMenuFramework::AddSectionItem("Advanced", RenderAdvanced);
    SKSEMenuFramework::AddSectionItem("Troubleshooting", RenderDebug);
}

class ModEventSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        const std::string_view name = event->eventName.c_str();
        if (name == "ostim_start") {
            sceneController.State().ostim.active.store(true);
            sceneController.State().ostim.connected.store(true);
            sceneController.State().ostim.endedMs.store(0);
            sceneController.State().ostim.entryStateValid.store(playerContext.physics.known.load());
            sceneController.State().ostim.entryCBPC.store(playerContext.physics.usingCBPC.load());
            sceneController.State().ostim.roleGeneration.fetch_add(1);
            sceneController.State().ostim.role.store(0);
            sceneController.State().ostim.roleValid.store(false);
            sceneController.State().ostim.roleQueryPending.store(false);
            sceneController.State().ostim.roleRetryAfterMs.store(0);
            Record("OStim player scene started");
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] {
                QueryOStimRole();
                Evaluate();
            });
        } else if (name == "ostim_scenechanged") {
            sceneController.State().ostim.connected.store(true);
            sceneController.State().ostim.roleGeneration.fetch_add(1);
            sceneController.State().ostim.roleValid.store(false);
            sceneController.State().ostim.roleQueryPending.store(false);
            sceneController.State().ostim.roleRetryAfterMs.store(0);
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] {
                QueryOStimRole();
                Evaluate();
            });
        } else if (name == "ostim_end") {
            sceneController.State().ostim.active.store(false);
            sceneController.State().ostim.connected.store(true);
            sceneController.State().ostim.endedMs.store(NowMs());
            sceneController.State().ostim.entryStateValid.store(false);
            sceneController.State().ostim.roleGeneration.fetch_add(1);
            sceneController.State().ostim.role.store(0);
            sceneController.State().ostim.roleValid.store(false);
            sceneController.State().ostim.roleQueryPending.store(false);
            ResetPPASceneTracking(2000);
            if (playerContext.physics.known.load() && !playerContext.physics.usingCBPC.load())
                softConfirmationDueMs.store(NowMs() + 250);
            Record("OStim player scene ended");
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { Evaluate(); });
        } else if (name == "HookAnimationStart" || name == "HookAnimationStarting" ||
            name == "HookStageStart" || name == "HookStageEnd" ||
            name == "HookActorsRelocated" || name == "HookActorChangeEnd" ||
            name == "HookAnimationEnding" || name == "HookAnimationEnd" ||
            name == "AnimationStart" || name == "AnimationEnd") {
            sceneController.State().sexLab.roleGeneration.fetch_add(1);
            sceneController.State().sexLab.roleQueryPending.store(false);
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] {
                QuerySexLab();
                if (sceneController.State().sexLab.active.load()) QuerySexLabRole();
            });
        } else if (name == "OSLA_ActorArousalUpdated" && event->sender == RE::PlayerCharacter::GetSingleton()) {
            // The callback query will evaluate the new value. Arousal updates
            // must not invalidate an already-applied bend or they create a
            // position replay loop while Automatic mode is erect.
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([] { QueryArousal(true); });
        } else if (name == "sla_UpdateComplete") {
            // SLO Aroused NG reports a completed update globally. Querying its
            // OSL compatibility stub is cheap and avoids waiting for the next poll.
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([] { QueryArousal(true); });
        } else if (name == "SexLabDisabled") {
            sceneController.State().sexLab.active.store(false);
            sceneController.State().sexLab.valid.store(false);
            sceneController.State().sexLab.entryStateValid.store(false);
            sceneController.State().sexLab.roleGeneration.fetch_add(1);
            sceneController.State().sexLab.role.store(0);
            sceneController.State().sexLab.roleValid.store(false);
            sceneController.State().sexLab.roleQueryPending.store(false);
            sceneController.State().sexLab.lastTopMs.store(0);
            sceneController.State().sexLab.bottomCandidateSinceMs.store(0);
            ResetPPASceneTracking(2000);
            if (playerContext.physics.known.load() && !playerContext.physics.usingCBPC.load())
                softConfirmationDueMs.store(NowMs() + 250);
        } else if (name == "SexLabEnabled" || name == "SexLabGameLoaded") {
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { QuerySexLab(); RefreshDiagnostics(); });
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

class NiNodeSink final : public RE::BSTEventSink<SKSE::NiNodeUpdateEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const SKSE::NiNodeUpdateEvent* event, RE::BSTEventSource<SKSE::NiNodeUpdateEvent>*) override {
        const auto now = NowMs();
        bool enabled = false;
        { std::scoped_lock lock(settingsLock); enabled = settings.equipmentChangeRecovery; }
        if (enabled && event && event->reference == RE::PlayerCharacter::GetSingleton() &&
            now >= ignoreNodeEventsUntilMs.load())
            nodeRefreshDueMs.store(now + 750);
        return RE::BSEventNotifyControl::kContinue;
    }
};

class EquipEventSink final : public RE::BSTEventSink<RE::TESEquipEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event,
        RE::BSTEventSource<RE::TESEquipEvent>*) override {
        if (!event || event->actor.get() != RE::PlayerCharacter::GetSingleton())
            return RE::BSEventNotifyControl::kContinue;

        auto* changedForm = RE::TESForm::LookupByID(event->baseObject);
        if (!changedForm || changedForm->GetFormType() != RE::FormType::Armor)
            return RE::BSEventNotifyControl::kContinue;

        bool enabled = false;
        { std::scoped_lock lock(settingsLock); enabled = settings.equipmentChangeRecovery; }
        if (enabled) {
            // Some armour managers replace the genital mesh without emitting
            // an SKSE NiNode update. Debounce paired equip/unequip events and
            // run the same owner recovery after the replacement nodes settle.
            nodeRefreshDueMs.store(NowMs() + 1000);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        if (!event || event->menuName != RE::SleepWaitMenu::MENU_NAME)
            return RE::BSEventNotifyControl::kContinue;

        auto* calendar = RE::Calendar::GetSingleton();
        if (!calendar) return RE::BSEventNotifyControl::kContinue;
        if (event->opening) {
            sleepWaitStartedHours.store(calendar->GetHoursPassed());
            return RE::BSEventNotifyControl::kContinue;
        }

        const float started = sleepWaitStartedHours.exchange(-1.0F);
        bool allowMorning = false;
        { std::scoped_lock lock(settingsLock); allowMorning = settings.morningErections; }
        const float restedHours = started >= 0.0F ? calendar->GetHoursPassed() - started : 0.0F;
        if (allowMorning && restedHours >= 3.0F) {
            morningErectionDueMs.store(NowMs() + 1500);
            Record(fmt::format("Morning erection queued after resting for {:.1f} hours", restedHours));
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

void ResetTransientTimers() {
    playerContext.physics.lastSwitchMs.store(0);
    playerContext.physics.retryAfterMs.store(0);
    loadSMPResetDueMs.store(0);
    loadSMPResetRestoreDueMs.store(0);
    softHandoffResetDueMs.store(0);
    softHandoffResetUntilMs.store(0);
    softHandoffResetRestoreDueMs.store(0);
    softAngleRefreshDueMs.store(0);
    softAngleRefreshRestoreDueMs.store(0);
    bendSettleDueMs.store(0);
    bendConfirmationDueMs.store(0);
    bendRetryDueMs.store(0);
    softConfirmationDueMs.store(0);
    softConfirmationUntilMs.store(0);
    cbpcConfirmationDueMs.store(0);
    cbpcConfirmationUntilMs.store(0);
    nodeRefreshDueMs.store(0);
    nodeRefreshFollowupDueMs.store(0);
    nodeCBPCReacquireDueMs.store(0);
    nodeCBPCReacquireUntilMs.store(0);
    erectMeshReplayDueMs.store(0);
    nodeSMPResetRestoreDueMs.store(0);
    diagnosticsRefreshDueMs.store(0);
    manualPhysicsTestUntilMs.store(0);
    activeManualPhysicsTest.store(-1);
    pendingQuickAction.store(-1);
    pendingQuickActionUntilMs.store(0);
    startupReconcileDueMs.store(0);
    startupReconcileUntilMs.store(0);
    postSwitchVerificationDueMs.store(0);
    postSwitchVerificationUntilMs.store(0);
    externalOwnerRepairDueMs.store(0);
    externalOwnerRepairUntilMs.store(0);
    ignoreNodeEventsUntilMs.store(0);
}

ModEventSink modEventSink;
NiNodeSink niNodeSink;
EquipEventSink equipEventSink;
MenuSink menuSink;

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
        // Any callback still queued belongs to the old game state. Give the VM
        // time to finish rebuilding before SPS sends another scripted request.
        InvalidatePapyrusQueries(3000);
        arousalController.ResetReading();
        sceneController.State().sexLab.valid.store(false);
        sceneController.State().sexLab.roleValid.store(false);
        sceneController.State().sexLab.lastTopMs.store(0);
        sceneController.State().sexLab.bottomCandidateSinceMs.store(0);
        sceneController.State().ostim.active.store(false);
        sceneController.State().ostim.role.store(0);
        sceneController.State().ostim.roleValid.store(false);
        sceneController.State().ostim.entryStateValid.store(false);
        randomErectionNextMs.store(0);
        randomErectionUntilMs.store(0);
        randomErectionManualTest.store(false);
        spontaneousRefractoryUntilMs.store(0);
        morningErectionDueMs.store(0);
        morningErectionActive.store(false);
        sleepWaitStartedHours.store(-1.0F);
        CancelErectionAnimation();
        ResetTransientTimers();
        return;
    }
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Load();
        RegisterMenu();
        RegisterPPAAPI();
        if (auto* source = SKSE::GetModCallbackEventSource()) source->AddEventSink(&modEventSink);
        if (auto* source = SKSE::GetNiNodeUpdateEventSource()) source->AddEventSink(&niNodeSink);
        if (auto* source = RE::ScriptEventSourceHolder::GetSingleton()) source->AddEventSink(&equipEventSink);
        if (auto* ui = RE::UI::GetSingleton()) ui->AddEventSink(&menuSink);
        RefreshDiagnostics();
        return;
    }
    if (message->type == SKSE::MessagingInterface::kPostLoadGame ||
        message->type == SKSE::MessagingInterface::kNewGame) {
        // PreLoadGame already invalidated callbacks from the previous save.
        // A shorter post-load gate still lets the new VM finish binding while
        // avoiding an unnecessary multi-second delay before the first arousal
        // query and physics decision.
        InvalidatePapyrusQueries(1000);
        ClearAPIRequests();
        ResetTransientTimers();
        StartPolling();
        arousalController.ResetReading();
        sceneController.State().sexLab.valid.store(false);
        sceneController.State().sexLab.entryStateValid.store(false);
        sceneController.State().sexLab.role.store(0);
        sceneController.State().sexLab.roleValid.store(false);
        sceneController.State().sexLab.lastTopMs.store(0);
        sceneController.State().sexLab.bottomCandidateSinceMs.store(0);
        sceneController.State().ostim.active.store(false);
        sceneController.State().ostim.connected.store(false);
        sceneController.State().ostim.endedMs.store(0);
        sceneController.State().ostim.entryStateValid.store(false);
        sceneController.State().ostim.role.store(0);
        sceneController.State().ostim.roleValid.store(false);
        randomErectionNextMs.store(0);
        randomErectionUntilMs.store(0);
        randomErectionManualTest.store(false);
        spontaneousRefractoryUntilMs.store(0);
        morningErectionDueMs.store(0);
        morningErectionActive.store(false);
        sleepWaitStartedHours.store(-1.0F);
        CancelErectionAnimation();
        // The live genital skeleton can be replaced a few seconds after the
        // save has loaded. Replay an already-selected erect angle once after
        // that late replacement, without changing the user's physics mode.
        erectMeshReplayDueMs.store(NowMs() + 5000);
        diagnosticsRefreshDueMs.store(NowMs() + 2000);
        ResetPPASceneTracking(2000);
        playerContext.physics.known.store(false);
        startupReconcileDueMs.store(NowMs() + 1000);
        startupReconcileUntilMs.store(NowMs() + 30000);
        ScheduleLoadSMPReset();
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([] {
                Settings copy;
                { std::scoped_lock lock(settingsLock); copy = settings; }
                if (copy.mode == 0)
                    QueryArousal(true);
                else
                    ClearArousalError();
                QuerySexLab();
            });
        }
    }
}
}

extern "C" __declspec(dllexport) const SPS::API::InterfaceV1* __cdecl
SchlongPhysicsSwapper_GetAPI_V1() {
    static const SPS::API::InterfaceV1 api{
        SPS::API::kVersion,
        sizeof(SPS::API::InterfaceV1),
        Mod::APIGetCapabilities,
        Mod::APIIsActorSupported,
        Mod::APIGetState,
        Mod::APIRequestPhysics,
        Mod::APIReleasePhysics,
        Mod::APINotifyPhysicsReset,
        Mod::APIRegisterStateListener,
        Mod::APIUnregisterStateListener
    };
    return &api;
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    Mod::runtimeVersion = skse->RuntimeVersion().string(".");
    Mod::skseVersion = REL::Version::unpack(skse->SKSEVersion()).string(".");
    SKSE::Init(skse);
    if (auto dir = SKSE::log::log_directory()) {
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>((*dir / "SchlongPhysicsSwapper.log").string(), true);
        spdlog::set_default_logger(std::make_shared<spdlog::logger>("global", std::move(sink)));
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
    }
    Mod::ConfigureControllers();
    SKSE::GetMessagingInterface()->RegisterListener(Mod::OnMessage);
    logger::info("Schlong Physics Swapper {} loaded", Mod::kVersion);
    return true;
}
