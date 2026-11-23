#include <RE/Skyrim.h>
#include <REL/Version.h>
#include <SKSE/SKSE.h>
#include <SKSEMenuFramework.h>
#include <fmt/format.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/ostream_sink.h>
#include <Windows.h>
#include "PPAInterface.h"
#include "PhysicsDecision.h"
#include "SettingsStore.h"
#include "SPSAPI.h"
#include "api/ExternalControlRegistry.h"
#include "controllers/ArousalController.h"
#include "controllers/ActorContext.h"
#include "controllers/PhysicsOwnershipController.h"
#include "controllers/PositionController.h"
#include "controllers/RecoveryController.h"
#include "controllers/SceneController.h"
#include "controllers/SceneState.h"
#include "diagnostics/ActivityLog.h"
#include "diagnostics/Diagnostics.h"
#include "diagnostics/SupportReport.h"
#include "runtime/Compatibility.h"
#include "runtime/ActorBridgeAdapter.h"
#include "runtime/ActorIdentity.h"
#include "runtime/ApiActorReads.h"
#include "runtime/BasicNpcRuntime.h"
#include "runtime/ManagedNpcRuntime.h"
#include "runtime/PapyrusGateway.h"
#include "runtime/PhysicsOwnershipAdapter.h"
#include "runtime/PositionBackendAdapter.h"
#include "runtime/ShaftPoseTracker.h"
#include "runtime/SosPoseAngle.h"
#include "controllers/PoseTargets.h"

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
#include <shellapi.h>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace logger = SKSE::log;

namespace Mod {
namespace fs = std::filesystem;

constexpr auto kName = "Schlong Physics Swapper";
constexpr auto kVersion = "2.3.0.0";
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
using SPS::Runtime::SoftbodyLoaded;
using SPS::Runtime::SosAeNativeLoaded;
using SPS::Runtime::SosAeNativeModuleName;
using SPS::Runtime::TngLoaded;
Settings settings;
bool settingsLoaded{ false };  // protected by settingsLock
Diagnostics diagnostics;
std::mutex settingsLock;
std::mutex diagnosticsLock;
SPS::Diagnostics::ActivityLog activityLog;
std::string runtimeVersion{ "unknown" };
std::string skseVersion{ "unknown" };
std::mutex debugCaptureLock;
std::vector<std::string> debugCaptureLines;
SPS::APIControl::ExternalControlRegistry apiRegistry;
// V4: a GetActorSceneRole call keeps the scene role queries running for 30 s even
// when the user's own role switching is off, so the answer is not always Unknown.
std::atomic<std::int64_t> apiSceneRoleInterestUntilMs{ 0 };
// Last scene state sent to V4 actor listeners (packed: bit 0 inScene, bits 1-2 role).
std::atomic<int> apiLastSceneEventState{ -1 };

SPS::Controllers::ArousalController arousalController;
SPS::Controllers::ActorWorkOwner playerArousalWriteOwner{ SPS::API::kPlayerFormID, {}, true };
SPS::Controllers::ActorContext playerContext{ 0x14 };
SPS::Controllers::PhysicsOwnershipController ownershipController{ playerContext.physics };
SPS::Controllers::PositionController positionController{ playerContext.position };
SPS::Controllers::RecoveryController recoveryController{ playerContext };
SPS::Controllers::SceneController sceneController;
std::atomic<bool> polling{ false };
std::atomic<bool> tickQueued{ false };
std::atomic<std::int64_t> diagnosticsRefreshDueMs{ 0 };
std::atomic<std::int64_t> playerBonesRefreshDueMs{ 0 };
// Set by an armour change; the equipment follow-up spends it on one more soft
// SMP reset after the late genital-node rebuild some outfits cause.
std::atomic<bool> equipSecondResetPending{ false };
// The angle the player held when a node refresh reset the applied bend. A
// morph update (for example an arousal morph addon) raises the same player
// NiNode event as a real mesh change, and the reconnect that follows used to
// restart the rise from SOSBend-9 or the fall from the top. Remembering the
// held angle lets the reconnect continue from it instead of visibly dropping.
std::atomic<int> heldBendBeforeReconnect{ -1 };
std::atomic<std::int64_t> heldBendUntilMs{ 0 };
constexpr std::int64_t kHeldBendWindowMs = 20000;
std::atomic<std::int64_t> manualPhysicsTestUntilMs{ 0 };
std::atomic<int> activeManualPhysicsTest{ -1 };  // -1 none, 0 SMP, 1 CBPC
std::atomic<int> pendingQuickAction{ -1 };       // -1 none, 0 SMP test, 1 CBPC test, 2 repair
std::atomic<std::int64_t> pendingQuickActionUntilMs{ 0 };
std::atomic<std::int64_t> papyrusDispatchAllowedAfterMs{ 0 };
std::atomic<std::int64_t> debugCaptureStartedMs{ 0 };
std::atomic<std::int64_t> debugCaptureUntilMs{ 0 };
std::atomic<std::uint64_t> debugCaptureGeneration{ 0 };
std::atomic<unsigned> externalResetNotices{ 0 };
std::atomic<unsigned> ownerRestorations{ 0 };
std::atomic<bool> fsmpCompatibilityWarningShown{ false };
std::atomic<bool> playerArousalWriteRequested{ false };
std::atomic<bool> playerArousalWriteInFlight{ false };
std::atomic<float> requestedPlayerArousal{ 0.0F };
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
void RefreshPlayerBoneDiagnostics();
void Save(bool clearLegacyMarker);
bool PapyrusReadyForDispatch();
const char* PapyrusStatusText();
bool SexLabHasPriority(const Settings& copy);
bool OStimHasPriority(const Settings& copy);
bool AnySceneHasPriority(const Settings& copy);
void ApplyRequestedBend(bool force, bool animate, bool automatic);
void ApplyRequestedSoftBend(bool force, bool animate);
void ScheduleSoftAngleRefresh(int delayMs = 350);
void ScheduleSoftPoseResetAfterScene(std::int64_t sceneEndedMs);
void RunSoftAngleRefresh();
void CancelErectionAnimation();
void StartGradualRelaxation(const Settings& copy);
bool ClearResolvedPositionError();
std::string BuildReport();
std::int64_t NowMs();
void Record(std::string message, bool error = false);
std::optional<SPS::APIControl::Request> ActiveAPIRequest();
void ClearAPIRequests();
void ClearNpcEventState();
void ScheduleExternalOwnerRepair(int delayMs, std::string_view reason);
bool SetOwner(bool cbpc, bool force = false);
bool QueueOwnershipHandoff(
    bool cbpc,
    SPS::Controllers::OwnershipPurpose purpose,
    bool softTransition = false);
void FinishOwnershipHandoff(std::uint64_t generation, bool success);
void ReportSmpDrift(const SPS::Controllers::OwnershipCompletion& completion);
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
    return SPS::Runtime::NowMs();
}

void ScheduleExternalOwnerRepair(int delayMs, std::string_view reason) {
    if (!playerContext.physics.known.load()) return;
    recoveryController.ScheduleExternalOwnerRepair(NowMs(), delayMs);
    ++externalResetNotices;
    logger::info("Physics owner re-check scheduled after {}", reason);
}

std::optional<SPS::APIControl::Request> ActiveAPIRequest() {
    return apiRegistry.Active(NowMs());
}

void ClearAPIRequests() {
    const auto cleared = apiRegistry.Clear() + apiRegistry.ClearRiseHolds();
    apiLastSceneEventState.store(-1);
    ClearNpcEventState();
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

std::uint32_t __cdecl APIGetCapabilitiesV2() {
    using SPS::API::Capability;
    return APIGetCapabilities() |
        static_cast<std::uint32_t>(Capability::LiveArousalReadback) |
        static_cast<std::uint32_t>(Capability::ErectThresholdReadback);
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

SPS::API::Result __cdecl APIGetArousal(std::uint32_t actorFormID, float* arousal) {
    if (!arousal) return SPS::API::Result::InvalidArgument;
    if (!APIIsActorSupported(actorFormID)) return SPS::API::Result::UnsupportedActor;
    const auto reading = arousalController.Reading();
    if (!reading) return SPS::API::Result::NotReady;
    *arousal = *reading;
    return SPS::API::Result::Success;
}

std::uint32_t __cdecl APIGetCapabilitiesV3() {
    using SPS::API::Capability;
    return APIGetCapabilitiesV2() |
        static_cast<std::uint32_t>(Capability::ActorArousalReadback) |
        static_cast<std::uint32_t>(Capability::ActorSceneReadback);
}

// V3. Any loaded actor. The player is answered from SPS's own state; every other
// actor is read through the arousal provider. Game thread only.
SPS::API::Result __cdecl APIGetActorArousal(std::uint32_t actorFormID, float* arousal) {
    if (!arousal) return SPS::API::Result::InvalidArgument;
    if (actorFormID == SPS::API::kPlayerFormID) return APIGetArousal(actorFormID, arousal);
    switch (SPS::Runtime::ReadApiActorArousal(actorFormID, arousal)) {
    case SPS::Runtime::ApiActorRead::ready: return SPS::API::Result::Success;
    case SPS::Runtime::ApiActorRead::notReady: return SPS::API::Result::NotReady;
    default: return SPS::API::Result::UnsupportedActor;
    }
}

// True during a SexLab/OStim scene, while Accurate Penetration is reporting one,
// and for the scene-end delay afterwards. Reads only scene state: it does not
// depend on who controls physics, so a rise hold, a RequestPhysics hold or an
// SPS-owned handoff never hides a scene.
bool PlayerInSceneForAPI(int delayMs) {
    const auto now = NowMs();
    const auto& state = sceneController.State();
    const auto recent = [&](std::int64_t endedMs) {
        return endedMs > 0 && now - endedMs < delayMs;
    };
    const bool ppaScene = state.ppa.sceneActive.load() && now - state.ppa.lastUpdateMs.load() < 3000;
    return state.sexLab.active.load() || state.ostim.active.load() || ppaScene ||
        recent(state.sexLab.endedMs.load()) || recent(state.ostim.endedMs.load());
}

SPS::API::SceneRole PlayerSceneRoleForAPI() {
    using SPS::API::SceneRole;
    const auto toRole = [](int value) {
        return value == 1 ? SceneRole::Receiving : value == 2 ? SceneRole::Penetrating : SceneRole::Unknown;
    };
    const auto now = NowMs();
    const auto& state = sceneController.State();
    if (state.ostim.active.load() && state.ostim.roleValid.load()) return toRole(state.ostim.role.load());
    if (state.sexLab.active.load() && state.sexLab.roleValid.load()) return toRole(state.sexLab.role.load());
    if (state.ppa.sceneActive.load() && state.ppa.sceneRoleValid.load() &&
        now - state.ppa.lastUpdateMs.load() < 3000)
        return toRole(state.ppa.sceneRole.load());
    return SceneRole::Unknown;
}

// V3. True during a scene and for the scene-end delay afterwards.
SPS::API::Result __cdecl APIIsActorInScene(std::uint32_t actorFormID, bool* inScene) {
    if (!inScene) return SPS::API::Result::InvalidArgument;
    int delayMs;
    { std::scoped_lock lock(settingsLock); delayMs = settings.sceneEndDelayMs; }
    if (actorFormID == SPS::API::kPlayerFormID) {
        *inScene = PlayerInSceneForAPI(delayMs);
        return SPS::API::Result::Success;
    }
    switch (SPS::Runtime::ReadApiActorScene(actorFormID, inScene, delayMs,
            papyrusDispatchAllowedAfterMs.load())) {
    case SPS::Runtime::ApiActorRead::ready: return SPS::API::Result::Success;
    case SPS::Runtime::ApiActorRead::notReady: return SPS::API::Result::NotReady;
    default: return SPS::API::Result::UnsupportedActor;
    }
}

std::uint32_t __cdecl APIGetCapabilitiesV4() {
    return APIGetCapabilitiesV3() | SPS::API::kCapabilitySceneRoleReadback |
        SPS::API::kCapabilityRiseHold | SPS::API::kCapabilityActorEvents;
}

// V4. Player only. Asking keeps the scene role queries running for 30 s, so the
// answer does not depend on the user's own role-switching setting.
SPS::API::Result __cdecl APIGetActorSceneRole(std::uint32_t actorFormID, SPS::API::SceneRole* role) {
    if (!role) return SPS::API::Result::InvalidArgument;
    if (actorFormID != SPS::API::kPlayerFormID) return SPS::API::Result::UnsupportedActor;
    apiSceneRoleInterestUntilMs.store(NowMs() + 30000);
    *role = PlayerSceneRoleForAPI();
    return SPS::API::Result::Success;
}

bool ApiWantsSceneRole() {
    return apiRegistry.HasActorListeners() || NowMs() < apiSceneRoleInterestUntilMs.load();
}

void DispatchActorEvent(SPS::API::ActorEventKind kind) {
    const auto listeners = apiRegistry.ActorListeners();
    if (listeners.empty()) return;
    int delayMs;
    { std::scoped_lock lock(settingsLock); delayMs = settings.sceneEndDelayMs; }
    SPS::API::ActorEvent event;
    event.kind = kind;
    const auto reading = arousalController.Reading();
    event.arousalValid = reading ? 1u : 0u;
    event.arousal = reading ? *reading : 0.0F;
    event.inScene = PlayerInSceneForAPI(delayMs) ? 1u : 0u;
    event.role = PlayerSceneRoleForAPI();
    for (const auto& listener : listeners) {
        if (!listener.callback) continue;
        try {
            listener.callback(&event, listener.userData);
        } catch (...) {
            Record("SPS-API-002: A compatibility API actor listener threw an exception", true);
        }
    }
}

int PackedSceneStateForAPI() {
    int delayMs;
    { std::scoped_lock lock(settingsLock); delayMs = settings.sceneEndDelayMs; }
    return (PlayerInSceneForAPI(delayMs) ? 1 : 0) | (static_cast<int>(PlayerSceneRoleForAPI()) << 1);
}

// Called from the poll. Sends SceneChanged when inScene or the role differs from
// what listeners were last told.
void NotifyAPISceneChangeIfNeeded() {
    if (!apiRegistry.HasActorListeners()) {
        apiLastSceneEventState.store(-1);
        return;
    }
    const int packed = PackedSceneStateForAPI();
    const int previous = apiLastSceneEventState.exchange(packed);
    if (previous != -1 && previous != packed) DispatchActorEvent(SPS::API::ActorEventKind::SceneChanged);
}

// V4. Runs on the game thread just before SPS evaluates a changed arousal value.
void NotifyAPIArousalChanged() {
    DispatchActorEvent(SPS::API::ActorEventKind::ArousalChanged);
}

SPS::API::RequestHandle __cdecl APIRequestRiseHold(const SPS::API::RiseHoldRequest* request) {
    if (!request || request->apiVersion != SPS::API::kVersionV4 ||
        request->size < sizeof(SPS::API::RiseHoldRequest) ||
        !APIIsActorSupported(request->actorFormID) || request->durationMilliseconds == 0)
        return 0;
    {
        std::scoped_lock lock(settingsLock);
        if (!settings.enabled) return 0;
    }
    const auto stored = apiRegistry.AddRiseHold(*request, NowMs());
    logger::info("Compatibility API: {} asked SPS to wait before the CBPC rise for up to {} ms",
        stored.requester,
        (std::min)(request->durationMilliseconds, SPS::API::kMaxRiseHoldMilliseconds));
    return stored.handle;
}

SPS::API::Result __cdecl APIReleaseRiseHold(SPS::API::RequestHandle request) {
    if (request == 0) return SPS::API::Result::InvalidArgument;
    const auto requester = apiRegistry.ReleaseRiseHold(request);
    if (!requester) return SPS::API::Result::NotFound;
    logger::info("Compatibility API: {} released its rise hold", *requester);
    // The rise the hold was delaying can start now.
    QueueAPIEvaluation(false);
    return SPS::API::Result::Success;
}

SPS::API::ListenerHandle __cdecl APIRegisterActorEventListener(
    SPS::API::ActorEventCallback callback, void* userData) {
    const auto handle = apiRegistry.RegisterActorListener(callback, userData);
    // Baseline for SceneChanged: the listener reads the current scene itself.
    if (handle != 0) apiLastSceneEventState.store(PackedSceneStateForAPI());
    return handle;
}

SPS::API::Result __cdecl APIUnregisterActorEventListener(SPS::API::ListenerHandle listener) {
    if (listener == 0) return SPS::API::Result::InvalidArgument;
    return apiRegistry.UnregisterActorListener(listener) ?
        SPS::API::Result::Success : SPS::API::Result::NotFound;
}

// ---- V5: read-only NPC state, scene role and events -------------------------

std::uint32_t __cdecl APIGetCapabilitiesV5() {
    return APIGetCapabilitiesV4() | SPS::API::kCapabilityActorState |
        SPS::API::kCapabilityActorSceneRole | SPS::API::kCapabilityNpcEvents;
}

SPS::API::SceneRole ApiRoleFromInt(int value) {
    return value == 1 ? SPS::API::SceneRole::Receiving :
        value == 2 ? SPS::API::SceneRole::Penetrating : SPS::API::SceneRole::Unknown;
}

// V5. The player, or a loaded NPC (cached; the first NPC call is NotReady).
SPS::API::Result __cdecl APIGetAnyActorSceneRole(std::uint32_t actorFormID, SPS::API::SceneRole* role) {
    if (!role) return SPS::API::Result::InvalidArgument;
    if (actorFormID == SPS::API::kPlayerFormID) return APIGetActorSceneRole(actorFormID, role);
    int value = 0;
    switch (SPS::Runtime::ReadApiActorSceneRole(actorFormID, &value, papyrusDispatchAllowedAfterMs.load())) {
    case SPS::Runtime::ApiActorRead::ready:
        *role = ApiRoleFromInt(value);
        return SPS::API::Result::Success;
    case SPS::Runtime::ApiActorRead::notReady: return SPS::API::Result::NotReady;
    default: return SPS::API::Result::UnsupportedActor;
    }
}

// V5. Player or any loaded actor. validFlags says which fields were readable.
SPS::API::Result __cdecl APIGetActorState(std::uint32_t actorFormID, SPS::API::ActorStateSnapshot* snapshot) {
    using namespace SPS::API;
    if (!snapshot || snapshot->apiVersion != kVersionV5 || snapshot->size < sizeof(ActorStateSnapshot))
        return Result::InvalidArgument;
    ActorStateSnapshot out;
    out.actorFormID = actorFormID;
    int delayMs;
    { std::scoped_lock lock(settingsLock); delayMs = settings.sceneEndDelayMs; }
    if (actorFormID == kPlayerFormID) {
        out.managed = 1;
        if (playerContext.physics.known.load()) {
            out.state = playerContext.physics.usingCBPC.load() ? PhysicsState::CBPC : PhysicsState::SMP;
            out.validFlags |= kStatePhysicsValid;
        }
        out.source = CurrentAPIControlSource();
        if (const auto reading = arousalController.Reading()) {
            out.arousal = *reading;
            out.validFlags |= kStateArousalValid;
        }
        out.inScene = PlayerInSceneForAPI(delayMs) ? 1u : 0u;
        out.role = PlayerSceneRoleForAPI();
        out.validFlags |= kStateSceneValid | kStateRoleValid;
    } else {
        auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormID);
        if (!actor || actor->IsDeleted() || actor->IsDisabled() || !actor->Is3DLoaded())
            return Result::UnsupportedActor;
        if (const auto managed = SPS::Runtime::ReadManagedNpcApiState(actorFormID)) {
            out.managed = 1;
            if (managed->physicsKnown) {
                out.state = managed->usingCBPC ? PhysicsState::CBPC : PhysicsState::SMP;
                out.source = ControlSource::SPS;
                out.validFlags |= kStatePhysicsValid;
            }
            if (managed->arousal) {
                out.arousal = *managed->arousal;
                out.validFlags |= kStateArousalValid;
            }
        }
        if (!(out.validFlags & kStateArousalValid)) {
            float arousal = 0.0F;
            if (SPS::Runtime::ReadApiActorArousal(actorFormID, &arousal) == SPS::Runtime::ApiActorRead::ready) {
                out.arousal = arousal;
                out.validFlags |= kStateArousalValid;
            }
        }
        bool inScene = false;
        if (SPS::Runtime::ReadApiActorScene(actorFormID, &inScene, delayMs,
                papyrusDispatchAllowedAfterMs.load()) == SPS::Runtime::ApiActorRead::ready) {
            out.inScene = inScene ? 1u : 0u;
            out.validFlags |= kStateSceneValid;
        }
        int role = 0;
        if (SPS::Runtime::ReadApiActorSceneRole(actorFormID, &role,
                papyrusDispatchAllowedAfterMs.load()) == SPS::Runtime::ApiActorRead::ready) {
            out.role = ApiRoleFromInt(role);
            out.validFlags |= kStateRoleValid;
        }
    }
    *snapshot = out;
    return Result::Success;
}

struct NpcEventLast {
    bool arousalValid{ false };
    float arousal{ 0.0F };
    int scene{ -1 };  // packed inScene | role << 1; -1 until first read
};
std::unordered_map<std::uint32_t, NpcEventLast> npcEventLast;

void ClearNpcEventState() {
    npcEventLast.clear();
}

void DispatchNpcEvent(const std::vector<SPS::APIControl::ActorListener>& listeners,
    const SPS::API::ActorEvent& event) {
    for (const auto& listener : listeners) {
        if (!listener.callback) continue;
        try {
            listener.callback(&event, listener.userData);
        } catch (...) {
            Record("SPS-API-003: A compatibility API NPC listener threw an exception", true);
        }
    }
}

// Runs once per service tick for each managed NPC while NPC listeners exist.
// Game thread. Raises ArousalChanged and SceneChanged.
void NpcEventTick(std::uint32_t formID, std::optional<float> arousal, std::int64_t allowedAfterMs) {
    const auto listeners = apiRegistry.NpcListeners();
    if (listeners.empty()) return;
    int delayMs;
    { std::scoped_lock lock(settingsLock); delayMs = settings.sceneEndDelayMs; }
    auto& last = npcEventLast[formID];

    bool inScene = false;
    const bool sceneReady = SPS::Runtime::ReadApiActorScene(formID, &inScene, delayMs, allowedAfterMs) ==
        SPS::Runtime::ApiActorRead::ready;
    int role = 0;
    // A scene with no SexLab/OStim has no role; treat that as Unknown, not pending.
    const bool roleReady = SPS::Runtime::ReadApiActorSceneRole(formID, &role, allowedAfterMs) ==
        SPS::Runtime::ApiActorRead::ready;
    if (!roleReady) role = 0;

    SPS::API::ActorEvent event;
    event.actorFormID = formID;
    event.arousalValid = arousal ? 1u : 0u;
    event.arousal = arousal ? *arousal : 0.0F;
    event.inScene = (sceneReady && inScene) ? 1u : 0u;
    event.role = ApiRoleFromInt(role);

    if (arousal && (!last.arousalValid || std::fabs(*arousal - last.arousal) >= 0.5F)) {
        last.arousalValid = true;
        last.arousal = *arousal;
        event.kind = SPS::API::ActorEventKind::ArousalChanged;
        DispatchNpcEvent(listeners, event);
    }
    if (sceneReady) {
        const int packed = (inScene ? 1 : 0) | (role << 1);
        const int previous = last.scene;
        last.scene = packed;
        if (previous != -1 && previous != packed) {
            event.kind = SPS::API::ActorEventKind::SceneChanged;
            DispatchNpcEvent(listeners, event);
        }
    }
}

SPS::API::ListenerHandle __cdecl APIRegisterNpcEventListener(
    SPS::API::ActorEventCallback callback, void* userData) {
    const auto handle = apiRegistry.RegisterNpcListener(callback, userData);
    if (handle != 0) SPS::Runtime::SetManagedNpcTickSink(NpcEventTick);
    return handle;
}

SPS::API::Result __cdecl APIUnregisterNpcEventListener(SPS::API::ListenerHandle listener) {
    if (listener == 0) return SPS::API::Result::InvalidArgument;
    if (!apiRegistry.UnregisterNpcListener(listener)) return SPS::API::Result::NotFound;
    if (!apiRegistry.HasNpcListeners()) {
        SPS::Runtime::SetManagedNpcTickSink({});
        npcEventLast.clear();
    }
    return SPS::API::Result::Success;
}

SPS::API::Result __cdecl APIGetErectThreshold(float* threshold) {
    if (!threshold) return SPS::API::Result::InvalidArgument;
    std::scoped_lock lock(settingsLock);
    if (!settingsLoaded) return SPS::API::Result::NotReady;
    *threshold = settings.threshold;
    return SPS::API::Result::Success;
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
        [] { Evaluate(); },
        [] { NotifyAPIArousalChanged(); });
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
            if (!previous && active && SoftbodyLoaded())
                ScheduleExternalOwnerRepair(3000, "the player's SOFTBODY scene start");
            if (previous && !active && (!playerContext.physics.known.load() ||
                !playerContext.physics.usingCBPC.load())) {
                playerContext.recovery.softConfirmationDueMs.store(now + 250);
                ScheduleSoftPoseResetAfterScene(now);
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
                    playerContext.position.settleDueMs.store(now + copy.settleDelayMs);
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

std::atomic<std::int64_t> shaftSampleStartMs{0};

// Diagnostic: after an SMP reset, log the local rotation (degrees, XYZ) of the six
// shaft bones once per poll for 15 s, to show whether the bones move after the reset.
void SampleShaftAfterReset() {
    const auto start = shaftSampleStartMs.load();
    if (start <= 0) return;
    const auto elapsed = NowMs() - start;
    if (elapsed > 15000) { shaftSampleStartMs.store(0); return; }
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* root = player ? player->Get3D() : nullptr;
    if (!root) return;
    static constexpr const char* names[6] = {
        "NPC Genitals01 [Gen01]", "NPC Genitals02 [Gen02]", "NPC Genitals03 [Gen03]",
        "NPC Genitals04 [Gen04]", "NPC Genitals05 [Gen05]", "NPC Genitals06 [Gen06]" };
    std::string line = fmt::format("Shaft sample +{:.1f}s:", elapsed / 1000.0);
    for (int i = 0; i < 6; ++i) {
        auto* node = root->GetObjectByName(RE::BSFixedString(names[i]));
        if (!node) { line += fmt::format(" G{}=none", i + 1); continue; }
        float x = 0, y = 0, z = 0;
        node->local.rotate.ToEulerAnglesXYZ(x, y, z);
        constexpr float deg = 57.2957795f;
        line += fmt::format(" G{}=({:.1f},{:.1f},{:.1f}) s={:.2f}", i + 1, x * deg, y * deg, z * deg, node->local.scale);
    }
    Record(line);
}

// True when the six shaft bones' local rotations have changed by less than
// 0.5 degrees since the previous call (at least ~400 ms apart) twice in a row.
// SMP rebuilds its bodies from the skeleton as it is at reset time, so a reset
// issued while the resting clip is still blending bakes a half-blended pose.
// Returns true when the bones cannot be read, so a missing node never blocks it.
bool ShaftPoseStable() {
    static SPS::Runtime::ShaftPoseTracker tracker;
    auto* player = RE::PlayerCharacter::GetSingleton();
    return tracker.Stable(player ? player->Get3D() : nullptr, NowMs());
}

void CaptureState(std::string_view reason) {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const auto* vm = SPS::Runtime::VM();
    const auto line = fmt::format(
        "{} | engine={} arousal={:.1f}/{} provider={} connected={} SexLab={} SexLabRole={} OStim={} OStimRole={} SMP={} CBPC={} bend={}/{} method={} animating={} guard={} suspended={} papyrus={} vmOverstressed={}",
        reason, playerContext.physics.known.load() ? (playerContext.physics.usingCBPC.load() ? "CBPC" : "SMP") : "unknown",
        arousalController.Value(), arousalController.Valid(), ArousalProviderName(), arousalController.Connected(), sceneController.State().sexLab.active.load(),
        SexLabRoleName(sceneController.State().sexLab.role.load()), sceneController.State().ostim.active.load(), OStimRoleName(sceneController.State().ostim.role.load()),
        playerContext.physics.smpConnected.load(), playerContext.physics.cbpcConnected.load(), playerContext.position.requestedBend.load(), playerContext.position.appliedBend.load(),
        playerContext.position.lastMethod.load(), playerContext.position.animating.load(), NowMs() < playerContext.position.guardUntilMs.load(),
        playerContext.position.automaticSuspended.load(), PapyrusStatusText(), vm && vm->overstressed);
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

const char* PapyrusStatusText() {
    const auto* reason = SPS::Runtime::PapyrusWaitReason(papyrusDispatchAllowedAfterMs.load(), NowMs());
    return reason ? reason : "ready";
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

bool CallSosAeBend(RE::Actor* actor, int bend) {
    // SOS AE 4.0.0 removed SOSAE_SKSE.SetSchlongBend. The angle now comes from
    // SosPoseAngle, which writes the base of the assembly directly and needs no
    // SOS native, so this is a pose write rather than a Papyrus dispatch.
    return SPS::Runtime::SosPose::Apply(actor, bend,
        SPS::Runtime::SosPose::ConfiguredDegrees());
}

// True when something can actually produce a chosen soft angle. The SOS native
// used to be the only source; the SPS pose angle replaces it and works with any
// skeleton that has the base bone, with or without SOS AE installed.
bool SoftAngleControlAvailable(int bendMethod) {
    return bendMethod != SPS::Runtime::kBendMethodEvents || SosAeNativeLoaded();
}

bool ConfirmCurrentPhysicsOwner(std::string_view reason) {
    if (!PapyrusReadyForDispatch()) return false;
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.enabled || !playerContext.physics.known.load()) return false;
    const bool expectCBPC = playerContext.physics.usingCBPC.load();
    const bool queued = QueueOwnershipHandoff(
        expectCBPC, SPS::Controllers::OwnershipPurpose::restore);
    if (!queued) {
        logger::warn("Could not queue {} restoration after {}",
            expectCBPC ? "CBPC" : "SMP", reason);
    }
    return queued;
}

void Load() {
    const auto result = SPS::Core::LoadSettings(kIni, kLegacyIni);
    {
        std::scoped_lock lock(settingsLock);
        settings = result.settings;
        settingsLoaded = true;
    }
    SPS::Runtime::ConfigureBasicNpcCommands(result.settings.enabled && result.settings.basicNpcEnabled &&
        result.settings.basicNpcProviderSosDisabled);
    // Managed commands are permission-gated the same way. Without this, a saved
    // ManagedNPCEnabled=true stayed inactive until the checkbox was toggled.
    SPS::Runtime::ConfigureManagedNpcCommands(result.settings.enabled && result.settings.managedNpcEnabled);
    spdlog::set_level(settings.verboseLogging ? spdlog::level::debug : spdlog::level::info);
    if (result.shouldWriteCurrent) {
        // Onboarding or migration writes the current file and consumes the
        // one-shot import marker so the next launch reads the current file.
        Save(result.dropLegacyMarker);
    }
    if (result.migratedLegacy) {
        Record("Existing UBE Physics Switch settings migrated to Schlong Physics Swapper");
    }
}

void Save(bool clearLegacyMarker) {
    Settings copy;
    {
        std::scoped_lock lock(settingsLock);
        copy = settings;
    }
    if (!SPS::Core::SaveSettings(kIni, copy, clearLegacyMarker)) {
        logger::error("Failed to save SPS settings to {}", kIni);
    }
}

void QueryArousal(bool force = false) {
    arousalController.Query(force);
}

void RequestPlayerOslArousalWrite(float value) {
    if (!SPS::Core::ValidArousalReading(value) || !OslArousedLoaded()) return;
    requestedPlayerArousal.store(value);
    playerArousalWriteRequested.store(true);
}

void ServicePlayerOslArousalWrite() {
    if (!playerArousalWriteRequested.load() || playerArousalWriteInFlight.load()) return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    const float requested = requestedPlayerArousal.load();
    if (SPS::Runtime::QueueActorArousalWrite(playerArousalWriteOwner.Lease(), player, requested,
            papyrusDispatchAllowedAfterMs.load(), [requested](auto, auto result) {
                playerArousalWriteInFlight.store(false);
                if (!result) {
                    if (std::abs(requestedPlayerArousal.load() - requested) < 0.01F)
                        playerArousalWriteRequested.store(false);
                    Record("OSL Aroused did not accept the requested player arousal value", true);
                    return;
                }
                arousalController.AcceptExternalReading(*result);
                if (std::abs(requestedPlayerArousal.load() - requested) < 0.01F)
                    playerArousalWriteRequested.store(false);
                Record(fmt::format("OSL player arousal set to {:.1f}", *result));
            }))
        playerArousalWriteInFlight.store(true);
}

void QuerySexLab() {
    sceneController.QuerySexLab();
}

bool RandomErectionActive() {
    const auto until = playerContext.spontaneous.randomUntilMs.load();
    return until > 0 && NowMs() < until;
}

void StartRefractoryPeriod(const Settings& copy, std::int64_t now = 0) {
    if (!copy.spontaneousRefractory) {
        playerContext.spontaneous.refractoryUntilMs.store(0);
        return;
    }
    if (now == 0) now = NowMs();
    playerContext.spontaneous.refractoryUntilMs.store(now +
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
        playerContext.spontaneous.randomNextMs.store(0);
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
    playerContext.spontaneous.randomNextMs.store(std::max(chosenTime, playerContext.spontaneous.refractoryUntilMs.load()));
    logger::info("Next random erection check scheduled in {} minutes", chosenMinutes);
}

void UpdateRandomErection(const Settings& copy) {
    const auto now = NowMs();
    const bool spontaneousEnabled = copy.randomErections || copy.morningErections;
    const bool blocked = !spontaneousEnabled || copy.mode != 0 ||
        AnySceneHasPriority(copy) || ActiveAPIRequest().has_value();
    if (blocked) {
        const bool wasActive = playerContext.spontaneous.randomUntilMs.exchange(0) > now;
        if (wasActive) {
            StartRefractoryPeriod(copy, now);
            Record("Spontaneous erection stopped because another SPS control took priority");
        }
        if (!copy.randomErections) playerContext.spontaneous.randomNextMs.store(0);
        playerContext.spontaneous.morningActive.store(false);
        playerContext.spontaneous.randomManualTest.store(false);
        return;
    }

    if ((copy.randomErectionSafeMoments || playerContext.spontaneous.morningActive.load()) &&
        !playerContext.spontaneous.randomManualTest.load() &&
        RandomErectionActivityBlocked()) {
        const bool wasActive = playerContext.spontaneous.randomUntilMs.exchange(0) > now;
        if (wasActive) StartRefractoryPeriod(copy, now);
        playerContext.spontaneous.morningActive.store(false);
        const auto retryAt = now + 60000;
        const auto next = playerContext.spontaneous.randomNextMs.load();
        if (copy.randomErections && (next == 0 || next < retryAt))
            playerContext.spontaneous.randomNextMs.store(retryAt);
        if (wasActive)
            Record("Spontaneous erection ended because gameplay was interrupted");
        return;
    }

    // Keep CBPC in control until the downward movement has completed. Without
    // this hold, the normal arousal decision immediately hands the bones back
    // to SMP and the random erection visibly snaps down.
    if (playerContext.position.relaxing.load()) return;

    const auto until = playerContext.spontaneous.randomUntilMs.load();
    if (until > 0) {
        if (now < until) return;
        playerContext.spontaneous.randomUntilMs.store(0);
        playerContext.spontaneous.randomManualTest.store(false);
        playerContext.spontaneous.morningActive.store(false);
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

    const auto morningDue = playerContext.spontaneous.morningDueMs.load();
    if (copy.morningErections && morningDue > 0 && now >= morningDue &&
        now >= playerContext.spontaneous.refractoryUntilMs.load()) {
        playerContext.spontaneous.morningDueMs.store(0);
        if (playerContext.physics.known.load() && !playerContext.physics.usingCBPC.load()) {
            playerContext.spontaneous.morningActive.store(true);
            playerContext.spontaneous.randomUntilMs.store(now +
                static_cast<std::int64_t>(copy.morningErectionDurationSeconds) * 1000);
            Record("A morning erection started after resting");
            return;
        }
    }

    const auto next = playerContext.spontaneous.randomNextMs.load();
    if (!copy.randomErections) return;
    if (next == 0) {
        ScheduleNextRandomErection(copy, now);
        return;
    }
    if (now < next || now < playerContext.spontaneous.refractoryUntilMs.load()) return;

    // Do not waste a random event while the player is already erect. Pick a
    // fresh interval and wait for a future soft state instead.
    if (!playerContext.physics.known.load() || playerContext.physics.usingCBPC.load()) {
        ScheduleNextRandomErection(copy, now);
        return;
    }

    playerContext.spontaneous.randomNextMs.store(0);
    playerContext.spontaneous.randomManualTest.store(false);
    playerContext.spontaneous.randomUntilMs.store(now +
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
    const int softBend = copy.flaccidAngleControl && SoftAngleControlAvailable(copy.bendMethod) ? copy.flaccidBend : 0;
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
    positionController.ResetAutomaticRecovery();
    ClearResolvedPositionError();
}

bool AutomaticBendAllowed(const Settings& copy) {
    const auto result = positionController.CheckAutomatic(NowMs(), copy.bounceGuard);
    if (result.pauseStarted) {
        Record("Bounce guard paused automatic position repairs for 5 seconds");
    }
    return result.allowed;
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
    if (dispatch.nativeAccepted) {
        playerContext.physics.sosConnected.store(true);
    }
    // A pose write succeeds or fails immediately, so it needs no retry ledger.
    // Only dispatched (event) angles are tracked; a failed pose falls through to
    // the graph inside the adapter and is reported on that path instead.
    if (dispatch.method == 0) {
        playerContext.position.appliedBend.store(bend);
        playerContext.position.lastMethod.store(0);
        const auto clearedPoseError = ClearResolvedPositionError();
        if (clearedPoseError) {
            Record("SOS angle control recovered");
        }
        return dispatch.accepted;
    }
    const auto now = NowMs();
    const auto result = positionController.CompleteDispatch(
        bend, dispatch.method, dispatch.accepted, automatic,
        copy.maxBendFailures, now);
    if (dispatch.accepted) {
        // SOS bend updates can emit a NiNode update. Ignore that echo so it
        // cannot be mistaken for a rebuilt skeleton and start a repair loop.
        playerContext.recovery.ignoreNodeEventsUntilMs.store(now + 2000);
        const bool clearedError = ClearResolvedPositionError();
        if (result.recovered || clearedError)
            Record("SOS angle control recovered");
    } else if (result.pauseStarted) {
        Record("SPS-011: SOS is not ready for the angle yet; automatic retry paused for 5 seconds", true);
    }
    return dispatch.accepted;
}

void CancelErectionAnimation() {
    positionController.CancelAnimation();
}

void StartBendAnimation(int startBend, int targetBend, int durationMs, bool relaxing) {
    startBend = std::clamp(startBend, 0, 20);
    targetBend = std::clamp(targetBend, 0, 20);
    // Matches the Rise/Fall time sliders and the INI clamp (0.5-30 s). The old
    // 15 s cap silently ignored the upper half of both sliders.
    durationMs = std::clamp(durationMs, 500, 30000);
    const auto generation = positionController.BeginAnimation(targetBend, relaxing, NowMs());
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    // Any backend whose behaviour graph binds SOSBend0-9 can drive the gradual
    // ramp through events, and the graph is also what CBPC follows. Only the
    // opt-in pose mode writes a bone directly; it is excluded here because the
    // animation graph and CBPC both rewrite GenBase/Gen01 every frame, so a
    // direct write from the ramp thread is overwritten before it is drawn.
    const bool tngBackend = TngLoaded();
    const bool poseBackend = copy.bendMethod == SPS::Runtime::kBendMethodPose;
    const bool useGraphEvents = !poseBackend &&
        (tngBackend || LegacySosLoaded() || SosAeNativeLoaded());
    const bool nativeBackend = poseBackend || (!useGraphEvents && SosAeNativeLoaded());
    if (!nativeBackend && !useGraphEvents) {
        positionController.FailAnimationStart();
        return;
    }

    // A descent has to walk 20 -> 1 while CBPC still owns the bones, and only
    // hand over to SMP once the lowest step has been sent. Ownership is
    // therefore pinned for the duration and released by the completion path
    // below. Releasing first would let FSMP settle the assembly into its own
    // resting pose before the angle had a chance to lower.
    if (relaxing) {
        playerContext.position.relaxing.store(true);
    }

    const bool softAngleSupported = SoftAngleControlAvailable(copy.bendMethod);
    erectionAnimationThread = std::jthread([generation, startBend, targetBend, durationMs, useGraphEvents, tngBackend, relaxing, softAngleSupported](std::stop_token token) {
        while (!token.stop_requested() && playerContext.position.animating.load() &&
            generation == playerContext.position.animationGeneration.load()) {
            if (SPS::Runtime::ExecutionPaused()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            const auto elapsed = std::max<std::int64_t>(0, NowMs() - playerContext.position.animationStartMs.load());
            const float t = std::clamp(static_cast<float>(elapsed) / static_cast<float>(durationMs), 0.0F, 1.0F);
            // Graph backends walk the visible stages in even steps, so the
            // configured time is the exact travel time from the first stage to
            // the last. The eased 0-20 curve spent its first and last sixth on
            // the end stages and finished the visible climb at about 84%.
            const float eased = t * t * (3.0F - 2.0F * t);
            const int bend = useGraphEvents
                ? SPS::Core::PoseTargets::StepRampBend(startBend, targetBend, t, tngBackend)
                : std::clamp(static_cast<int>(std::lround(
                    startBend + (targetBend - startBend) * eased)), 0, 20);
            // The queue key is the event that will be sent, shifted off the -1
            // sentinel. SOSBend-9 is index -9, so its key is 1. Bend 0 and bend
            // 1 share that event and must not be dispatched twice.
            const int queueKey = useGraphEvents
                ? AnimationEventBend(bend, tngBackend) + 10
                : bend;
            const int previousQueueKey = playerContext.position.animationLastQueuedBend.exchange(queueKey);
            // t >= 1.0F forces the final step out even when it maps to a stage
            // that was already sent. That is what guarantees the target value
            // lands and the handoff sees a completed ramp; dedupe alone can
            // leave the last decimal of the curve unsent.
            if (queueKey != previousQueueKey || t >= 1.0F) {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([generation, bend, targetBend, useGraphEvents, tngBackend, relaxing, softAngleSupported] {
                        if (!playerContext.position.animating.load() || generation != playerContext.position.animationGeneration.load()) return;
                        if (SPS::Runtime::ExecutionPaused() || ownershipController.Read().pending) {
                            playerContext.position.animationLastQueuedBend.store(-1);
                            return;
                        }
                        if (!playerContext.physics.known.load() || !playerContext.physics.usingCBPC.load()) {
                            // A descent releases CBPC only once SOSBend-9 has been
                            // reached. A single tick that does not read as CBPC
                            // must not cancel a rise; that restart is the snap
                            // down and the second climb.
                            const bool atRelease = relaxing && SPS::Core::PoseTargets::IsCbpcReleaseIndex(
                                AnimationEventBend(bend, tngBackend));
                            if (atRelease) {
                                positionController.CompleteAnimation(targetBend);
                                Evaluate(true);
                            } else if (!relaxing) {
                                playerContext.position.animationLastQueuedBend.store(-1);
                            } else {
                                CancelErectionAnimation();
                            }
                            return;
                        }
                        auto* player = RE::PlayerCharacter::GetSingleton();
                        if (!player) {
                            CancelErectionAnimation();
                            return;
                        }
                        const bool ok = useGraphEvents
                            ? SendPositionEvent(static_cast<RE::Actor*>(player),
                                RE::BSFixedString(SPS::Runtime::SosBendEventName(bend, tngBackend)),
                                tngBackend)
                            : CallSosAeBend(static_cast<RE::Actor*>(player), bend);
                        if (!ok) {
                            // Some SOS AE builds stop accepting native bend values
                            // just above their visible flaccid limit. During
                            // relaxation that means the animation has already gone
                            // as low as this backend can display. Finish the owner
                            // handoff instead of cancelling and restarting the same
                            // 5 -> 0 animation every poll.
                            if (relaxing) {
                                // Release on the lowest pose. SOSFlaccid is a
                                // different clip and would play after the handoff
                                // point.
                                const bool bottomAccepted =
                                    SendPositionEvent(static_cast<RE::Actor*>(player),
                                        RE::BSFixedString(SPS::Core::PoseTargets::EventName(0, tngBackend)),
                                        tngBackend);
                                const auto completedAt = NowMs();
                                // Keep the transition marked as relaxing until
                                // SMP actually accepts ownership. Otherwise the
                                // ordinary CBPC repair pass can see CBPC still
                                // active for one Papyrus tick and replay the
                                // erect angle immediately before the soft
                                // handoff.
                                positionController.CompleteRelaxationFallback(
                                    targetBend, bottomAccepted, completedAt);
                                if (bottomAccepted) {
                                    playerContext.recovery.ignoreNodeEventsUntilMs.store(completedAt + 2000);
                                }
                                Record(bottomAccepted
                                    ? "Reached SOSBend-9; SMP handoff requested"
                                    : "SOS reached its lowest available angle; SMP handoff requested");
                                SetOwner(false, true);
                                return;
                            }
                            positionController.FailAnimationStep(NowMs() + 1500);
                            Record("Gradual erection is waiting for SOS; the normal angle was queued instead");
                            return;
                        }
                        if (!useGraphEvents) playerContext.physics.sosConnected.store(true);
                        const auto acceptedAt = NowMs();
                        positionController.AcceptAnimationStep(
                            bend, useGraphEvents ? 1 : 0, acceptedAt);
                        ClearResolvedPositionError();
                        playerContext.recovery.ignoreNodeEventsUntilMs.store(acceptedAt + 2000);
                        // Animation-event backends expose discrete visible SOS
                        // positions. Finish the handoff as soon as the event
                        // actually sent is the one the target resolves to,
                        // instead of holding CBPC for the remainder of the
                        // internal 0-20 easing curve - no reachable target may
                        // be left unable to complete, or the handoff back to SMP
                        // is never issued and the assembly stays floppy.
                        // SOSBend-9 is the bottom. A fall does not finish on an earlier
                        // event, and it does not continue into the SOSFlaccid clip.
                        const bool atCbpcRelease = useGraphEvents
                            ? SPS::Core::PoseTargets::IsCbpcReleaseIndex(
                                AnimationEventBend(bend, tngBackend))
                            : bend <= 1;
                        const bool reachedVisibleTarget = relaxing
                            ? atCbpcRelease
                            : (useGraphEvents
                                ? std::string_view(SPS::Runtime::SosBendEventName(bend, tngBackend)) ==
                                      std::string_view(SPS::Runtime::SosBendEventName(targetBend, tngBackend))
                                : bend == targetBend);
                        if (!reachedVisibleTarget) return;
                        // A retry that lands on an already-dispatched stage still
                        // has to report the bend it reached, but the completion
                        // path must run only once per ramp: repeating it rewrites
                        // appliedBend at this same height every tick and reissues
                        // the SMP handoff, which is the visible dip.
                        if (playerContext.position.animationCompleted.load()) return;
                        positionController.CompleteAnimation(targetBend);
                        // SOS AE can rebuild or finish its graph transition
                        // just after the gradual native updates complete.
                        // Replay the final value once through compatibility
                        // mode so the visible angle cannot remain at zero.
                        if (!relaxing && softAngleSupported)
                            playerContext.position.confirmationDueMs.store(NowMs() + 500);
                        ++playerContext.position.repairs;
                        Record(relaxing
                            ? "Erection lowered to SOSBend-9; SMP handoff requested"
                            : fmt::format("Gradual erection completed: {}/20", targetBend));
                        // SOSBend-9 was just sent while CBPC still owned the bones.
                        // Hand them to SMP now. Another descent from here would
                        // play a further clip under CBPC after the release point.
                        if (relaxing) {
                            SetOwner(false, true);
                        }
                    });
                }
            }
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
    // appliedBend -1 and 0 both start on SOSBend-9, the lowest pose. They must
    // not start on SOSFlaccid: that clip snaps through the middle, falls, then
    // the rest of the rise climbs again.
    int current = playerContext.position.appliedBend.load();
    if (current < 0 && NowMs() < heldBendUntilMs.load()) current = heldBendBeforeReconnect.load();
    const int from = std::clamp(current < 0 ? 0 : current, 0, targetBend);
    StartBendAnimation(from, targetBend, durationMs, false);
}

void StartGradualRelaxation(const Settings& copy) {
    int appliedNow = playerContext.position.appliedBend.load();
    if (appliedNow < 0 && NowMs() < heldBendUntilMs.load()) appliedNow = heldBendBeforeReconnect.load();
    const int startBend = appliedNow >= 0 ? appliedNow : copy.erectBend;
    int targetBend = copy.flaccidAngleControl && SoftAngleControlAvailable(copy.bendMethod) ? copy.flaccidBend : 0;
    // SOSBend-9 is the lowest angle. CBPC is released there, so a higher
    // configured soft bend cannot end the fall early.
    if (SPS::Core::PoseTargets::EventIndex(targetBend, TngLoaded()) > -9)
        targetBend = 0;
    if (playerContext.position.animating.load() && playerContext.position.relaxing.load() &&
        playerContext.position.animationTargetBend.load() == targetBend) return;
    // From this point onward the requested state is soft. Any delayed erect
    // confirmation, retry or replacement-mesh replay belongs to the previous
    // state and must not be allowed to race the CBPC -> SMP handoff.
    playerContext.position.settleDueMs.store(0);
    playerContext.position.confirmationDueMs.store(0);
    playerContext.position.retryDueMs.store(0);
    playerContext.recovery.erectMeshReplayDueMs.store(0);
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
    if (playerContext.position.animating.load()) return;
    if (!force && playerContext.position.settleDueMs.load() > now) return;
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
                Record(fmt::format("Erect vertical bend requested: {}/20", desired));
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

    // The soft pose is a bend stage: 0 sends SOSBend-9 (the lowest position),
    // a supported FlaccidBend sends its own stage. SOSFlaccid is never sent.
    const bool customSoftAngle = copy.flaccidAngleControl && SoftAngleControlAvailable(copy.bendMethod);
    const int desired = customSoftAngle ? copy.flaccidBend : 0;
    playerContext.position.requestedBend.store(desired);
    if (!force && playerContext.position.appliedBend.load() == desired) return;

    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        const int previous = playerContext.position.appliedBend.load();
        const bool ok = ApplyBend(static_cast<RE::Actor*>(player), desired, true, animate, false);
        if (playerContext.position.lastMethod.load() < 0) return;
        if (ok && customSoftAngle && previous != desired)
            Record(fmt::format("Soft vertical bend requested: {}/20", desired));
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
    if (!QueueOwnershipHandoff(
        false, SPS::Controllers::OwnershipPurpose::confirmSoft)) {
        logger::warn("Soft-state confirmation could not be queued");
        return false;
    }
    return true;
}

bool ConfirmCBPCState() {
    if (!playerContext.physics.known.load() || !playerContext.physics.usingCBPC.load()) return true;
    if (!PapyrusReadyForDispatch()) return false;
    // Repeat both halves of the handoff on a later game tick. A successful
    // Papyrus dispatch only means the calls were queued; during a new game or
    // skeleton rebuild either physics engine can finish after the other one.
    // Reasserting SMP-off before CBPC-on makes the final owner deterministic.
    if (!QueueOwnershipHandoff(
        true, SPS::Controllers::OwnershipPurpose::confirmCBPC)) {
        logger::warn("Erect-state confirmation could not be queued");
        return false;
    }
    return true;
}

void RunLoadSMPReset() {
    if (ownershipController.Read().pending || PPAOwnsPosition()) {
        playerContext.recovery.loadSmpResetDueMs.store(NowMs() + 1000);
        return;
    }
    if (!PapyrusReadyForDispatch()) {
        playerContext.recovery.loadSmpResetDueMs.store(NowMs() + 1000);
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

    if (!playerContext.physics.known.load() || !QueueOwnershipHandoff(
            playerContext.physics.usingCBPC.load(), SPS::Controllers::OwnershipPurpose::resetLoad)) {
        playerContext.recovery.loadSmpResetDueMs.store(NowMs() + 1000);
        return;
    }
    Record("Player SMP reset and ownership restoration queued after loading");
}

void ScheduleLoadSMPReset() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.enabled || !copy.resetSMPAfterLoad || !FsmpActorApiAvailable()) {
        playerContext.recovery.loadSmpResetDueMs.store(0);
        return;
    }
    playerContext.recovery.loadSmpResetDueMs.store(NowMs() + copy.loadResetDelayMs);
    Record(fmt::format("Player SMP reset scheduled in {:.1f} seconds", copy.loadResetDelayMs / 1000.0F));
}

void ScheduleSoftAngleRefresh(int delayMs) {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.enabled || !copy.positionControl || !SoftAngleControlAvailable(copy.bendMethod) ||
        !playerContext.physics.known.load() || playerContext.physics.usingCBPC.load() || PPAOwnsPosition()) {
        playerContext.recovery.softAngleRefreshDueMs.store(0);
        return;
    }
    playerContext.recovery.softAngleRefreshDueMs.store(NowMs() + std::clamp(delayMs, 0, 2000));
}

void ScheduleSoftPoseResetAfterScene(std::int64_t sceneEndedMs) {
    if (!playerContext.physics.known.load() || playerContext.physics.usingCBPC.load() ||
        !FsmpActorApiAvailable()) return;

    // PPA/Accurate Penetration and SOFTBODY can legitimately move or rebuild
    // the genital chain during a scene. If the player remains soft throughout,
    // there is no CBPC -> SMP handoff to trigger the normal pose rebuild. Queue
    // the same player-only FSMP reset after scene cleanup has settled so stale
    // stretched transforms cannot become the new floppy resting pose.
    const bool scenePhysicsCanMoveSoftPose =
        ::GetModuleHandleW(SPS::PPA::kPluginDLL) != nullptr || SoftbodyLoaded();
    if (!scenePhysicsCanMoveSoftPose) return;

    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.enabled) return;
    const auto due = sceneEndedMs + std::max(copy.sceneEndDelayMs + 500, 2000);
    playerContext.recovery.softHandoffResetDueMs.store(due);
    playerContext.recovery.softHandoffResetUntilMs.store(due + 10000);
    Record("Post-scene soft SMP pose refresh queued");
}

void RunSoftAngleRefresh() {
    if (!playerContext.physics.known.load() || playerContext.physics.usingCBPC.load()) return;
    if (ownershipController.Read().pending || PPAOwnsPosition()) {
        playerContext.recovery.softAngleRefreshDueMs.store(NowMs() + 1000);
        return;
    }
    if (!PapyrusReadyForDispatch()) {
        playerContext.recovery.softAngleRefreshDueMs.store(NowMs() + 1000);
        return;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || ::GetModuleHandleW(L"hdtsmp64.dll") == nullptr) return;

    if (!QueueOwnershipHandoff(false, SPS::Controllers::OwnershipPurpose::resetAngle)) {
        Record("SPS-018: Soft-angle refresh could not be queued", true);
        return;
    }
    Record("Soft angle changed; guarded player pose refresh queued");
}

bool RefreshSMPAfterPlayerMeshChange() {
    if (!playerContext.physics.known.load() || playerContext.physics.usingCBPC.load()) return false;
    if (ownershipController.Read().pending || PPAOwnsPosition()) {
        playerContext.recovery.nodeRefreshDueMs.store(NowMs() + 750);
        return false;
    }
    if (!PapyrusReadyForDispatch()) {
        playerContext.recovery.nodeRefreshDueMs.store(NowMs() + 1000);
        return false;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player || ::GetModuleHandleW(L"hdtsmp64.dll") == nullptr) return false;

    const auto now = NowMs();
    playerContext.recovery.ignoreNodeEventsUntilMs.store(now + 3000);
    const bool dispatched = QueueOwnershipHandoff(false, SPS::Controllers::OwnershipPurpose::resetMesh);
    if (!dispatched) return false;

    Record("Player mesh changed; refreshing its SMP physics");
    return true;
}

std::int64_t stableWaitStartMs = 0;

void RunSoftHandoffSMPReset() {
    if (playerContext.physics.known.load() && playerContext.physics.usingCBPC.load()) {
        stableWaitStartMs = 0;
        playerContext.recovery.softHandoffResetDueMs.store(0);
        playerContext.recovery.softHandoffResetUntilMs.store(0);
        return;
    }
    const auto now = NowMs();
    auto retry = [now](std::string_view reason) {
        auto until = playerContext.recovery.softHandoffResetUntilMs.load();
        if (until == 0) {
            until = now + 10000;
            playerContext.recovery.softHandoffResetUntilMs.store(until);
        }
        if (now < until) {
            playerContext.recovery.softHandoffResetDueMs.store(now + 1000);
            logger::debug("Soft SMP pose refresh is waiting for {}", reason);
        } else {
            playerContext.recovery.softHandoffResetUntilMs.store(0);
            Record("SPS-019: Soft SMP pose refresh stopped before it could run", true);
        }
    };
    if (PPAOwnsPosition()) {
        // Active penetration alignment is expected to last longer than the
        // normal retry window. Keep the refresh waiting until scene cleanup.
        playerContext.recovery.softHandoffResetUntilMs.store(now + 10000);
        playerContext.recovery.softHandoffResetDueMs.store(now + 1000);
        return;
    }
    if (!playerContext.physics.known.load() || ownershipController.Read().pending) {
        retry("the current ownership handoff");
        return;
    }
    if (!PapyrusReadyForDispatch()) {
        retry("Papyrus");
        return;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        retry("the player");
        return;
    }
    if (::GetModuleHandleW(L"hdtsmp64.dll") == nullptr) {
        playerContext.recovery.softHandoffResetUntilMs.store(0);
        Record("SPS-019: Soft SMP pose refresh could not find Faster HDT-SMP", true);
        return;
    }

    // Wait (at most 6 s) for the shaft bones to stop moving before the rebuild,
    // so SMP does not bake a pose that is still blending. After the cap the reset
    // runs anyway, so this can only delay it.
    if (stableWaitStartMs == 0) stableWaitStartMs = now;
    if (now - stableWaitStartMs < 6000 && !ShaftPoseStable()) {
        playerContext.recovery.softHandoffResetDueMs.store(now + 500);
        logger::debug("Soft SMP pose refresh is waiting for the shaft pose to settle");
        return;
    }
    Record(now - stableWaitStartMs >= 6000
        ? "Soft SMP reset: shaft pose still moving after 6 s, resetting anyway"
        : fmt::format("Soft SMP reset: shaft pose stable after {} ms", now - stableWaitStartMs));
    stableWaitStartMs = 0;

    // The actor reset reloads meshes but does not run the global button's
    // physics-world reset. The guarded soft reset also lets the six managed
    // bones follow the skeleton briefly after rebuilding, then resumes SMP.
    // Command completion still cannot prove the visible resting length.
    playerContext.recovery.ignoreNodeEventsUntilMs.store(now + 3000);
    const bool dispatched = QueueOwnershipHandoff(false, SPS::Controllers::OwnershipPurpose::resetSoft);
    if (!dispatched) {
        retry("Faster HDT-SMP");
        return;
    }

    playerContext.recovery.softConfirmationDueMs.store(0);
    Record("Soft SMP pose rebuild and ownership restoration queued");
}

void ReportSmpHandoffProof(const SPS::Controllers::OwnershipCompletion& completion)
{
    if (completion.targetCBPC || !completion.success) return;
    using SPS::Controllers::ClassifySmpHandoff;
    using SPS::Controllers::SmpHandoffProof;
    const auto proof = ClassifySmpHandoff(completion.dynamicBonesAfter, completion.boneCount);
    if (proof == SmpHandoffProof::certain) {
        Record(fmt::format(
            "SMP proof: {}/{} managed bones were dynamic after the handoff",
            completion.dynamicBonesAfter, completion.boneCount));
        return;
    }
    if (proof == SmpHandoffProof::notProven) {
        Record(fmt::format(
            "SMP not proven: {}/{} managed bones were dynamic after the handoff",
            completion.dynamicBonesAfter, completion.boneCount), true);
        return;
    }
    Record("SMP not proven: the handoff returned no bone sample", true);
}

void HandleOwnershipCompletion(const SPS::Controllers::OwnershipCompletion& completion) {
    if (!completion.matched) return;

    if (completion.superseded) {
        logger::debug("Outdated physics transaction retired; current policy will select the next owner");
        return;
    }

    if (completion.purpose == SPS::Controllers::OwnershipPurpose::maintainCBPC) {
        // The selected owner is CBPC, so only CBPC is corroborated here. The
        // disabled direction cannot be proved: FSMP reports "kinematic or not
        // found" identically, so SMP is not marked ready on that basis.
        playerContext.physics.cbpcConnected.store(completion.success && completion.targetCBPC);
        playerContext.physics.smpConnected.store(false);
        ReportSmpDrift(completion);
        if (!completion.success)
            logger::warn("SMP-off safeguard did not complete; ownership is unknown and will be recovered");
        // A single SMP-off command cannot acknowledge CBPC attachment, clear
        // unrelated repair errors, or restart the selected owner's bend timers.
        return;
    }

    const auto now = NowMs();
    // The selected owner and its opposite are reported separately. Both were
    // previously set from one boolean, which made an SMP-off handoff report SMP
    // as ready and overstated engine health in support reports.
    if (completion.success) {
        playerContext.physics.cbpcConnected.store(completion.targetCBPC);
        playerContext.physics.smpConnected.store(!completion.targetCBPC);
    } else {
        playerContext.physics.smpConnected.store(false);
        playerContext.physics.cbpcConnected.store(false);
    }
    ReportSmpDrift(completion);
    if (!completion.success) {
        CancelErectionAnimation();
        if (completion.softTransition)
            playerContext.position.relaxing.store(true);
        switch (completion.purpose) {
        case SPS::Controllers::OwnershipPurpose::switchOwner:
            Record(fmt::format("SPS-010: Ordered physics handoff to {} did not complete; retry queued",
                completion.targetCBPC ? "CBPC" : "SMP"), true);
            break;
        case SPS::Controllers::OwnershipPurpose::confirmSoft:
            if (now < playerContext.recovery.softConfirmationUntilMs.load())
                playerContext.recovery.softConfirmationDueMs.store(now + 1000);
            logger::warn("Soft-state ownership confirmation did not complete; bounded retry queued");
            break;
        case SPS::Controllers::OwnershipPurpose::confirmCBPC:
            if (now < playerContext.recovery.cbpcConfirmationUntilMs.load())
                playerContext.recovery.cbpcConfirmationDueMs.store(now + 1000);
            logger::warn("Erect-state ownership confirmation did not complete; bounded retry queued");
            break;
        case SPS::Controllers::OwnershipPurpose::resetSoft:
            if (now < playerContext.recovery.softHandoffResetUntilMs.load())
                playerContext.recovery.softHandoffResetDueMs.store(now + 1000);
            Record("SPS-019: Soft pose transaction did not complete; current ownership will be recovered", true);
            break;
        case SPS::Controllers::OwnershipPurpose::resetLoad:
        case SPS::Controllers::OwnershipPurpose::resetAngle:
        case SPS::Controllers::OwnershipPurpose::resetMesh:
            Record("SPS-010: Physics preparation did not complete; current ownership will be recovered", true);
            break;
        case SPS::Controllers::OwnershipPurpose::reconnectMesh:
            if (now < playerContext.recovery.nodeCbpcReacquireUntilMs.load())
                playerContext.recovery.nodeCbpcReacquireDueMs.store(now + 1000);
            Record("SPS-010: Equipment reconnection did not complete; current ownership will be recovered", true);
            break;
        case SPS::Controllers::OwnershipPurpose::restore:
            if (now < playerContext.recovery.externalOwnerRepairUntilMs.load())
                playerContext.recovery.externalOwnerRepairDueMs.store(now + 1000);
            logger::warn("Physics ownership restoration did not complete; bounded retry queued");
            break;
        }
        return;
    }

    ClearResolvedOwnerError();
    ReportSmpHandoffProof(completion);
    using Purpose = SPS::Controllers::OwnershipPurpose;
    if (completion.purpose == Purpose::resetLoad || completion.purpose == Purpose::resetSoft ||
        completion.purpose == Purpose::resetAngle || completion.purpose == Purpose::resetMesh ||
        completion.purpose == Purpose::reconnectMesh) {
        ClearResolvedRefreshError();
        playerContext.recovery.ignoreNodeEventsUntilMs.store(now + 2000);
        // A post-fall soft reset follows a resting pose that was already sent and
        // has had time to blend. Sending SOSFlaccid again would replay the clip
        // after the reset and move the bones the reset just settled, so that one
        // case keeps the recorded pose and does not resend it.
        const bool restingPoseAlreadyApplied = completion.purpose == Purpose::resetSoft &&
            !completion.targetCBPC && playerContext.position.appliedBend.load() >= 0;
        if (!restingPoseAlreadyApplied) playerContext.position.appliedBend.store(-1);
        if (!completion.targetCBPC) shaftSampleStartMs.store(now);
        if (completion.purpose == Purpose::resetLoad) {
            playerContext.recovery.lastLoadSmpResetMs.store(now);
            ++playerContext.recovery.loadSmpResets;
        }
        if (completion.purpose == Purpose::resetSoft)
            playerContext.recovery.softHandoffResetUntilMs.store(0);
        if (completion.purpose == Purpose::reconnectMesh) {
            playerContext.recovery.nodeCbpcReacquireDueMs.store(0);
            playerContext.recovery.nodeCbpcReacquireUntilMs.store(0);
        }
        if (completion.targetCBPC) {
            Settings copy;
            { std::scoped_lock lock(settingsLock); copy = settings; }
            if (copy.positionControl) {
                playerContext.position.settleDueMs.store(now + copy.settleDelayMs);
                playerContext.position.confirmationDueMs.store(now + copy.settleDelayMs + 1500);
            }
        } else if (!restingPoseAlreadyApplied) ApplyRequestedSoftBend(true, true);
        Record(restingPoseAlreadyApplied
            ? "Player SMP reset executed; resting pose kept (not replayed)"
            : "Player physics preparation and owner handoff executed; pose update requested");
        return;
    }
    if (completion.purpose == SPS::Controllers::OwnershipPurpose::confirmSoft) {
        playerContext.recovery.softConfirmationUntilMs.store(0);
        ApplyRequestedSoftBend(true, true);
        Record("Soft ownership commands executed; live motion requires in-game verification");
        return;
    }
    if (completion.purpose == SPS::Controllers::OwnershipPurpose::confirmCBPC) {
        playerContext.recovery.cbpcConfirmationUntilMs.store(0);
        Record("CBPC ownership commands executed; live motion requires in-game verification");
        return;
    }
    if (completion.purpose == SPS::Controllers::OwnershipPurpose::restore) {
        playerContext.recovery.externalOwnerRepairUntilMs.store(0);
        playerContext.recovery.lastOwnerRestorationMs.store(now);
        playerContext.recovery.ignoreNodeEventsUntilMs.store(now + 2000);
        ++ownerRestorations;
        Record(fmt::format("{} ownership restoration completed",
            completion.targetCBPC ? "CBPC" : "SMP"));
        return;
    }

    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const bool cbpc = completion.targetCBPC;
    const auto previousState = completion.previousKnown ?
        (completion.previousCBPC ? SPS::API::PhysicsState::CBPC : SPS::API::PhysicsState::SMP) :
        SPS::API::PhysicsState::Unknown;

    if (!cbpc || now >= playerContext.recovery.nodeCbpcReacquireUntilMs.load()) {
        playerContext.recovery.nodeCbpcReacquireDueMs.store(0);
        playerContext.recovery.nodeCbpcReacquireUntilMs.store(0);
    }
    if (cbpc) {
        playerContext.recovery.softHandoffResetDueMs.store(0);
        playerContext.recovery.softHandoffResetUntilMs.store(0);
        playerContext.recovery.softAngleRefreshDueMs.store(0);
    }
    playerContext.recovery.ignoreNodeEventsUntilMs.store(now + 4000);
    ResetPositionRecovery();
    playerContext.position.appliedBend.store(-1);
    const bool realSoftHandoff = !cbpc && previousState == SPS::API::PhysicsState::CBPC;
    if (realSoftHandoff) {
        playerContext.recovery.softConfirmationDueMs.store(0);
        // The resting clip (SOSFlaccid) is sent when this handoff completes. Reset
        // SMP only after it has had time to blend, so the reset captures the
        // finished pose rather than a mid-blend one.
        playerContext.recovery.softHandoffResetDueMs.store(now + 2500);
        playerContext.recovery.softHandoffResetUntilMs.store(now + 12000);
    } else {
        playerContext.recovery.softConfirmationDueMs.store(cbpc ? 0 : now + 750);
    }
    playerContext.recovery.softConfirmationUntilMs.store(cbpc ? 0 : now + 15000);
    // The CBPC confirmation is its own ownership transaction. Queued at +750 ms
    // it was still pending when the 400 ms settle came due on the next tick,
    // so the gradual rise waited about 2 s for it with CBPC holding the old
    // pose. During a timed rise, confirm after the ramp instead.
    std::int64_t cbpcConfirmDelayMs = 750;
    if (cbpc && copy.positionControl && copy.gradualErection &&
        (!copy.arousalBasedErection || RandomErectionActive())) {
        cbpcConfirmDelayMs = SPS::Core::PoseTargets::SwitchSettleMs(copy.settleDelayMs) +
            std::clamp(copy.erectionDurationMs, 500, 30000) + 750;
    }
    playerContext.recovery.cbpcConfirmationDueMs.store(cbpc ? now + cbpcConfirmDelayMs : 0);
    playerContext.recovery.cbpcConfirmationUntilMs.store(cbpc ? now + cbpcConfirmDelayMs + 15000 : 0);
    if (copy.positionControl) {
        if (cbpc) {
            // CBPC must own Gen01-Gen06 before the angle is written, otherwise
            // the bend is applied to nodes nothing is simulating yet and the
            // assembly falls back to its flaccid pose. The ordered handoff stops
            // SMP and calls StartPhysics per bone, but CBPC binds those bones on
            // its own update, which is after the Papyrus stack has returned.
            // Committing the owner is therefore not the same as CBPC being
            // attached, and the first angle write waits for that gap to close.
            const auto settleMs = SPS::Core::PoseTargets::SwitchSettleMs(copy.settleDelayMs);
            playerContext.position.requestedBend.store(DesiredBend(copy));
            const bool timedGradual = copy.gradualErection &&
                (!copy.arousalBasedErection || RandomErectionActive());
            // Always deferred through settleDueMs. Starting the ramp or the
            // first angle write here would bypass the attach wait above.
            playerContext.position.settleDueMs.store(now + settleMs);
            playerContext.position.confirmationDueMs.store(
                timedGradual ? 0 : now + settleMs + 1500);
        } else {
            playerContext.position.settleDueMs.store(0);
            playerContext.position.confirmationDueMs.store(0);
            if (!PPAOwnsPosition()) ApplyRequestedSoftBend(true, true);
        }
    }
    if (cbpc && previousState == SPS::API::PhysicsState::Unknown) {
        playerContext.recovery.loadSmpResetDueMs.store(0);
    }
    Record(fmt::format("Physics switched to {} ({}) after the ordered handoff completed",
        cbpc ? "CBPC" : "SMP", cbpc ? "erect" : "soft"));
    const auto currentState = cbpc ? SPS::API::PhysicsState::CBPC : SPS::API::PhysicsState::SMP;
    if (previousState != currentState) {
        // A real owner change is a genuine rise or fall, so the angle held
        // before an earlier reconnect must not seed it.
        heldBendUntilMs.store(0);
        NotifyAPIStateChanged(previousState, currentState);
    }
}

void FinishOwnershipHandoff(std::uint64_t generation, bool success) {
    HandleOwnershipCompletion(ownershipController.Complete(generation, success, NowMs()));
}

void ReportSmpDrift(const SPS::Controllers::OwnershipCompletion& completion)
{
    using Purpose = SPS::Controllers::OwnershipPurpose;
    if (!completion.smpDriftDetected) return;
    const bool backgroundProbe = completion.purpose == Purpose::maintainCBPC ||
        completion.purpose == Purpose::restore;
    Record(fmt::format(
        "SMP resumed ownership of {}/6 managed bones while {} was selected{}; the owner was reasserted",
        completion.dynamicBonesBefore,
        completion.targetCBPC ? "CBPC" : "SMP",
        backgroundProbe ? " during a background check" : ""), backgroundProbe);
}

bool QueueOwnershipHandoff(
    bool cbpc,
    SPS::Controllers::OwnershipPurpose purpose,
    bool softTransition) {
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;

    if (!PapyrusReadyForDispatch() || !SPS::Runtime::OrderedFsmpBridgeAvailable()) return false;
    // A timed-out/cancelled stack can still be retiring. Waiting for its lease
    // is not another failed physics attempt, and must not inflate SPS-010.
    if (!ownershipController.Read().pending && SPS::Runtime::PhysicsOperationBusy()) return false;
    const auto begin = ownershipController.Begin(cbpc, purpose, softTransition, NowMs());
    if (begin.status == SPS::Controllers::OwnershipBeginStatus::alreadyPending)
        return true;
    if (begin.status == SPS::Controllers::OwnershipBeginStatus::blocked)
        return false;

    using Purpose = SPS::Controllers::OwnershipPurpose;
    const bool reset = purpose == Purpose::resetLoad ||
        purpose == Purpose::resetAngle || purpose == Purpose::resetMesh;
    const int preparation = purpose == Purpose::maintainCBPC ? 3 :
        (purpose == Purpose::resetSoft ? 5 :
            (reset ? 1 : (purpose == Purpose::reconnectMesh ? 2 : 0)));
    // resetSoft used preparation 4, which only toggled SMP off and on and left the
    // shaft settled against the old pose. It now uses 5: the actor's
    // DynamicHDT.ResetPhysics (the FSMP reset the user ran by hand) and nothing
    // after it, because the CBPC stop and SMP re-enable that preparation 1 ends
    // with moved the bones again about a second after the reset.
    // softBend >= 0 makes SPS_FSMPBridge send SOSFlaccid before its reset.
    // That clip is not the lowest pose, so it is no longer requested on the
    // graph path: the soft stage (SOSBend-9 or FlaccidBend) is sent natively by
    // ApplyRequestedSoftBend when the reset completes. Only the experimental
    // pose-angle method still uses the bridge's soft-angle step.
    int softBend = -1;
    if (purpose == Purpose::resetAngle) {
        std::scoped_lock lock(settingsLock);
        if (settings.bendMethod == SPS::Runtime::kBendMethodPose &&
            SoftAngleControlAvailable(settings.bendMethod))
            softBend = settings.flaccidAngleControl ? settings.flaccidBend : 0;
    }
    const auto dispatched = SPS::Runtime::SetPlayerPhysicsOwner(
        static_cast<RE::Actor*>(player), cbpc, preparation, softBend,
        FsmpActorApiAvailable(), papyrusDispatchAllowedAfterMs.load(), NowMs(),
        [generation = begin.generation](SPS::Runtime::OwnershipReadback readback) {
            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddTask([generation, readback] {
                    SPS::Controllers::OwnershipEvidence evidence;
                    evidence.success = readback.acknowledged;
                    evidence.dynamicBonesBefore = readback.dynamicBonesBefore;
                    evidence.boneCount = readback.boneCount;
                    evidence.dynamicBonesAfter = readback.dynamicBonesAfter;
                    HandleOwnershipCompletion(
                        ownershipController.Complete(generation, evidence, NowMs()));
                });
            }
        });
    if (dispatched != SPS::Runtime::OwnershipDispatchResult::queued) {
        FinishOwnershipHandoff(begin.generation, false);
        return false;
    }

    return true;
}

// The soft path used to leave the graph on SOSFlaccid, and a configured
// FlaccidBend or another mod can still leave it on a higher stage. Whatever takes the bones at the
// start of an SMP -> CBPC switch (CBPC itself, or the kinematic gap of a
// superseded handoff) follows that pose until the ramp's first event, which is
// the snap up then snap down seen at the start of a rise. Put the graph on the
// ramp's own first stage, SOSBend-9, before the switch is dispatched. The ramp
// then starts from the pose the bones already have.
void PrimeLowestStageForRise(const Settings& copy) {
    const bool timedGradual = copy.gradualErection &&
        (!copy.arousalBasedErection || RandomErectionActive());
    if (!copy.positionControl || !timedGradual || copy.bendMethod == SPS::Runtime::kBendMethodPose ||
        PPAOwnsPosition() || AnySceneHasPriority(copy)) return;
    const bool tngBackend = TngLoaded();
    if (!tngBackend && !LegacySosLoaded() && !SosAeNativeLoaded()) return;
    if (auto* tasks = SKSE::GetTaskInterface()) {
        tasks->AddTask([tngBackend] {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player || !player->Is3DLoaded()) return;
            const bool sent = SendPositionEvent(static_cast<RE::Actor*>(player),
                RE::BSFixedString(SPS::Core::PoseTargets::EventName(0, tngBackend)), tngBackend);
            if (sent) Record("Rise primed on SOSBend-9 before the CBPC handoff");
        });
    }
}

bool SetOwner(bool cbpc, bool force) {
    const auto now = NowMs();
    if (ownershipController.Supersede(cbpc)) {
        SPS::Runtime::CancelPhysicsOperation();
        CancelErectionAnimation();
        logger::debug("Requested physics owner changed to {}; cancelling the outdated transaction",
            cbpc ? "CBPC" : "SMP");
    }
    const auto ownership = ownershipController.Read();
    if (ownership.pending) {
        if (ownership.pendingSuperseded) return false;
        if (ownership.pendingCBPC == cbpc &&
            ownership.pendingPurpose == SPS::Controllers::OwnershipPurpose::switchOwner)
            return true;
        if (!force && ownership.known && ownership.usingCBPC == cbpc)
            return true;
        return false;
    }
    if (!force && playerContext.physics.known.load() && playerContext.physics.usingCBPC.load() == cbpc) {
        // The end-of-tick safeguard reasserts SMP-off for the erect owner after
        // higher-priority recovery has had a chance to acquire the bridge.
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

    const bool softTransitionPending = !cbpc && playerContext.position.relaxing.load();
    CancelErectionAnimation();
    const bool enteringCBPC = cbpc &&
        !(playerContext.physics.known.load() && playerContext.physics.usingCBPC.load());
    if (enteringCBPC) PrimeLowestStageForRise(copy);
    return QueueOwnershipHandoff(
        cbpc, SPS::Controllers::OwnershipPurpose::switchOwner,
        softTransitionPending);
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
    const auto now = NowMs();
    const bool sexLabRoleQuerySettled =
        !sceneController.State().sexLab.roleQueryPending.load() &&
        now >= sceneController.State().sexLab.roleRetryAfterMs.load();
    const bool recentPPAUpdate = sexLabRoleQuerySettled &&
        sceneController.State().ppa.sceneRoleValid.load() &&
        now - sceneController.State().ppa.lastUpdateMs.load() < 3000;
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
        if (playerContext.position.relaxing.load()) {
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

    // An addon asked SPS to wait before the arousal-driven rise (V4 rise hold),
    // so the addon's own SMP request can arrive first. Only the everyday rise
    // waits: a scene's own request (normalControl is false) is never delayed, and
    // neither is a rise already under way (CBPC already owns the bones).
    if (normalControl && cbpc && apiRegistry.RiseHoldActive(NowMs()) &&
        !(playerContext.physics.known.load() && playerContext.physics.usingCBPC.load())) {
        logger::debug("CBPC rise is waiting for a compatibility API rise hold");
        return;
    }

    // A manual troubleshooting test should remain visible long enough for the
    // user to inspect it. Scene requests still take control immediately.
    if (normalControl && !force && SPS::Core::ManualPhysicsTestHoldsControl(
            pendingQuickAction.load(), NowMs() < pendingQuickActionUntilMs.load(),
            NowMs() < manualPhysicsTestUntilMs.load())) return;
    if (!normalControl) manualPhysicsTestUntilMs.store(0);

    // Scene integrations take priority over an everyday/spontaneous softening
    // animation. Cancel it here, where the new owner is known, instead of in
    // the random-erection scheduler. The old scheduler-side cancellation also
    // stopped ordinary softening every poll when random erections were off,
    // causing an endless restart at the last accepted bend (commonly 5/20).
    if (!normalControl && playerContext.position.relaxing.load()) {
        CancelErectionAnimation();
        playerContext.position.appliedBend.store(-1);
    }

    if (normalControl && playerContext.position.relaxing.load()) {
        if (!cbpc) {
            // A backend can reach its lowest visible angle before Papyrus is
            // ready to accept the owner switch. The animation has finished,
            // but the soft transition remains pending; retry only the handoff
            // and never restore the erect angle in this gap.
            if (SPS::Core::SoftHandoffNeedsRetry(playerContext.position.animating.load(),
                    playerContext.physics.known.load(), playerContext.physics.usingCBPC.load()))
                SetOwner(false, force);
            return;
        }
        CancelErectionAnimation();
        playerContext.position.appliedBend.store(-1);
    }

    // Lower the angle while CBPC still owns the bones, then return ownership
    // to SMP. Handing the bones to SMP first makes the erection snap down.
    if (normalControl && !cbpc && playerContext.physics.known.load() && playerContext.physics.usingCBPC.load() &&
        !playerContext.position.relaxing.load() && copy.gradualErection && copy.positionControl &&
        (playerContext.position.appliedBend.load() < 0 ||
            SPS::Core::PoseTargets::EventIndex(playerContext.position.appliedBend.load(), TngLoaded()) >
                SPS::Core::PoseTargets::EventIndex(
                    copy.flaccidAngleControl && SoftAngleControlAvailable(copy.bendMethod) ? copy.flaccidBend : 0,
                    TngLoaded())) &&
        !PPAOwnsPosition()) {
        StartGradualRelaxation(copy);
        return;
    }
    SetOwner(cbpc, force);
}

void Tick() {
    SPS::Runtime::UpdateExecutionClock();
    if (SPS::Runtime::ExecutionPaused()) return;
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const auto now = NowMs();

    ServicePlayerOslArousalWrite();

    ProcessPendingQuickAction();
    const auto expired = ownershipController.Expire(now, 5000);
    if (expired.matched) SPS::Runtime::CancelPhysicsOperation();
    HandleOwnershipCompletion(expired);
    if (!copy.enabled) {
        const auto ownership = ownershipController.Read();
        if (ownership.pending) {
            SetOwner(false, true);
            return;
        }
        if (!ownership.pending && (!ownership.known || ownership.usingCBPC) &&
            FsmpActorApiAvailable() && PapyrusReadyForDispatch()) {
            SetOwner(false, true);
        } else if (!ownership.pending && ownership.known && !ownership.usingCBPC) {
            // No recovery from the enabled session may wake up later if SPS is
            // re-enabled. The confirmed disabled state is plain SMP ownership.
            recoveryController.ResetTransient();
        }
        return;
    }

    if (copy.mode == 0)
        QueryArousal();
    else
        ClearArousalError();
    if (copy.sexLabOverride) QuerySexLab();
    const bool apiRoleWanted = ApiWantsSceneRole();
    if (copy.sexLabOverride && (copy.sexLabRoleSwitching || apiRoleWanted) && sceneController.State().sexLab.active.load()) QuerySexLabRole();
    if (copy.ostimOverride && (copy.ostimRoleSwitching || apiRoleWanted) && sceneController.State().ostim.active.load()) QueryOStimRole();
    NotifyAPISceneChangeIfNeeded();
    UpdateRandomErection(copy);
    bool forceStartupDecision = false;
    auto startupDue = playerContext.recovery.startupReconcileDueMs.load();
    const bool arousalProviderPresent =
        OslArousedLoaded() || SloArousedLoaded() || ClassicArousedLoaded();
    const bool waitingForInitialArousal = startupDue > 0 && !playerContext.physics.known.load() &&
        copy.mode == 0 && arousalProviderPresent && !arousalController.Valid() &&
        !RandomErectionActive() && !AnySceneHasPriority(copy) &&
        !ActiveAPIRequest().has_value();
    if (startupDue > 0 && now >= startupDue) {
        if (now >= playerContext.recovery.startupReconcileUntilMs.load()) {
            playerContext.recovery.startupReconcileDueMs.store(0);
            playerContext.recovery.startupReconcileUntilMs.store(0);
            Record("SPS-020: Startup physics check is still waiting for the game; use Repair current physics once loading finishes", true);
        } else if (!playerContext.physics.known.load()) {
            // In Automatic mode, do not force the temporary soft fallback while
            // a detected arousal provider is still returning its first value.
            // That old path produced soft -> erect during every high-arousal
            // load and doubled the number of Papyrus physics handoffs precisely
            // while the VM and player mesh were busiest.
            // Startup reconciliation must not supersede a queued manual test
            // merely because that test has not acknowledged an owner yet.
            forceStartupDecision = !waitingForInitialArousal &&
                !SPS::Core::ManualPhysicsTestHoldsControl(pendingQuickAction.load(),
                    now < pendingQuickActionUntilMs.load(), now < manualPhysicsTestUntilMs.load());
            playerContext.recovery.startupReconcileDueMs.store(now + (waitingForInitialArousal ? 500 : 1000));
        } else {
            playerContext.recovery.startupReconcileDueMs.store(0);
            playerContext.recovery.startupReconcileUntilMs.store(0);
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
    SampleShaftAfterReset();

    if (playerContext.recovery.startupReconcileDueMs.load() > 0 && playerContext.physics.known.load()) {
        playerContext.recovery.startupReconcileDueMs.store(0);
        playerContext.recovery.startupReconcileUntilMs.store(0);
        Record("Startup physics state selected");
    }

    if (activeManualPhysicsTest.load() >= 0 && now >= manualPhysicsTestUntilMs.load()) {
        activeManualPhysicsTest.store(-1);
        Record("Physics test finished; normal control resumed");
    }

    if (recoveryController.ClaimPostSwitchVerification(now)) {
        if (ConfirmCurrentPhysicsOwner("the completed physics switch")) {
            playerContext.recovery.postSwitchVerificationUntilMs.store(0);
        } else if (now < playerContext.recovery.postSwitchVerificationUntilMs.load()) {
            playerContext.recovery.postSwitchVerificationDueMs.store(now + 1000);
        } else {
            playerContext.recovery.postSwitchVerificationUntilMs.store(0);
            Record("SPS-010: Final physics handoff check failed after several bounded attempts", true);
        }
    }

    auto diagnosticsDue = diagnosticsRefreshDueMs.load();
    if (diagnosticsDue > 0 && now >= diagnosticsDue &&
        diagnosticsRefreshDueMs.compare_exchange_strong(diagnosticsDue, 0)) {
        RefreshDiagnostics();
    }

    auto playerBonesDue = playerBonesRefreshDueMs.load();
    if (playerBonesDue > 0 && now >= playerBonesDue &&
        playerBonesRefreshDueMs.compare_exchange_strong(playerBonesDue, 0)) {
        RefreshPlayerBoneDiagnostics();
    }

    if (recoveryController.ClaimExternalOwnerRepair(now)) {
        if (ConfirmCurrentPhysicsOwner("an external physics reset")) {
            // Keep the original retry budget until the completion callback.
        } else if (now < playerContext.recovery.externalOwnerRepairUntilMs.load()) {
            playerContext.recovery.externalOwnerRepairDueMs.store(now + 1000);
        } else {
            playerContext.recovery.externalOwnerRepairUntilMs.store(0);
            Record("SPS-010: Physics ownership could not be restored after an external reset", true);
        }
    }

    if (recoveryController.ClaimLoadSmpReset(now)) {
        RunLoadSMPReset();
    }

    if (recoveryController.ClaimSoftHandoffReset(
            now, playerContext.physics.usingCBPC.load())) {
        RunSoftHandoffSMPReset();
    }

    if (recoveryController.ClaimSoftAngleRefresh(
            now, playerContext.physics.usingCBPC.load())) {
        RunSoftAngleRefresh();
    }

    if (recoveryController.ClaimNodeCbpcReacquire(
            now, playerContext.physics.usingCBPC.load())) {
        if (playerContext.physics.known.load() && QueueOwnershipHandoff(
                true, SPS::Controllers::OwnershipPurpose::reconnectMesh)) {
            // The callback clears the budget only after reconnection executes.
        } else if (now < playerContext.recovery.nodeCbpcReacquireUntilMs.load()) {
            playerContext.recovery.nodeCbpcReacquireDueMs.store(now + 1000);
        } else {
            playerContext.recovery.nodeCbpcReacquireUntilMs.store(0);
            Record("SPS-010: Erect physics could not reconnect after the player equipment change", true);
        }
    }

    if (recoveryController.ClaimSoftConfirmation(
            now, playerContext.physics.usingCBPC.load())) {
        auto until = playerContext.recovery.softConfirmationUntilMs.load();
        if (until == 0) {
            until = now + 10000;
            playerContext.recovery.softConfirmationUntilMs.store(until);
        }
        if (ConfirmSoftState()) {
            // Keep the original retry budget until the completion callback.
        } else if (now < until) {
            playerContext.recovery.softConfirmationDueMs.store(now + 1000);
        } else {
            playerContext.recovery.softConfirmationUntilMs.store(0);
            Record("SPS-010: Soft-state confirmation stopped after several bounded attempts", true);
        }
    }

    if (recoveryController.ClaimCbpcConfirmation(
            now, playerContext.physics.usingCBPC.load())) {
        auto until = playerContext.recovery.cbpcConfirmationUntilMs.load();
        if (until == 0) {
            until = now + 10000;
            playerContext.recovery.cbpcConfirmationUntilMs.store(until);
        }
        if (ConfirmCBPCState()) {
            // Keep the original retry budget until the completion callback.
        } else if (now < until) {
            playerContext.recovery.cbpcConfirmationDueMs.store(now + 1000);
        } else {
            playerContext.recovery.cbpcConfirmationUntilMs.store(0);
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
        targetStillWantsCBPC = SPS::Core::MaintenanceWantsCBPC(
            NormalSettingsWantCBPC(copy), activeManualPhysicsTest.load(),
            now < manualPhysicsTestUntilMs.load());

    if (!targetStillWantsCBPC) {
        // No delayed erect-position callback may survive a decision to soften.
        playerContext.position.settleDueMs.store(0);
        playerContext.position.confirmationDueMs.store(0);
        playerContext.position.retryDueMs.store(0);
    }

    bool settledNow = false;
    if (!ownershipController.Read().pending && positionController.ClaimSettle(
            now, playerContext.physics.usingCBPC.load(), targetStillWantsCBPC)) {
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
    //
    // Never do this while a ramp is running, and never while one has failed and
    // is waiting to retry. ApplyRequestedBend overwrites animationTargetBend with
    // the value it is about to apply, so confirming mid-ramp both truncated the
    // rise and left the ramp aiming at whatever height it had already reached:
    // that is the visible rise, snap back, then slow climb again.
    const bool rampOwnsAngle = playerContext.position.animating.load() ||
        playerContext.position.retryDueMs.load() != 0;
    if (!rampOwnsAngle &&
        !ownershipController.Read().pending && positionController.ClaimConfirmation(
            now, playerContext.physics.usingCBPC.load(), targetStillWantsCBPC)) {
        ApplyRequestedBend(true, true, true);
    }

    // During a failed or delayed CBPC -> SMP handoff the confirmed owner can
    // still be CBPC for another tick even though the requested state is soft.
    // The old maintenance pass interpreted that gap as a lost erect angle and
    // started raising 0 back to 17/20 at zero arousal. Only maintain the erect
    // angle while the current control source still actually wants CBPC.
    if (!settledNow && !ownershipController.Read().pending &&
        playerContext.physics.usingCBPC.load() && targetStillWantsCBPC &&
        copy.positionControl && !playerContext.position.relaxing.load() &&
        !playerContext.position.animating.load()) {
        const int desired = DesiredBend(copy);
        const auto retryDue = playerContext.position.retryDueMs.load();
        if (playerContext.position.appliedBend.load() != desired && playerContext.position.settleDueMs.load() == 0 &&
            (retryDue == 0 || now >= retryDue))
            ApplyRequestedBend(true, copy.animatePosition, true);
    }

    if (recoveryController.ClaimNodeRefresh(now)) {
        if (ownershipController.Read().pending) {
            // Never release or reset bones under an ownership transaction that
            // has already been dispatched. Wait for its callback (or timeout)
            // so equipment recovery cannot invert the requested final owner.
            playerContext.recovery.nodeRefreshDueMs.store(now + 500);
        } else {
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
                if (const int held = playerContext.position.appliedBend.load(); held >= 0) {
                    heldBendBeforeReconnect.store(held);
                    heldBendUntilMs.store(now + kHeldBendWindowMs);
                }
                playerContext.position.appliedBend.store(-1);
                playerContext.position.settleDueMs.store(0);
                playerContext.position.confirmationDueMs.store(0);
                playerContext.recovery.cbpcConfirmationDueMs.store(0);
                playerContext.recovery.nodeCbpcReacquireDueMs.store(now + 250);
                playerContext.recovery.nodeCbpcReacquireUntilMs.store(now + 10000);
                Record("Player mesh changed; guarded erect-physics reconnect queued");
            } else if (playerContext.physics.known.load() && !playerContext.physics.usingCBPC.load()) {
                playerContext.recovery.softConfirmationDueMs.store(now + 750);
            }
            recoveryController.ScheduleNodeRefreshFollowup(now);
        }
    }

    if (recoveryController.ClaimNodeRefreshFollowup(now)) {
        // Some armour managers rebuild the genital node twice. This quiet
        // second confirmation catches the late rebuild without another reset.
        if (ownershipController.Read().pending ||
            !ConfirmCurrentPhysicsOwner("the completed equipment change")) {
            if (!recoveryController.RetryNodeRefreshFollowup(now)) {
                Record("SPS-010: Equipment follow-up stopped; use Repair current physics to retry", true);
            }
        } else {
            playerContext.recovery.nodeRefreshFollowupUntilMs.store(0);
            playerContext.position.appliedBend.store(-1);
            if (playerContext.physics.usingCBPC.load() && copy.positionControl)
                playerContext.recovery.erectMeshReplayDueMs.store(now + 1000);
            else if (!playerContext.physics.usingCBPC.load()) {
                // One more guarded reset for outfits whose nodes finish
                // rebuilding after the first reset ran. Soft shaft only.
                if (equipSecondResetPending.exchange(false) && RefreshSMPAfterPlayerMeshChange())
                    Record("Equipment change: second SMP reset queued after the late rebuild");
                else
                    playerContext.recovery.softConfirmationDueMs.store(now + 500);
            }
        }
    }

    if (recoveryController.ClaimErectMeshReplay(now)) {
        // Armour and schlong changes can replace the live skeleton after both
        // the CBPC handoff and the first bend request have already succeeded.
        // Replay the saved angle once after the replacement mesh is stable.
        if (ownershipController.Read().pending) {
            playerContext.recovery.erectMeshReplayDueMs.store(now + 500);
        } else if (playerContext.physics.known.load() && playerContext.physics.usingCBPC.load() && copy.positionControl) {
            CancelErectionAnimation();
            playerContext.position.appliedBend.store(-1);
            ApplyRequestedBend(true, true, true);
            Record("Erect angle restored after the replacement mesh settled");
        }
    }

    // FSMP can restore XML dynamics without an event SPS receives. Keep its
    // six managed bones off while the current policy still requests CBPC.
    // Do this last so real handoffs, resets and equipment repair take priority.
    // Not during a ramp: a pending maintenance transaction makes the ramp skip
    // and resend its current stage, which breaks the even step timing. The
    // probe resumes on the first tick after the ramp ends.
    if (ownershipController.CanMaintainCBPC(
            targetStillWantsCBPC,
            playerContext.position.relaxing.load() || playerContext.position.animating.load(),
            now)) {
        QueueOwnershipHandoff(true, SPS::Controllers::OwnershipPurpose::maintainCBPC);
    }
}

void StartPolling() {
    if (polling.exchange(true)) return;
    pollThread = std::jthread([](std::stop_token token) {
        auto nextPlayerTick = std::chrono::steady_clock::now();
        while (!token.stop_requested()) {
            int pollMs;
            bool basicEnabled;
            {
                std::scoped_lock lock(settingsLock);
                pollMs = settings.pollMs;
                basicEnabled = settings.enabled &&
                    ((settings.basicNpcEnabled && settings.basicNpcProviderSosDisabled) ||
                        settings.managedNpcEnabled);
            }
            const int wait = basicEnabled ? std::min(pollMs, 250) : pollMs;
            std::this_thread::sleep_for(std::chrono::milliseconds(wait));
            if (!token.stop_requested()) {
                if (auto* tasks = SKSE::GetTaskInterface(); tasks && !tickQueued.exchange(true)) {
                    const auto now = std::chrono::steady_clock::now();
                    const bool playerDue = now >= nextPlayerTick;
                    if (playerDue) nextPlayerTick = now + std::chrono::milliseconds(pollMs);
                    tasks->AddTask([playerDue] {
                        tickQueued.store(false);
                        if (playerDue) Tick();
                        else SPS::Runtime::UpdateExecutionClock();
                        Settings current;
                        { std::scoped_lock lock(settingsLock); current = settings; }
                        const auto ownership = ownershipController.Read();
                        const auto position = positionController.Read();
                        // Only work that holds the shared Papyrus operation gate (an
                        // owner handoff or a menu test) makes NPCs wait. The player's
                        // angle ramp and fall run on their own thread and do not take
                        // the gate, so they no longer stall NPC commands for their
                        // whole 4-8 s; each NPC dispatch still checks the gate itself.
                        const char* busyReason = !ownership.known ? "player owner not known yet" :
                            ownership.pending ? "player owner handoff pending" :
                            pendingQuickAction.load() >= 0 ? "menu physics action pending" :
                            activeManualPhysicsTest.load() >= 0 ? "menu physics test active" : nullptr;
                        const bool playerBusy = busyReason != nullptr;
                        {
                            // Changes always logged; the same reason at most every 10 s.
                            static std::string lastBusyReason;
                            static std::int64_t lastBusyLogMs = 0;
                            const std::string reasonText = busyReason ? busyReason : "";
                            const auto nowMs = NowMs();
                            if (reasonText != lastBusyReason &&
                                (reasonText.empty() || nowMs - lastBusyLogMs >= 10000)) {
                                if (!reasonText.empty())
                                    logger::info("NPC work is waiting for the player: {}", reasonText);
                                lastBusyReason = reasonText;
                                lastBusyLogMs = nowMs;
                            }
                        }
                        SPS::Runtime::TickBasicNpcs(current.enabled && current.basicNpcEnabled,
                            current.basicNpcProviderSosDisabled, papyrusDispatchAllowedAfterMs.load(), playerBusy);
                        SPS::Runtime::TickManagedNpcs(current.enabled && current.managedNpcEnabled,
                            current, papyrusDispatchAllowedAfterMs.load(), playerBusy);
                        if (playerDue && current.enabled) {
                            std::vector<std::uint32_t> watched{ SPS::API::kPlayerFormID };
                            for (const auto& actor : SPS::Runtime::ReadManagedNpcStatus().actors)
                                watched.push_back(actor.formID);
                            SPS::Runtime::TickActorIdentities(watched, papyrusDispatchAllowedAfterMs.load());
                        }
                    });
                }
            }
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

void RefreshPlayerBoneDiagnostics() {
    const int found = SPS::Diagnostics::CountPlayerBones();
    int previous = 0;
    {
        std::scoped_lock lock(diagnosticsLock);
        previous = diagnostics.playerBonesFound;
        diagnostics.playerBonesFound = found;
    }
    if (previous != found)
        Record(fmt::format("Compatible schlong check updated: {}/6 live physics bones found", found));
}

void StatusLine(const char* label, const char* status, int level) {
    ImGuiMCP::Text("%s", label);
    ImGuiMCP::SameLine(245.0F);
    const ImGuiMCP::ImVec4 color = level == 2 ? ImGuiMCP::ImVec4(0.35F, 1.0F, 0.45F, 1.0F)
        : level == 1 ? ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F)
        : ImGuiMCP::ImVec4(1.0F, 0.35F, 0.35F, 1.0F);
    ImGuiMCP::TextColored(color, "%s", status);
}

// Settings live in SKSE\Plugins\SchlongPhysicsSwapper.ini. Most of them are not
// exposed here on purpose: the menu only carries the choices people actually
// change, and the rest have sane defaults that can be edited in the file.
void ReloadSettingsFromDisk() {
    const auto result = SPS::Core::LoadSettings(kIni, kLegacyIni);
    {
        std::scoped_lock lock(settingsLock);
        settings = result.settings;
    }
    settingsLoaded = true;
    SPS::Runtime::ConfigureBasicNpcCommands(result.settings.enabled && result.settings.basicNpcEnabled &&
        result.settings.basicNpcProviderSosDisabled);
    // Managed commands are permission-gated the same way. Without this, a saved
    // ManagedNPCEnabled=true stayed inactive until the checkbox was toggled.
    SPS::Runtime::ConfigureManagedNpcCommands(result.settings.enabled && result.settings.managedNpcEnabled);
    Record("Settings reloaded from SchlongPhysicsSwapper.ini");
}

void OpenSettingsFile() {
    const auto path = std::filesystem::absolute(kIni);
    ShellExecuteW(nullptr, L"open", L"notepad.exe", path.wstring().c_str(), nullptr, SW_SHOWNORMAL);
}

void PageHeading(const char* title, const char* description) {
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "%s", title);
    if (description && description[0] != '\0')
        ImGuiMCP::TextWrapped("%s", description);
    ImGuiMCP::Separator();
}

void SectionHeading(const char* title) {
    // SeparatorText gives each group a titled rule, which reads as structure
    // rather than another line of coloured text competing with the controls.
    ImGuiMCP::Spacing();
    ImGuiMCP::SeparatorText(title);
}

// One-line explanation under a control. Kept short on purpose: a wall of text
// under every slider is what made the old pages hard to scan.
void Hint(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGuiMCP::TextDisabledV(fmt, args);
    va_end(args);
}

// A setting that only matters once the feature above it is on. Disabling
// rather than hiding keeps the layout stable so nothing appears to jump.
void BeginDependent(bool enabled) {
    ImGuiMCP::BeginDisabled(!enabled);
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
    value.poseAngleDegrees = 45.0F;
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
    const bool basicChanged = copy.enabled != previous.enabled ||
        copy.basicNpcEnabled != previous.basicNpcEnabled ||
        copy.basicNpcProviderSosDisabled != previous.basicNpcProviderSosDisabled;
    const bool managedChanged = copy.enabled != previous.enabled ||
        copy.managedNpcEnabled != previous.managedNpcEnabled;
    const bool softPositionChanged = copy.flaccidAngleControl != previous.flaccidAngleControl ||
        copy.flaccidBend != previous.flaccidBend;
    const bool positionChanged = copy.erectBend != previous.erectBend || softPositionChanged ||
        copy.sexLabBend != previous.sexLabBend || copy.useSexLabBend != previous.useSexLabBend ||
        copy.positionControl != previous.positionControl || copy.bendMethod != previous.bendMethod ||
        copy.poseAngleDegrees != previous.poseAngleDegrees ||
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
    // The pose natives receive -1 from Papyrus to mean "use the INI value", so
    // the module needs the current setting rather than a compiled constant.
    SPS::Runtime::SosPose::SetConfiguredDegrees(copy.poseAngleDegrees);
    if (basicChanged) SPS::Runtime::ConfigureBasicNpcCommands(copy.enabled && copy.basicNpcEnabled &&
        copy.basicNpcProviderSosDisabled);
    if (managedChanged) SPS::Runtime::ConfigureManagedNpcCommands(copy.enabled && copy.managedNpcEnabled);
    if (!copy.equipmentChangeRecovery) {
        playerContext.recovery.nodeRefreshDueMs.store(0);
        playerContext.recovery.nodeRefreshFollowupDueMs.store(0);
        playerContext.recovery.nodeRefreshFollowupUntilMs.store(0);
    }
    if (previous.enabled && !copy.enabled) {
        ClearAPIRequests();
        CancelErectionAnimation();
        recoveryController.ResetTransient();
        activeManualPhysicsTest.store(-1);
        manualPhysicsTestUntilMs.store(0);
        pendingQuickAction.store(-1);
        pendingQuickActionUntilMs.store(0);
    }
    spdlog::set_level(copy.verboseLogging ? spdlog::level::debug : spdlog::level::info);
    Save(false);
    if (positionChanged) {
        CancelErectionAnimation();
        ResetPositionRecovery();
        playerContext.position.appliedBend.store(-1);
        if (playerContext.physics.known.load() && playerContext.physics.usingCBPC.load())
            playerContext.position.confirmationDueMs.store(NowMs() + 1000);
    }
    if (randomSettingsChanged) {
        CancelErectionAnimation();
        playerContext.spontaneous.randomNextMs.store(0);
        playerContext.spontaneous.randomUntilMs.store(0);
        playerContext.spontaneous.randomManualTest.store(false);
        playerContext.position.relaxing.store(false);
        playerContext.spontaneous.refractoryUntilMs.store(0);
        playerContext.spontaneous.morningDueMs.store(0);
        playerContext.spontaneous.morningActive.store(false);
    }
    if (auto* tasks = SKSE::GetTaskInterface()) {
        tasks->AddTask([positionChanged, softPositionChanged, basicChanged, managedChanged,
            enabled = copy.enabled, positionEnabled = copy.positionControl] {
            if (basicChanged) SPS::Runtime::ResetBasicNpcs();
            if (managedChanged) SPS::Runtime::ResetManagedNpcs();
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
    const auto ownership = ownershipController.Read();
    bool changed = false;

    Diagnostics d;
    { std::scoped_lock lock(diagnosticsLock); d = diagnostics; }
    const bool healthChecked = d.checkedAtMs > 0;
    const bool coreReady = CoreReady(d, copy);

    PageHeading("HOME", "See what SPS is doing and change its everyday behaviour.");

    SectionHeading("CURRENT STATUS");
    StatusLine("SPS", !healthChecked ? "Checking setup..." :
        (coreReady ? (ownership.known ? "Ready" : "Waiting for the player") : "Needs attention"),
        !healthChecked || !ownership.known ? 1 : (coreReady ? 2 : 0));
    StatusLine("Physics", ownership.known ?
        (ownership.usingCBPC ? "Erect (CBPC)" : "Soft (SMP)") : "Not decided yet",
        ownership.known ? 2 : 1);
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
            // Only keep the hidden rising start consistent when the feature is
            // actually in use; otherwise a threshold change would silently
            // rewrite a value the user cannot see.
            if (copy.arousalBasedErection && copy.threshold <= copy.erectionStartArousal)
                copy.erectionStartArousal = std::max(0.0F, copy.threshold - 1.0F);
            changed = true;
        }
        if (copy.arousalBasedErection) {
            if (ImGuiMCP::SliderFloat("Rising starts at arousal", &copy.erectionStartArousal, 0.0F, 99.0F, "%.0f")) {
                if (copy.erectionStartArousal >= copy.threshold)
                    copy.threshold = std::min(100.0F, copy.erectionStartArousal + 1.0F);
                changed = true;
            }
            Hint("The angle rises between %.0f and %.0f arousal, so it follows the value instead of jumping.",
                copy.erectionStartArousal, copy.threshold);
        } else {
            Hint("Erect at %.0f arousal, soft again below %.0f.",
                copy.threshold, std::max(0.0F, copy.threshold - copy.hysteresis));
        }
        changed |= ImGuiMCP::SliderFloat("Soft return gap", &copy.hysteresis, 0, 25, "%.0f arousal");
        Hint("Arousal must fall below %.0f before SPS returns to soft. Widen this if the angle flickers.",
            std::max(0.0F, copy.threshold - copy.hysteresis));
    } else if (copy.mode == 1) {
        Hint("Keeps the player soft with SMP. An arousal mod is not required.");
    } else {
        Hint("Keeps the player erect with CBPC. An arousal mod is not required.");
    }

    ImGuiMCP::Separator();
    SectionHeading("AROUSAL TEST");
    // Manual arousal writes used to live on the NPCs page. It is a test control
    // for the player, so it belongs beside the player's own settings.
    const bool oslReady = OslArousedLoaded() && !SloArousedLoaded() && !ClassicArousedLoaded();
    ImGuiMCP::BeginDisabled(!oslReady);
    static float playerArousalSlider{ 0.0F };
    static bool playerArousalEditing{ false };
    if (!playerArousalEditing && arousalController.Valid()) playerArousalSlider = arousalController.Value();
    if (ImGuiMCP::SliderFloat("Player OSL arousal", &playerArousalSlider, 0.0F, 100.0F, "%.0f"))
        playerArousalEditing = true;
    if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
        RequestPlayerOslArousalWrite(playerArousalSlider);
        playerArousalEditing = false;
    }
    ImGuiMCP::EndDisabled();
    Hint(oslReady ? "Writes the real OSL Aroused value when released; OSL can change it again afterwards."
                  : "Current OSL Aroused is required for this test control.");

    ImGuiMCP::Separator();
    SectionHeading("SETTINGS FILE");
    if (ImGuiMCP::Button("Open settings file")) OpenSettingsFile();
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Reload settings")) ReloadSettingsFromDisk();
    Hint("Most settings are only in the file. Reload applies your edits without restarting.");
    if (ImGuiMCP::Button("Refresh status"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { QueryArousal(true); QuerySexLab(); QuerySexLabRole(); QueryOStimRole(); Evaluate(); });
    if (ImGuiMCP::CollapsingHeader("Reset options")) {
        Hint("Restores the recommended SPS settings. This changes nothing outside SPS.");
        if (ImGuiMCP::Button("Restore recommended settings")) { UseRecommendedSettings(copy); changed = true; }
    }
    if (changed) SaveSettingsAndApply(copy, previous);
}

void __stdcall RenderLooks() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const Settings previous = copy;
    const auto ownership = ownershipController.Read();
    bool changed = false;

    PageHeading("APPEARANCE", "The soft and erect angles, and how SPS moves between them.");

    // Angle first: it is the visible setting. Transition speed only shapes how
    // fast the two angles are reached, so it reads better underneath the poses.
    SectionHeading("POSITIONS");
    const bool softAngleAvailable = SoftAngleControlAvailable(copy.bendMethod);
    if (!copy.positionControl) {
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F),
            "Angle control is turned off below. Physics switching still works.");
    }

    ImGuiMCP::BeginDisabled(!copy.positionControl);
    changed |= ImGuiMCP::SliderInt("Erect angle", &copy.erectBend, 0, 20);
    Hint("0 points straight out. 20 is the highest position this skeleton offers.");

    // One slider instead of a checkbox plus a slider: 0 already means "leave
    // the skeleton's own floppy pose alone", so there is nothing extra to hold.
    // The flag is only written on an actual edit, so it never lands in the INI
    // as a change the user did not make.
    if (softAngleAvailable) {
        int softAngle = copy.flaccidBend;
        if (ImGuiMCP::SliderInt("Soft angle", &softAngle, 0, 20,
            softAngle == 0 ? "0 (normal hanging position)" : "%d")) {
            copy.flaccidBend = softAngle;
            copy.flaccidAngleControl = softAngle > 0;
            changed = true;
        }
    }
    ImGuiMCP::EndDisabled();
    if (!softAngleAvailable)
        Hint("Custom soft angles need SOS AE or the SPS pose angle. Legacy SOS and TNG keep their own floppy pose.");

    ImGuiMCP::Separator();
    SectionHeading("TIMING");
    float erectionSeconds = copy.erectionDurationMs / 1000.0F;
    if (ImGuiMCP::SliderFloat("Rise time", &erectionSeconds, 0.5F, 30.0F, "%.1f seconds")) {
        copy.erectionDurationMs = static_cast<int>(std::lround(erectionSeconds * 1000.0F));
        // Any duration is a valid timed rise, so the old separate toggle is
        // redundant: the slider itself expresses "how fast".
        copy.gradualErection = true;
        changed = true;
    }
    float softeningSeconds = copy.softeningDurationMs / 1000.0F;
    if (ImGuiMCP::SliderFloat("Fall time", &softeningSeconds, 0.5F, 30.0F, "%.1f seconds")) {
        copy.softeningDurationMs = static_cast<int>(std::lround(softeningSeconds * 1000.0F));
        copy.gradualErection = true;
        changed = true;
    }
    // The graph binds 19 positions (SOSBend-9 to SOSBend9), so a full climb is
    // 18 even steps. The honest readout is how long each step holds.
    Hint("Rise and fall times are the full -9 to 9 travel time, in 18 even steps: "
         "about %.2f s per step up, %.2f s per step down.",
        (copy.erectionDurationMs / 1000.0F) / 18.0F, (copy.softeningDurationMs / 1000.0F) / 18.0F);

    ImGuiMCP::BeginDisabled(!ownership.known || !ownership.usingCBPC);
    if (ImGuiMCP::Button("Preview rise and fall")) {
        const int target = copy.erectBend;
        const int riseMs = copy.erectionDurationMs;
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask([target, riseMs] {
                playerContext.position.appliedBend.store(-1);
                StartBendAnimation(0, target, riseMs, false);
            });
        Record("Previewing the angle timing");
    }
    ImGuiMCP::SameLine();
    ImGuiMCP::BeginDisabled(!copy.positionControl || !ownership.known);
    if (ImGuiMCP::Button("Reapply angle")) {
        ResetPositionRecovery();
        playerContext.position.appliedBend.store(-1);
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask([] { ApplyRequestedBend(true, true, false); });
    }
    ImGuiMCP::EndDisabled();
    ImGuiMCP::EndDisabled();
    Hint("Preview plays one ramp and back. Reapply is only needed if the physics looks stale.");

    if (ImGuiMCP::CollapsingHeader("Advanced")) {
        // Everything here has a working default. It is collapsed rather than
        // removed so a genuine problem can still be diagnosed in game, while
        // the everyday view stays short.
        changed |= ImGuiMCP::Checkbox("Let SPS control the angle", &copy.positionControl);
        BeginDependent(copy.positionControl);
        // Choice -> Position/Method: 0 stages (1), 1 automatic (2), 2 pose (3).
        // Automatic used to write 3, the experimental pose angle, which the
        // animation graph overwrites every frame (docs/SOSAE_4_POSE_ANGLE_2026-11-01.md),
        // so touching this combo silently stopped the angle from moving.
        // Method 0 (the removed SOS AE native) behaves like automatic.
        const char* bendMethods[]{ "SOS / TNG animation stages", "Choose automatically (recommended)",
            "SPS pose angle (experimental)" };
        int methodChoice = copy.bendMethod == SPS::Runtime::kBendMethodEvents ? 0 :
            copy.bendMethod == SPS::Runtime::kBendMethodPose ? 2 : 1;
        if (ImGuiMCP::Combo("Method", &methodChoice, bendMethods, 3)) {
            copy.bendMethod = methodChoice == 0 ? SPS::Runtime::kBendMethodEvents :
                methodChoice == 2 ? SPS::Runtime::kBendMethodPose : SPS::Runtime::kBendMethodAutomatic;
            changed = true;
        }
        if (copy.bendMethod == SPS::Runtime::kBendMethodPose) {
            ImGuiMCP::TextDisabled("Experimental: writes the base bone directly. The SOS/TNG animation "
                "usually overwrites it, so the angle may not move. Use automatic instead.");
        } else {
            ImGuiMCP::TextDisabled("SOS/TNG animation stages: 19 positions, SOSBend-9 to SOSBend9.");
        }
        changed |= ImGuiMCP::Checkbox("Smooth angle changes", &copy.animatePosition);
        changed |= ImGuiMCP::Checkbox("Stop repeated bouncing", &copy.bounceGuard);
        changed |= ImGuiMCP::Checkbox("Rise gradually with arousal", &copy.arousalBasedErection);
        changed |= ImGuiMCP::Checkbox("Allow random erections", &copy.randomErections);
        BeginDependent(copy.randomErections);
        changed |= ImGuiMCP::SliderInt("Shortest interval (minutes)", &copy.randomErectionMinMinutes, 1, 120);
        changed |= ImGuiMCP::SliderInt("Longest interval (minutes)", &copy.randomErectionMaxMinutes, 1, 240);
        changed |= ImGuiMCP::SliderInt("Duration (seconds)", &copy.randomErectionDurationSeconds, 5, 600);
        ImGuiMCP::EndDisabled();
        ImGuiMCP::EndDisabled();
        Hint("Timing, poll rate and failure thresholds are in the settings file.");
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

    PageHeading("SCENES", "How SPS behaves during SexLab or OStim scenes. Nothing here affects normal gameplay.");

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

void __stdcall RenderNPCs() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const Settings previous = copy;
    bool changed = false;
    const ImGuiMCP::ImVec4 green(0.45F, 0.85F, 0.45F, 1.0F);
    const ImGuiMCP::ImVec4 amber(1.0F, 0.78F, 0.25F, 1.0F);
    const ImGuiMCP::ImVec4 grey(0.75F, 0.75F, 0.75F, 1.0F);

    // Window-scoped; reset at the end of this page so other pages keep their size.
    ImGuiMCP::SetWindowFontScale(1.15F);

    PageHeading("NPCs", "Choose which nearby NPCs SPS controls. Everything else is left to your arousal mod.");
    if (!copy.enabled)
        ImGuiMCP::TextColored(amber, "SPS is turned off on the Home page, so NPC control is off.");

    const auto managed = SPS::Runtime::ReadManagedNpcStatus();

    // Every registry access is game-thread-only, so the render callback only
    // queues an intent by FormID and the tick applies it.
    const auto queueAdd = [](std::uint32_t formID) {
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask([formID] {
                if (!SPS::Runtime::QueueManagedNpcAdd(formID))
                    Record(fmt::format("Could not queue managed NPC {:08X}", formID), true);
            });
    };
    const auto queueRemove = [](std::uint32_t formID) {
        if (auto* tasks = SKSE::GetTaskInterface())
            tasks->AddTask([formID] { SPS::Runtime::QueueManagedNpcRemove(formID); });
    };

    // ---- Managed NPCs: the part most people come here for ----
    SectionHeading("MANAGED NPCS");
    changed |= ImGuiMCP::Checkbox("Enable managed NPCs", &copy.managedNpcEnabled);
    ImGuiMCP::TextWrapped("A managed NPC gets full SPS physics: soft or erect depending on its own arousal. "
        "The list is forgotten when you close the game.");
    ImGuiMCP::BeginDisabled(!copy.managedNpcEnabled);
    changed |= ImGuiMCP::Checkbox("Allow up to 8 managed NPCs (default is 1)", &copy.managedNpcUpToEight);
    ImGuiMCP::EndDisabled();
    if (copy.managedNpcUpToEight) {
        ImGuiMCP::TextColored(amber, "Warning: every managed NPC runs its own SMP/CBPC handoffs and Papyrus checks. "
            "Several at once can cost frame rate and script time, especially in busy scenes. Not tested with more than two.");
    } else if (managed.actors.size() > managed.capacity) {
        ImGuiMCP::TextColored(amber, "%zu NPCs are managed but the limit is %zu. Remove the extras; "
            "none can be added until you do.", managed.actors.size(), managed.capacity);
    }
    ImGuiMCP::Spacing();

    if (managed.actors.empty()) {
        ImGuiMCP::TextColored(grey, "No NPC is managed yet. Pick one from the list below, or look at one and press Add NPC in crosshair.");
    }
    struct ManagedSliderState { float value{ 0.0F }; bool editing{ false }; };
    static std::unordered_map<std::uint32_t, ManagedSliderState> managedSliders;
    std::erase_if(managedSliders, [&managed](const auto& item) {
        return std::none_of(managed.actors.begin(), managed.actors.end(),
            [formID = item.first](const auto& actor) { return actor.formID == formID; });
    });
    const bool oslReadyManaged = OslArousedLoaded() && !SloArousedLoaded() && !ClassicArousedLoaded();
    for (const auto& actor : managed.actors) {
        ImGuiMCP::TextColored(green, "%s", actor.name.c_str());
        ImGuiMCP::SameLine();
        if (actor.arousal)
            ImGuiMCP::Text("- %s, arousal %.0f", actor.physics.c_str(), *actor.arousal);
        else
            ImGuiMCP::Text("- %s, waiting for arousal", actor.physics.c_str());
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button(fmt::format("Remove###sps-managed-remove-{:08X}", actor.formID).c_str()))
            queueRemove(actor.formID);
        if (oslReadyManaged) {
            auto& slider = managedSliders[actor.formID];
            if (!slider.editing && actor.arousal) slider.value = *actor.arousal;
            const auto label = fmt::format("Set arousal###sps-managed-arousal-{:08X}", actor.formID);
            if (ImGuiMCP::SliderFloat(label.c_str(), &slider.value, 0.0F, 100.0F, "%.0f"))
                slider.editing = true;
            if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
                const auto formID = actor.formID;
                const auto value = slider.value;
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([formID, value] {
                        if (!SPS::Runtime::QueueManagedNpcArousalWrite(formID, value))
                            Record(fmt::format("Could not queue an OSL arousal write for managed NPC {:08X}", formID), true);
                    });
                }
                slider.editing = false;
            }
        }
        // This NPC's own settings. They are separate from the player's.
        if (ImGuiMCP::CollapsingHeader(fmt::format("Settings for {}###sps-managed-opt-{:08X}",
                actor.name, actor.formID).c_str())) {
            // Local copy while dragging, so the page does not snap back to the
            // last published value; the file is written when the control is released.
            struct OptionState { SPS::Runtime::ManagedNpcOptions value; bool editing{ false }; };
            static std::unordered_map<std::uint32_t, OptionState> optionStates;
            auto& state = optionStates[actor.formID];
            if (!state.editing) state.value = actor.options;
            auto& o = state.value;
            bool edited = false, released = false;
            const auto track = [&](bool changedNow) {
                edited |= changedNow;
                released |= ImGuiMCP::IsItemDeactivatedAfterEdit();
            };
            track(ImGuiMCP::SliderFloat(fmt::format("Erect at arousal###sps-o-at-{:08X}", actor.formID).c_str(),
                &o.erectAt, 0.0F, 100.0F, "%.0f"));
            track(ImGuiMCP::SliderInt(fmt::format("Erect angle###sps-o-ang-{:08X}", actor.formID).c_str(),
                &o.angle, 1, 20));
            if (ImGuiMCP::Checkbox(fmt::format("Rise with arousal###sps-o-follow-{:08X}", actor.formID).c_str(),
                    &o.followArousal)) { edited = true; released = true; }
            track(ImGuiMCP::SliderInt(fmt::format("Rise time (seconds)###sps-o-rise-{:08X}", actor.formID).c_str(),
                &o.riseSeconds, 1, 30));
            track(ImGuiMCP::SliderInt(fmt::format("Fall time (seconds)###sps-o-fall-{:08X}", actor.formID).c_str(),
                &o.fallSeconds, 1, 30));
            if (edited) state.editing = !released;
            if (edited || released) {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    const auto formID = actor.formID;
                    const auto value = o;
                    const bool save = released;
                    tasks->AddTask([formID, value, save] { SPS::Runtime::SetManagedNpcOptions(formID, value, save); });
                }
            }
        }
    }
    if (oslReadyManaged && !managed.actors.empty())
        Hint("Writes OSL's real value when you let go. OSL can change it again afterwards.");
    if (!managed.rejection.empty())
        ImGuiMCP::TextColored(amber, "Could not do that: %s", managed.rejection.c_str());

    ImGuiMCP::Spacing();
    if (ImGuiMCP::Button("Add NPC in crosshair")) {
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([] {
                const auto target = SPS::Runtime::ResolveManagedNpcTarget();
                if (!target) Record("No NPC in the crosshair to manage", true);
                else if (!SPS::Runtime::QueueManagedNpcAdd(target))
                    Record(fmt::format("Could not queue managed NPC {:08X}", target), true);
            });
        }
    }
    ImGuiMCP::SameLine();
    ImGuiMCP::BeginDisabled(managed.actors.empty());
    if (ImGuiMCP::Button("Remove all")) {
        for (const auto& actor : managed.actors) queueRemove(actor.formID);
    }
    ImGuiMCP::EndDisabled();

    // ---- Nearby list ----
    SectionHeading("NEARBY NPCS");
    if (!copy.managedNpcEnabled || !copy.enabled)
        ImGuiMCP::TextColored(grey, "Turn on Enable managed NPCs to see who is nearby.");
    else if (managed.nearby.empty())
        ImGuiMCP::TextColored(grey, "Nobody in range. Move closer to the NPC you want.");
    const bool atLimit = managed.actors.size() >= managed.capacity;
    for (const auto& candidate : managed.nearby) {
        const auto meters = candidate.distance / 70.0F;  // ~70 units per metre
        ImGuiMCP::Text("%s", candidate.name.c_str());
        ImGuiMCP::SameLine(300.0F);
        ImGuiMCP::TextColored(grey, "%.0f m", meters);
        ImGuiMCP::SameLine(380.0F);
        if (candidate.managed) {
            ImGuiMCP::TextColored(green, "Managed");
            continue;
        }
        if (!candidate.unavailable.empty()) {
            ImGuiMCP::TextColored(grey, "%s", candidate.unavailable.c_str());
            continue;
        }
        ImGuiMCP::BeginDisabled(atLimit);
        if (ImGuiMCP::Button(fmt::format("Manage###sps-managed-add-{:08X}", candidate.formID).c_str()))
            queueAdd(candidate.formID);
        ImGuiMCP::EndDisabled();
    }
    if (atLimit && !managed.nearby.empty())
        ImGuiMCP::TextColored(amber, "Limit reached (%zu). Remove one to manage another.", managed.capacity);

    // ---- Optional and technical sections, closed by default ----
    ImGuiMCP::Spacing();
    ImGuiMCP::Separator();

    // Kept outside the collapsible section so the safety rule always applies.
    if (!copy.basicNpcProviderSosDisabled && copy.basicNpcEnabled) { copy.basicNpcEnabled = false; changed = true; }

    if (ImGuiMCP::CollapsingHeader("Everyone else (basic NPC poses)")) {
        ImGuiMCP::TextWrapped("Poses other nearby NPCs soft or erect from their arousal, without SMP/CBPC control. "
            "Turn off Enable SOS in OSL Aroused first so the two do not fight, then tick the box below.");
        changed |= ImGuiMCP::Checkbox("I turned off OSL's SOS control", &copy.basicNpcProviderSosDisabled);
        ImGuiMCP::BeginDisabled(!copy.basicNpcProviderSosDisabled);
        changed |= ImGuiMCP::Checkbox("Pose other NPCs", &copy.basicNpcEnabled);
        ImGuiMCP::EndDisabled();
        Hint("NPCs wait during scenes. Turning this off leaves their last pose.");

        const auto npc = SPS::Runtime::ReadBasicNpcStatus();
        ImGuiMCP::Spacing();
        ImGuiMCP::Text("%s", npc.state.c_str());
        Hint("Nearby: %u / 8", npc.tracked);

        ImGuiMCP::Spacing();
        ImGuiMCP::Text("Set arousal (test)");
        Hint("Writes OSL's real value when you let go. OSL can change it again afterwards.");
        const bool oslReady = OslArousedLoaded() && !SloArousedLoaded() && !ClassicArousedLoaded();
        ImGuiMCP::BeginDisabled(!oslReady);
        struct SliderState { float value{ 0.0F }; bool editing{ false }; };
        static std::unordered_map<std::uint32_t, SliderState> npcSliders;
        std::erase_if(npcSliders, [&npc](const auto& item) {
            return std::none_of(npc.actors.begin(), npc.actors.end(),
                [formID = item.first](const auto& actor) { return actor.formID == formID; });
        });
        if (!oslReady) npcSliders.clear();
        for (const auto& actor : npc.actors) {
            auto& slider = npcSliders[actor.formID];
            if (!slider.editing && actor.arousal) slider.value = *actor.arousal;
            const auto label = fmt::format("{}###OSL NPC arousal {:08X}", actor.name, actor.formID);
            if (ImGuiMCP::SliderFloat(label.c_str(), &slider.value, 0.0F, 100.0F, "%.0f"))
                slider.editing = true;
            if (ImGuiMCP::IsItemDeactivatedAfterEdit()) {
                // The basic NPC pool is game-thread-only. This render callback must
                // not read or write it directly; the request is queued by ID so the
                // pool can also be rebuilt or reset before the task runs.
                const auto formID = actor.formID;
                const auto value = slider.value;
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([formID, value] {
                        if (!SPS::Runtime::QueueBasicNpcOslArousalWrite(formID, value,
                                papyrusDispatchAllowedAfterMs.load()))
                            Record(fmt::format("Could not queue an OSL arousal write for NPC {:08X}", formID), true);
                    });
                }
                slider.editing = false;
            }
        }
        ImGuiMCP::EndDisabled();
        if (!oslReady) Hint("Needs OSL Aroused, with no other arousal mod taking priority.");
        else if (npc.actors.empty()) Hint("No eligible NPC nearby yet.");
    }

    if (ImGuiMCP::CollapsingHeader("Schlong and SexLab")) {
        const bool sosAe = SosAeNativeLoaded();
        const bool sexLab = PluginLoaded({ "SexLab.esm" });
        if (!sosAe && !sexLab)
            ImGuiMCP::TextColored(grey, "Needs Schlongs of Skyrim AE or SexLab P+ to show this.");
        else
            ImGuiMCP::TextWrapped("Shows the schlong and SexLab sex of you and your managed NPCs. "
                "Make futa uses SexLab's own futa override. SPS remembers it in this save and re-applies it if SexLab forgets.");
        std::vector<std::pair<std::uint32_t, std::string>> people{ { SPS::API::kPlayerFormID, "Player" } };
        for (const auto& actor : managed.actors) people.emplace_back(actor.formID, actor.name);
        for (const auto& [formID, name] : people) {
            const auto identity = SPS::Runtime::ReadActorIdentity(formID);
            if (!identity || !identity->known) {
                ImGuiMCP::Text("%s: reading...", name.c_str());
                continue;
            }
            std::string schlong = !identity->sosRead ? "SOS AE not detected" :
                identity->addon.empty() ? "no schlong" :
                fmt::format("{} (size {})", identity->addon, identity->rank);
            std::string sex = !identity->sexLabRead ? "SexLab not detected" :
                fmt::format("SexLab: {}{}", SPS::Runtime::SexLabSexName(identity->sex),
                    identity->sex == 2 && identity->baseSex != 2 ? " (forced)" :
                    identity->baseSex != identity->sex ?
                        fmt::format(" (base: {})", SPS::Runtime::SexLabSexName(identity->baseSex)) : "");
            ImGuiMCP::TextColored(green, "%s", name.c_str());
            ImGuiMCP::Text("%s, %s%s", schlong.c_str(), sex.c_str(),
                identity->rememberedFuta ? ", remembered in this save" : "");
            if (!identity->sexLabRead) continue;
            const bool forced = identity->rememberedFuta || (identity->sex == 2 && identity->baseSex != 2);
            if (SPS::Runtime::FutaEligible(*identity) && identity->sex != 2) {
                if (ImGuiMCP::Button(fmt::format("Make futa in SexLab###sps-futa-{:08X}", formID).c_str()))
                    SPS::Runtime::RequestFuta(formID, true);
            } else if (forced) {
                if (ImGuiMCP::Button(fmt::format("Undo futa###sps-unfuta-{:08X}", formID).c_str()))
                    SPS::Runtime::RequestFuta(formID, false);
            } else if (identity->baseSex != 0 && identity->baseSex != 1) {
                if (identity->sex != 2) {
                    if (ImGuiMCP::Button(fmt::format("Force futa in SexLab###sps-forcefuta-{:08X}", formID).c_str()))
                        SPS::Runtime::RequestFuta(formID, true);
                    Hint(fmt::format("SexLab's base sex for this character is {}. Forcing futa overrides it; creature animations may no longer match.",
                        SPS::Runtime::SexLabSexName(identity->baseSex)).c_str());
                }
            } else if (identity->sosRead && identity->addon.empty()) {
                Hint("No futa button: give them a schlong in SOS first.");
            }
            ImGuiMCP::Spacing();
        }
    }

    if (ImGuiMCP::CollapsingHeader("Technical details")) {
        const auto npc = SPS::Runtime::ReadBasicNpcStatus();
        Hint("Managed NPCs");
        ImGuiMCP::TextWrapped("%s (limit %zu)", managed.state.c_str(), managed.capacity);
        if (!managed.actors.empty())
            ImGuiMCP::TextWrapped("Commands sent: %llu physics changes, %llu poses applied, %llu deferred, %llu failed, %llu blocked. "
                "These count commands, so check the NPC in game for visible motion.",
                static_cast<unsigned long long>(managed.switched), static_cast<unsigned long long>(managed.applied),
                static_cast<unsigned long long>(managed.deferred), static_cast<unsigned long long>(managed.failed),
                static_cast<unsigned long long>(managed.blocked));
        ImGuiMCP::Spacing();
        Hint("Basic NPC poses");
        ImGuiMCP::TextWrapped("Waiting for information: %u. In scenes: %u. Pending requests: %u.",
            npc.waiting, npc.inScene, npc.pending);
        ImGuiMCP::TextWrapped("%llu completed, %llu scene deferrals, %llu failed or expired, %llu waiting for a bridge or free slot.",
            static_cast<unsigned long long>(npc.completed), static_cast<unsigned long long>(npc.deferred),
            static_cast<unsigned long long>(npc.failed), static_cast<unsigned long long>(npc.blocked));
    }

    ImGuiMCP::SetWindowFontScale(1.0F);
    if (changed) SaveSettingsAndApply(copy, previous);
}

std::string BuildReport() {
    Diagnostics d;
    { std::scoped_lock lock(diagnosticsLock); d = diagnostics; }
    Settings s;
    { std::scoped_lock lock(settingsLock); s = settings; }
    const auto activity = activityLog.Read();
    const auto ownership = ownershipController.Read();
    const auto position = positionController.Read();
    const auto recovery = recoveryController.Read();
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
    const bool ppaLoaded = ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr;
    SPS::Diagnostics::SupportReportInput report;
    report.version = kVersion;
    report.runtimeVersion = runtimeVersion;
    report.skseVersion = skseVersion;
    report.components = {
        LoadedDllVersion(L"SKSEMenuFramework.dll"),
        LoadedDllVersion(L"OSLAroused.dll"),
        LoadedDllVersion(L"SexlabArousedNG.dll"),
        LoadedDllVersion(L"hdtsmp64.dll"),
        LoadedDllVersion(L"cbp.dll"),
        LoadedDllVersion(L"SexLabUtil.dll"),
        LoadedDllVersion(L"OStim.dll"),
        SosAeNativeModuleName() ? LoadedDllVersion(SosAeNativeModuleName()) : "not loaded"
    };
    report.diagnostics = std::move(d);
    report.ownership = ownership;
    report.position = position;
    report.recovery = recovery;
    report.scenes = {
        sceneController.State().sexLab.active.load(),
        sceneController.State().sexLab.connected.load(),
        SexLabRoleName(sceneController.State().sexLab.role.load()),
        sceneController.State().sexLab.roleValid.load(),
        sceneController.State().ostim.active.load(),
        sceneController.State().ostim.connected.load(),
        OStimRoleName(sceneController.State().ostim.role.load()),
        sceneController.State().ostim.roleValid.load()
    };
    report.activity = activity;
    report.apiStats = apiStats;
    report.settings = s;
    report.activeAPIRequester = activeAPIRequest ? activeAPIRequest->requester : "none";
    report.arousalProvider = ArousalProviderName();
    report.positionBackend = PositionBackendName();
    report.lastBendMethod = BendMethodName(position.lastMethod);
    report.fixes = std::move(fixes);
    report.arousal = arousalController.Value();
    report.arousalConnected = arousalController.Connected();
    report.positionBackendReady = PositionBackendAvailable();
    report.ppaLoaded = ppaLoaded;
    report.apiVersion = SPS::API::kVersion;
    report.externalResetNotices = externalResetNotices.load();
    report.ownerRestorations = ownerRestorations.load();
    report.nowMs = NowMs();
    const auto* vm = SPS::Runtime::VM();
    return SPS::Diagnostics::FormatSupportReport(report) + fmt::format(
        "\nPapyrus: Dispatch={} VMOverstressed={}\n", PapyrusStatusText(), vm && vm->overstressed);
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
    const int action = cbpc ? 1 : 0;
    const bool retryingCompletedHandoff = pendingQuickAction.load() == action &&
        !ownershipController.Read().pending && playerContext.physics.known.load() &&
        playerContext.physics.usingCBPC.load() == cbpc;
    if (!retryingCompletedHandoff && !SetOwner(cbpc, true)) {
        const bool newlyQueued = pendingQuickAction.exchange(action) != action;
        if (newlyQueued || pendingQuickActionUntilMs.load() == 0)
            pendingQuickActionUntilMs.store(NowMs() + 15000);
        activeManualPhysicsTest.store(-1);
        manualPhysicsTestUntilMs.store(0);
        if (newlyQueued)
            Record(fmt::format("{} physics test queued; waiting for script execution or the physics handoff",
                cbpc ? "Erect" : "Soft"));
        return;
    }
    if (ownershipController.Read().pending) {
        const bool newlyQueued = pendingQuickAction.exchange(action) != action;
        if (newlyQueued || pendingQuickActionUntilMs.load() == 0)
            pendingQuickActionUntilMs.store(NowMs() + 15000);
        activeManualPhysicsTest.store(-1);
        manualPhysicsTestUntilMs.store(0);
        if (newlyQueued)
            Record(fmt::format("{} physics test handoff queued; waiting for completion",
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
    if (!cbpc) {
        playerContext.recovery.softConfirmationDueMs.store(testStartedMs + 250);
        playerContext.recovery.softHandoffResetDueMs.store(testStartedMs + 250);
        playerContext.recovery.softHandoffResetUntilMs.store(testStartedMs + 10250);
    }
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
            Record(fmt::format("Repair queued; waiting for script execution ({})", PapyrusStatusText()));
        return;
    }
    const bool completedQueuedRepair = retryingQueuedRepair &&
        !ownershipController.Read().pending && playerContext.physics.known.load() &&
        NowMs() >= playerContext.physics.retryAfterMs.load();
    if (!completedQueuedRepair)
        Evaluate(true);
    if (ownershipController.Read().pending || !playerContext.physics.known.load() ||
        NowMs() < playerContext.physics.retryAfterMs.load()) {
        const bool newlyQueued = pendingQuickAction.exchange(2) != 2;
        if (newlyQueued || pendingQuickActionUntilMs.load() == 0)
            pendingQuickActionUntilMs.store(NowMs() + 15000);
        if (newlyQueued)
            Record("Repair is waiting for a usable player physics state");
        return;
    }
    pendingQuickAction.store(-1);
    pendingQuickActionUntilMs.store(0);
    const auto repairedAt = NowMs();
    playerContext.recovery.postSwitchVerificationDueMs.store(repairedAt + 1000);
    playerContext.recovery.postSwitchVerificationUntilMs.store(repairedAt + 11000);
    if (!playerContext.physics.usingCBPC.load()) {
        playerContext.recovery.softHandoffResetDueMs.store(repairedAt + 250);
        playerContext.recovery.softHandoffResetUntilMs.store(repairedAt + 10250);
    }
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
        const auto* reason = SPS::Runtime::PapyrusWaitReason(papyrusDispatchAllowedAfterMs.load(), now);
        Record(fmt::format("SPS-020: Requested quick fix timed out ({})",
            reason ? reason : "physics handoff incomplete"), true);
        return;
    }
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (action != 2 && (!copy.enabled || AnySceneHasPriority(copy) || ActiveAPIRequest())) {
        // A queued test must not re-request its owner on every tick after a
        // scene/API request has superseded it.
        pendingQuickAction.store(-1);
        pendingQuickActionUntilMs.store(0);
        activeManualPhysicsTest.store(-1);
        manualPhysicsTestUntilMs.store(0);
        Record("Queued physics test cancelled because another control source has priority");
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
    const auto ownership = ownershipController.Read();
    const auto position = positionController.Read();

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
    ImGuiMCP::TextWrapped("Setup checks find the required files and bones. Use the physics tests to check actual movement; a completed command cannot verify live motion.");

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
    if (ImGuiMCP::CollapsingHeader("Recovery")) {
        // Recovery used to be a whole page. Every option here has a working
        // default, so it belongs with the rest of the "something is wrong"
        // tools rather than in the everyday menu.
        Settings recovery;
        { std::scoped_lock lock(settingsLock); recovery = settings; }
        bool recoveryChanged = false;
        recoveryChanged |= ImGuiMCP::Checkbox("Refresh physics after loading", &recovery.resetSMPAfterLoad);
        recoveryChanged |= ImGuiMCP::Checkbox("Refresh after changing armour or schlong",
            &recovery.equipmentChangeRecovery);
        Hint("Both are recommended. Turning them off leaves the last state in place.");
        if (position.automaticSuspended) {
            ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F),
                "Angle recovery paused after repeated failures.");
            if (ImGuiMCP::Button("Try angle recovery again")) {
                ResetPositionRecovery();
                playerContext.position.appliedBend.store(-1);
            }
        }
        if (recoveryChanged) {
            const Settings previous = debugSettings;
            debugSettings = recovery;
            SaveSettingsAndApply(debugSettings, previous);
        }
    }

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
            "Waiting for the game to run the %s...", queuedName);
    } else if (activeManualPhysicsTest.load() >= 0 && NowMs() < manualPhysicsTestUntilMs.load()) {
        const auto remaining = std::max<std::int64_t>(0, manualPhysicsTestUntilMs.load() - NowMs());
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.35F, 1.0F, 0.45F, 1.0F),
            "%s physics test is active (%lld seconds left)",
            activeManualPhysicsTest.load() == 1 ? "Erect" : "Soft", (remaining + 999) / 1000);
    }
    ImGuiMCP::TextWrapped("Tests temporarily force one state, then normal control resumes. Repair reapplies the state SPS currently expects. Actions wait briefly while the game or a physics handoff is busy.");
    ImGuiMCP::Separator();

    SectionHeading("DETECTED COMPONENTS");
    const auto providerName = ArousalProviderName();
    if (debugSettings.mode != 0 && (!d.oslModuleLoaded || !d.oslPluginLoaded))
        StatusLine("Arousal mod", "Not installed - not needed in manual mode", 2);
    else
        StatusLine("Arousal mod", d.oslModuleLoaded && d.oslPluginLoaded ? (arousalController.Connected() ? fmt::format("{} - ready", providerName).c_str() : fmt::format("{} - still checking", providerName).c_str()) : "Missing", d.oslModuleLoaded && d.oslPluginLoaded ? (arousalController.Connected() ? 2 : 1) : 0);
    // "Commands accepted" is deliberate: neither engine can be read back. FSMP
    // reports only whether a bone was dynamic before a call, and CBPC's
    // StartPhysics/StopPhysics return nothing at all. Claiming "ready" here was
    // an overstatement in support reports.
    StatusLine("Soft physics", !d.fsmpModuleLoaded ? "Faster HDT-SMP is missing" :
        (!d.fsmpActorApiAvailable ? "FSMP is too old - update to 4.0.1+" :
            (!d.fsmpBridgePresent ? "SPS FSMP bridge is missing - reinstall SPS" :
                (ownership.smpConnected ? "SMP - commands accepted" : "SMP found - not tested yet"))),
        !d.fsmpModuleLoaded || !d.fsmpActorApiAvailable || !d.fsmpBridgePresent ? 0 : (ownership.smpConnected ? 2 : 1));
    StatusLine("Erect physics", d.cbpcModuleLoaded ? (ownership.cbpcConnected ? "CBPC - commands accepted" : "CBPC found - not tested yet") : "CBPC is missing", d.cbpcModuleLoaded ? (ownership.cbpcConnected ? 2 : 1) : 0);
    if (ownership.smpDriftDetections > 0)
        StatusLine("SMP drift", fmt::format("Recovered {} time(s); {} of 6 bones had resumed", ownership.smpDriftDetections, ownership.lastDynamicBonesBefore).c_str(), 1);
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
        StatusLine("SOFTBODY", d.softbodyLoaded ? "Ready - automatic scene reload recovery" : "Not installed", d.softbodyLoaded ? 2 : 1);
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
        StatusLine("Ordered handoff", ownership.pending ?
            (ownership.pendingCBPC ? "Queued for CBPC" : "Queued for SMP") : "No handoff pending",
            ownership.pending ? 1 : 2);
        ImGuiMCP::Text("Confirmed owner restorations: %u", ownerRestorations.load());
        ImGuiMCP::Text("Successful physics changes: %u", ownership.successes);
        ImGuiMCP::Text("Failed physics changes: %u", ownership.failures);
        ImGuiMCP::Text("Erect angle applications: %u", position.repairs);
        ImGuiMCP::Text("Requested / applied angle: %d / %d", position.requestedBend, position.appliedBend);
        ImGuiMCP::Text("Last angle method: %s", BendMethodName(position.lastMethod));
        StatusLine("Gradual erection", position.animating ? fmt::format("Moving: {}/{}", position.appliedBend, position.animationTargetBend).c_str() : "Not moving", position.animating ? 1 : 2);
        StatusLine("Angle repair", position.automaticSuspended ? "Paused after failures" : (NowMs() < position.guardUntilMs ? "Waiting for bouncing to stop" : "Ready"), position.automaticSuspended ? 0 : (NowMs() < position.guardUntilMs ? 1 : 2));
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
    SKSEMenuFramework::AddSectionItem("NPCs", RenderNPCs);
    SKSEMenuFramework::AddSectionItem("Troubleshooting", RenderDebug);
}

class ModEventSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        const std::string_view name = event->eventName.c_str();
        // Extended Hook* events go only to Papyrus registrations. The native
        // SKSE event carries the unprefixed name and thread ID in strArg.
        if (SPS::Core::IsSexLabThreadEvent(name)) {
            const bool playerThread = SPS::Core::MatchesPlayerSceneThread(
                event->strArg.c_str(), sceneController.State().sexLab.threadID.load());
            if (playerThread) {
                ++sceneController.State().sexLab.roleGeneration;
                sceneController.State().sexLab.roleQueryPending.store(false);
                sceneController.State().sexLab.roleRetryAfterMs.store(NowMs() + 350);
                if (SoftbodyLoaded() && SPS::Core::IsSexLabPhysicsReloadEvent(name))
                    ScheduleExternalOwnerRepair(3000, "the player's SOFTBODY scene reload");
            }
            // Unknown/new threads are checked through the player bridge; NPC
            // events never reset the player's role or schedule physics work.
            if (playerThread || sceneController.State().sexLab.threadID.load() < 0)
                if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { QuerySexLab(); });
            return RE::BSEventNotifyControl::kContinue;
        }
        const bool softbodyReloadEvent = name == "OStim_Start" || name == "OStim_SceneChanged" ||
            name == "ostim_start" || name == "ostim_scenechanged" ||
            ((name == "ostim_thread_start" || name == "ostim_thread_scenechanged") && event->numArg == 0.0F);
        if (softbodyReloadEvent && SoftbodyLoaded())
            ScheduleExternalOwnerRepair(3000, "the player's SOFTBODY scene reload");

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
            if (!playerContext.physics.known.load() || !playerContext.physics.usingCBPC.load()) {
                playerContext.recovery.softConfirmationDueMs.store(NowMs() + 250);
                ScheduleSoftPoseResetAfterScene(NowMs());
            }
            Record("OStim player scene ended");
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { Evaluate(); });
        } else if (name == "OSLA_ActorArousalUpdated" && event->sender == RE::PlayerCharacter::GetSingleton()) {
            // OSL already supplies the updated player value in numArg. Using it
            // directly avoids a query -> update event -> query feedback loop.
            const float reading = event->numArg;
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([reading] { arousalController.AcceptExternalReading(reading); });
        } else if (name == "sla_UpdateComplete") {
            // SLO Aroused NG reports a completed update globally. Querying its
            // OSL compatibility stub is cheap and avoids waiting for the next poll.
            if (auto* tasks = SKSE::GetTaskInterface())
                tasks->AddTask([] { QueryArousal(true); });
        } else if (name == "SexLabDisabled") {
            sceneController.State().sexLab.threadID.store(-1);
            ++sceneController.State().sexLab.queryGeneration;
            sceneController.State().sexLab.queryPending.store(false);
            sceneController.State().sexLab.endedMs.store(0);
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
            if (!playerContext.physics.known.load() || !playerContext.physics.usingCBPC.load()) {
                playerContext.recovery.softConfirmationDueMs.store(NowMs() + 250);
                ScheduleSoftPoseResetAfterScene(NowMs());
            }
        } else if (name == "SexLabEnabled" || name == "SexLabGameLoaded") {
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { QuerySexLab(); RefreshDiagnostics(); });
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

class NpcLoadedSink final : public RE::BSTEventSink<RE::TESObjectLoadedEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* event,
        RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override {
        if (event) {
            SPS::Runtime::InvalidateBasicNpc(event->formID);
            SPS::Runtime::InvalidateManagedNpc(event->formID);
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
        if (event && event->reference == RE::PlayerCharacter::GetSingleton() &&
            now >= playerContext.recovery.ignoreNodeEventsUntilMs.load()) {
            playerBonesRefreshDueMs.store(now + 1500);
            if (enabled)
                playerContext.recovery.nodeRefreshDueMs.store(now + 750);
        }
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
        playerBonesRefreshDueMs.store(NowMs() + 1500);
        if (enabled) {
            // Some armour managers replace the genital mesh without emitting
            // an SKSE NiNode update. Debounce paired equip/unequip events and
            // run the same owner recovery after the replacement nodes settle.
            // Unequipping rebuilds the body nodes later than equipping, so it
            // waits longer. The latest event always wins the debounce.
            playerContext.recovery.nodeRefreshDueMs.store(NowMs() + (event->equipped ? 2000 : 2500));
            equipSecondResetPending.store(true);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        SPS::Runtime::UpdateExecutionClock();
        if (!event || event->menuName != RE::SleepWaitMenu::MENU_NAME)
            return RE::BSEventNotifyControl::kContinue;

        auto* calendar = RE::Calendar::GetSingleton();
        if (!calendar) return RE::BSEventNotifyControl::kContinue;
        if (event->opening) {
            playerContext.spontaneous.sleepWaitStartedHours.store(calendar->GetHoursPassed());
            return RE::BSEventNotifyControl::kContinue;
        }

        const float started = playerContext.spontaneous.sleepWaitStartedHours.exchange(-1.0F);
        bool allowMorning = false;
        { std::scoped_lock lock(settingsLock); allowMorning = settings.morningErections; }
        const float restedHours = started >= 0.0F ? calendar->GetHoursPassed() - started : 0.0F;
        if (allowMorning && restedHours >= 3.0F) {
            playerContext.spontaneous.morningDueMs.store(NowMs() + 1500);
            Record(fmt::format("Morning erection queued after resting for {:.1f} hours", restedHours));
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

void ResetTransientTimers() {
    SPS::Runtime::ResetActorIdentities();
    SPS::Runtime::ResetBasicNpcs();
    SPS::Runtime::ResetManagedNpcs();
    SPS::Runtime::ResetApiActorReads();
    SPS::Runtime::ResetActorBridgeRequests();
    SPS::Runtime::ResetPhysicsOperations();
    ownershipController.ResetPending();
    recoveryController.ResetTransient();
    diagnosticsRefreshDueMs.store(0);
    playerBonesRefreshDueMs.store(0);
    equipSecondResetPending.store(false);
    manualPhysicsTestUntilMs.store(0);
    activeManualPhysicsTest.store(-1);
    pendingQuickAction.store(-1);
    pendingQuickActionUntilMs.store(0);
    playerArousalWriteRequested.store(false);
    playerArousalWriteInFlight.store(false);
}

ModEventSink modEventSink;
NiNodeSink niNodeSink;
NpcLoadedSink npcLoadedSink;
EquipEventSink equipEventSink;
MenuSink menuSink;

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
        // Any callback still queued belongs to the old game state. Give the VM
        // time to finish rebuilding before SPS sends another scripted request.
        SPS::Runtime::PauseExecutionClock();
        InvalidatePapyrusQueries(3000);
        sceneController.ResetSession();
        arousalController.ResetReading();
        sceneController.State().sexLab.valid.store(false);
        sceneController.State().sexLab.roleValid.store(false);
        sceneController.State().sexLab.lastTopMs.store(0);
        sceneController.State().sexLab.bottomCandidateSinceMs.store(0);
        sceneController.State().ostim.active.store(false);
        sceneController.State().ostim.role.store(0);
        sceneController.State().ostim.roleValid.store(false);
        sceneController.State().ostim.entryStateValid.store(false);
        playerContext.spontaneous.randomNextMs.store(0);
        playerContext.spontaneous.randomUntilMs.store(0);
        playerContext.spontaneous.randomManualTest.store(false);
        playerContext.spontaneous.refractoryUntilMs.store(0);
        playerContext.spontaneous.morningDueMs.store(0);
        playerContext.spontaneous.morningActive.store(false);
        playerContext.spontaneous.sleepWaitStartedHours.store(-1.0F);
        CancelErectionAnimation();
        ResetTransientTimers();
        return;
    }
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        // The load order and Data files are final from here on.
        SPS::Runtime::FreezeCompatibilityCache();
        Load();
        RegisterMenu();
        RegisterPPAAPI();
        if (auto* source = SKSE::GetModCallbackEventSource()) source->AddEventSink(&modEventSink);
        if (auto* source = SKSE::GetNiNodeUpdateEventSource()) source->AddEventSink(&niNodeSink);
        if (auto* source = RE::ScriptEventSourceHolder::GetSingleton()) source->AddEventSink(&equipEventSink);
        if (auto* source = RE::ScriptEventSourceHolder::GetSingleton()) source->AddEventSink(&npcLoadedSink);
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
        SPS::Runtime::UpdateExecutionClock();
        InvalidatePapyrusQueries(1000);
        sceneController.ResetSession();
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
        playerContext.spontaneous.randomNextMs.store(0);
        playerContext.spontaneous.randomUntilMs.store(0);
        playerContext.spontaneous.randomManualTest.store(false);
        playerContext.spontaneous.refractoryUntilMs.store(0);
        playerContext.spontaneous.morningDueMs.store(0);
        playerContext.spontaneous.morningActive.store(false);
        playerContext.spontaneous.sleepWaitStartedHours.store(-1.0F);
        CancelErectionAnimation();
        // The live genital skeleton can be replaced a few seconds after the
        // save has loaded. Replay an already-selected erect angle once after
        // that late replacement, without changing the user's physics mode.
        playerContext.recovery.erectMeshReplayDueMs.store(NowMs() + 5000);
        diagnosticsRefreshDueMs.store(NowMs() + 2000);
        ResetPPASceneTracking(2000);
        playerContext.physics.known.store(false);
        playerContext.recovery.startupReconcileDueMs.store(NowMs() + 1000);
        playerContext.recovery.startupReconcileUntilMs.store(NowMs() + 30000);
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

extern "C" __declspec(dllexport) const SPS::API::InterfaceV2* __cdecl
SchlongPhysicsSwapper_GetAPI_V2() {
    static const SPS::API::InterfaceV2 api{
        SPS::API::kVersionV2,
        sizeof(SPS::API::InterfaceV2),
        Mod::APIGetCapabilitiesV2,
        Mod::APIIsActorSupported,
        Mod::APIGetState,
        Mod::APIRequestPhysics,
        Mod::APIReleasePhysics,
        Mod::APINotifyPhysicsReset,
        Mod::APIRegisterStateListener,
        Mod::APIUnregisterStateListener,
        Mod::APIGetArousal,
        Mod::APIGetErectThreshold
    };
    return &api;
}

extern "C" __declspec(dllexport) const SPS::API::InterfaceV3* __cdecl
SchlongPhysicsSwapper_GetAPI_V3() {
    static const SPS::API::InterfaceV3 api{
        SPS::API::kVersionV3,
        sizeof(SPS::API::InterfaceV3),
        Mod::APIGetCapabilitiesV3,
        Mod::APIIsActorSupported,
        Mod::APIGetState,
        Mod::APIRequestPhysics,
        Mod::APIReleasePhysics,
        Mod::APINotifyPhysicsReset,
        Mod::APIRegisterStateListener,
        Mod::APIUnregisterStateListener,
        Mod::APIGetArousal,
        Mod::APIGetErectThreshold,
        Mod::APIGetActorArousal,
        Mod::APIIsActorInScene
    };
    return &api;
}

extern "C" __declspec(dllexport) const SPS::API::InterfaceV4* __cdecl
SchlongPhysicsSwapper_GetAPI_V4() {
    static const SPS::API::InterfaceV4 api{
        SPS::API::kVersionV4,
        sizeof(SPS::API::InterfaceV4),
        Mod::APIGetCapabilitiesV4,
        Mod::APIIsActorSupported,
        Mod::APIGetState,
        Mod::APIRequestPhysics,
        Mod::APIReleasePhysics,
        Mod::APINotifyPhysicsReset,
        Mod::APIRegisterStateListener,
        Mod::APIUnregisterStateListener,
        Mod::APIGetArousal,
        Mod::APIGetErectThreshold,
        Mod::APIGetActorArousal,
        Mod::APIIsActorInScene,
        Mod::APIGetActorSceneRole,
        Mod::APIRequestRiseHold,
        Mod::APIReleaseRiseHold,
        Mod::APIRegisterActorEventListener,
        Mod::APIUnregisterActorEventListener
    };
    return &api;
}

extern "C" __declspec(dllexport) const SPS::API::InterfaceV5* __cdecl
SchlongPhysicsSwapper_GetAPI_V5() {
    static const SPS::API::InterfaceV5 api{
        SPS::API::kVersionV5,
        sizeof(SPS::API::InterfaceV5),
        Mod::APIGetCapabilitiesV5,
        Mod::APIIsActorSupported,
        Mod::APIGetState,
        Mod::APIRequestPhysics,
        Mod::APIReleasePhysics,
        Mod::APINotifyPhysicsReset,
        Mod::APIRegisterStateListener,
        Mod::APIUnregisterStateListener,
        Mod::APIGetArousal,
        Mod::APIGetErectThreshold,
        Mod::APIGetActorArousal,
        Mod::APIIsActorInScene,
        Mod::APIGetActorSceneRole,
        Mod::APIRequestRiseHold,
        Mod::APIReleaseRiseHold,
        Mod::APIRegisterActorEventListener,
        Mod::APIUnregisterActorEventListener,
        Mod::APIGetActorState,
        Mod::APIGetAnyActorSceneRole,
        Mod::APIRegisterNpcEventListener,
        Mod::APIUnregisterNpcEventListener
    };
    return &api;
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    Mod::runtimeVersion = skse->RuntimeVersion().string(".");
    Mod::skseVersion = REL::Version::unpack(skse->SKSEVersion()).string(".");
    // CommonLib's own log setup and spdlog's file sink both take the log path as
    // a narrow string (path::string()). That throws for a Documents path the
    // system code page cannot represent, which would crash Skyrim during plugin
    // load. SPS replaces the default logger anyway, so CommonLib's is disabled
    // and SPS opens the file itself through a wide path.
    SKSE::Init(skse, SKSE::InitInfo{ .log = false });
    static std::ofstream logStream;
    if (auto dir = SKSE::log::log_directory()) {
        std::error_code ec;
        std::filesystem::create_directories(*dir, ec);
        logStream.open(*dir / L"SchlongPhysicsSwapper.log",
            std::ios::out | std::ios::trunc | std::ios::binary);
    }
    std::shared_ptr<spdlog::sinks::sink> sink = logStream.is_open()
        ? std::shared_ptr<spdlog::sinks::sink>(
            std::make_shared<spdlog::sinks::ostream_sink_mt>(logStream, true))
        : std::shared_ptr<spdlog::sinks::sink>(std::make_shared<spdlog::sinks::null_sink_mt>());
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global", std::move(sink)));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    Mod::ConfigureControllers();
    // Per-save list of actors the user made futa in SexLab (co-save "SPSW").
    SPS::Runtime::RegisterActorIdentitySerialization();
    if (!SKSE::GetPapyrusInterface()->Register([](RE::BSScript::IVirtualMachine* vm) {
            return SPS::Runtime::RegisterPhysicsBridge(vm) && SPS::Runtime::RegisterActorBridgeGuards(vm) &&
                SPS::Runtime::SosPose::RegisterPoseAngleNatives(vm) &&
                SPS::Runtime::RegisterActorIdentityNatives(vm);
        })) {
        logger::critical("SPS physics bridge registration failed");
        return false;
    }
    SKSE::GetMessagingInterface()->RegisterListener(Mod::OnMessage);
    logger::info("Schlong Physics Swapper {} loaded", Mod::kVersion);
    return true;
}
