#include <RE/Skyrim.h>
#include <REL/Version.h>
#include <SKSE/SKSE.h>
#include <SimpleIni.h>
#include <SKSEMenuFramework.h>
#include <fmt/format.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <Windows.h>
#include "PPAInterface.h"
#include "SPSAPI.h"
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
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace logger = SKSE::log;

namespace Mod {
namespace fs = std::filesystem;

constexpr auto kName = "Schlong Physics Swapper";
constexpr auto kVersion = "1.8.1";
constexpr auto kIni = "Data/SKSE/Plugins/SchlongPhysicsSwapper.ini";
constexpr auto kLegacyIni = "Data/SKSE/Plugins/UBEPhysicsSwitch.ini";
constexpr auto kReport = "Data/SKSE/Plugins/SchlongPhysicsSwapper_Diagnostics.txt";
constexpr auto kCaptureReport = "Data/SKSE/Plugins/SchlongPhysicsSwapper_DebugCapture.txt";
constexpr std::array<const char*, 6> kBones{
    "NPC Genitals01 [Gen01]", "NPC Genitals02 [Gen02]", "NPC Genitals03 [Gen03]",
    "NPC Genitals04 [Gen04]", "NPC Genitals05 [Gen05]", "NPC Genitals06 [Gen06]"
};

struct Settings {
    bool enabled{ true };
    float threshold{ 60.0F };
    float hysteresis{ 5.0F };
    int mode{ 0 };  // 0 automatic, 1 force SMP, 2 force CBPC
    int erectBend{ 14 };
    int pollMs{ 1000 };
    bool sexLabOverride{ true };
    bool sexLabRoleSwitching{ true };
    int sexLabBottomBehavior{ 0 };  // 0 keep entry, 1 live arousal, 2 SMP, 3 CBPC
    int sexLabUnknownRole{ 0 };  // 0 keep current, 1 SMP, 2 CBPC
    int sceneEndDelayMs{ 1500 };
    int switchCooldownMs{ 750 };
    bool resetSMPAfterLoad{ true };
    int loadResetDelayMs{ 10000 };
    bool positionControl{ true };
    int bendMethod{ 2 };  // 0 native, 1 animation event, 2 compatibility
    bool animatePosition{ true };
    bool gradualErection{ true };
    int erectionDurationMs{ 3000 };
    bool bounceGuard{ true };
    bool useSexLabBend{ false };
    int sexLabBend{ 14 };
    int settleDelayMs{ 350 };
    int maxBendFailures{ 3 };
    bool verboseLogging{ false };
};

struct Diagnostics {
    bool menuFrameworkLoaded{ false };
    bool oslModuleLoaded{ false };
    bool fsmpModuleLoaded{ false };
    bool cbpcModuleLoaded{ false };
    bool sexLabModuleLoaded{ false };
    bool sexLabPluginLoaded{ false };
    bool sexLabRoleBridgePresent{ false };
    bool oslPluginLoaded{ false };
    bool classicArousedPluginLoaded{ false };
    bool supportedAddonLoaded{ false };
    bool sosPluginLoaded{ false };
    bool tngPluginLoaded{ false };
    bool sosScriptPresent{ false };
    bool physicsEditorLoaded{ false };
    bool autoPhysicsResetLoaded{ false };
    bool crashLoggerLoaded{ false };
    int playerBonesFound{ 0 };
    int xmlFiles{ 0 };
    int compatibleXmlFiles{ 0 };
    int cbpcMapFiles{ 0 };
    int compatibleCbpcMaps{ 0 };
    int cbpcParameterFiles{ 0 };
    int compatibleCbpcParameters{ 0 };
    std::string xmlSummary{ "None found" };
    std::string cbpcMapSummary{ "None found" };
    std::string cbpcParameterSummary{ "None found" };
    std::int64_t checkedAtMs{ 0 };
};

struct ExternalPhysicsRequest {
    SPS::API::RequestHandle handle{ 0 };
    SPS::API::PhysicsState state{ SPS::API::PhysicsState::Unknown };
    std::int64_t expiresAtMs{ 0 };
    std::string requester{ "Unnamed plugin" };
};

struct APIStateListener {
    SPS::API::StateChangedCallback callback{ nullptr };
    void* userData{ nullptr };
};

Settings settings;
Diagnostics diagnostics;
std::mutex settingsLock;
std::mutex diagnosticsLock;
std::mutex activityLock;
std::deque<std::string> activity;
std::string lastAction{ "Waiting for a loaded game" };
std::string lastError;
std::string runtimeVersion{ "unknown" };
std::string skseVersion{ "unknown" };
std::mutex debugCaptureLock;
std::vector<std::string> debugCaptureLines;
std::mutex apiLock;
std::unordered_map<SPS::API::RequestHandle, ExternalPhysicsRequest> apiRequests;
std::unordered_map<SPS::API::ListenerHandle, APIStateListener> apiListeners;

std::atomic<float> arousal{ 0.0F };
std::atomic<bool> arousalValid{ false };
std::atomic<bool> queryPending{ false };
std::atomic<std::int64_t> queryStartedMs{ 0 };
std::atomic<std::int64_t> arousalRetryAfterMs{ 0 };
std::atomic<std::uint64_t> arousalQueryGeneration{ 0 };
std::atomic<bool> sexLabActive{ false };
std::atomic<bool> sexLabValid{ false };
std::atomic<bool> sexLabQueryPending{ false };
std::atomic<std::int64_t> sexLabQueryStartedMs{ 0 };
std::atomic<std::int64_t> sexLabQueryRetryAfterMs{ 0 };
std::atomic<std::uint64_t> sexLabQueryGeneration{ 0 };
std::atomic<std::int64_t> sexLabEndedMs{ 0 };
std::atomic<bool> sexLabEntryCBPC{ false };
std::atomic<bool> sexLabEntryStateValid{ false };
std::atomic<int> sexLabRole{ 0 };  // 0 unknown, 1 bottom/receiving, 2 top/penetrating
std::atomic<bool> sexLabRoleValid{ false };
std::atomic<bool> sexLabRoleQueryPending{ false };
std::atomic<std::int64_t> sexLabRoleQueryStartedMs{ 0 };
std::atomic<std::int64_t> sexLabRoleRetryAfterMs{ 0 };
std::atomic<std::int64_t> sexLabLastTopMs{ 0 };
std::atomic<std::int64_t> sexLabBottomCandidateSinceMs{ 0 };
std::atomic<std::uint64_t> sexLabRoleGeneration{ 0 };
std::atomic<bool> ppaApiConnected{ false };
std::atomic<bool> ppaSceneActive{ false };
std::atomic<int> ppaSceneRole{ 0 };
std::atomic<bool> ppaSceneRoleValid{ false };
std::atomic<std::int64_t> ppaLastUpdateMs{ 0 };
std::atomic<std::int64_t> ppaLastTopMs{ 0 };
std::atomic<std::int64_t> ppaBottomCandidateSinceMs{ 0 };
std::atomic<std::int64_t> ppaIgnoreUntilMs{ 0 };
std::atomic<bool> usingCBPC{ false };
std::atomic<bool> stateKnown{ false };
std::atomic<bool> polling{ false };
std::atomic<bool> oslConnected{ false };
std::atomic<bool> sexLabConnected{ false };
std::atomic<bool> smpConnected{ false };
std::atomic<bool> cbpcConnected{ false };
std::atomic<bool> sosConnected{ false };
std::atomic<std::int64_t> lastSwitchMs{ 0 };
std::atomic<std::int64_t> retryAfterMs{ 0 };
std::atomic<std::int64_t> loadSMPResetDueMs{ 0 };
std::atomic<std::int64_t> loadSMPResetRestoreDueMs{ 0 };
std::atomic<std::int64_t> lastLoadSMPResetMs{ 0 };
std::atomic<std::int64_t> lastBendApplyMs{ 0 };
std::atomic<std::int64_t> bendSettleDueMs{ 0 };
std::atomic<std::int64_t> bendConfirmationDueMs{ 0 };
std::atomic<std::int64_t> softConfirmationDueMs{ 0 };
std::atomic<std::int64_t> cbpcConfirmationDueMs{ 0 };
std::atomic<std::int64_t> erectionAnimationStartMs{ 0 };
std::atomic<std::int64_t> bendGuardUntilMs{ 0 };
std::atomic<std::int64_t> bendGuardWindowStartMs{ 0 };
std::atomic<std::int64_t> nodeRefreshDueMs{ 0 };
std::atomic<std::int64_t> externalOwnerRepairDueMs{ 0 };
std::atomic<std::int64_t> lastExternalOwnerRepairMs{ 0 };
std::atomic<std::int64_t> ignoreNodeEventsUntilMs{ 0 };
std::atomic<std::int64_t> papyrusDispatchAllowedAfterMs{ 0 };
std::atomic<int> appliedBend{ -1 };
std::atomic<int> requestedBend{ -1 };
std::atomic<int> erectionAnimationTargetBend{ 0 };
std::atomic<int> erectionAnimationLastQueuedBend{ -1 };
std::atomic<int> lastBendMethod{ -1 };
std::atomic<int> bendGuardCount{ 0 };
std::atomic<int> bendConsecutiveFailures{ 0 };
std::atomic<bool> lastBendSucceeded{ false };
std::atomic<bool> positionAutoSuspended{ false };
std::atomic<bool> erectionAnimating{ false };
std::atomic<std::uint64_t> erectionAnimationGeneration{ 0 };
std::atomic<unsigned> switchSuccesses{ 0 };
std::atomic<unsigned> switchFailures{ 0 };
std::atomic<unsigned> bendRepairs{ 0 };
std::atomic<unsigned> loadSMPResets{ 0 };
std::atomic<std::int64_t> debugCaptureStartedMs{ 0 };
std::atomic<std::int64_t> debugCaptureUntilMs{ 0 };
std::atomic<std::uint64_t> debugCaptureGeneration{ 0 };
std::atomic<SPS::API::RequestHandle> nextAPIRequestHandle{ 1 };
std::atomic<SPS::API::ListenerHandle> nextAPIListenerHandle{ 1 };
std::atomic<unsigned> apiRequestsAccepted{ 0 };
std::atomic<unsigned> apiRequestsReleased{ 0 };
std::atomic<unsigned> externalResetNotices{ 0 };
std::atomic<unsigned> externalOwnerRepairs{ 0 };
std::jthread pollThread;
std::jthread erectionAnimationThread;
std::jthread debugCaptureThread;
const SPS::PPA::InterfaceV1* ppaAPI{ nullptr };
SPS::PPA::ListenerHandle ppaListener{ 0 };

void Evaluate(bool force = false);
void QuerySexLab();
void QuerySexLabRole();
void RefreshDiagnostics();
void Save();
bool SexLabHasPriority(const Settings& copy);
void ApplyRequestedBend(bool force, bool animate, bool automatic);
void CancelErectionAnimation();
std::string BuildReport();
bool PluginLoaded(std::initializer_list<std::string_view> names);
std::int64_t NowMs();
void Record(std::string message, bool error = false);
std::optional<ExternalPhysicsRequest> ActiveAPIRequest();
void ClearAPIRequests();
void ScheduleExternalOwnerRepair(int delayMs, std::string_view reason);

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
    if (sexLabActive.load() && sexLabRoleValid.load()) {
        if (sexLabRole.load() == 1) return usingCBPC.load();
        if (sexLabRole.load() == 2) return true;
        return usingCBPC.load();
    }
    // The listener gives exact scene state on current PPA builds. Retain the
    // conservative SexLab fallback for older PPA versions without the export.
    const bool recentPPAUpdate = ppaSceneActive.load() && NowMs() - ppaLastUpdateMs.load() < 3000;
    return recentPPAUpdate || (sexLabValid.load() && sexLabActive.load());
}

std::int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void ScheduleExternalOwnerRepair(int delayMs, std::string_view reason) {
    if (!stateKnown.load()) return;
    const auto due = NowMs() + std::clamp(delayMs, 100, 5000);
    externalOwnerRepairDueMs.store(due);
    ++externalResetNotices;
    logger::info("Physics owner re-check scheduled after {}", reason);
}

std::optional<ExternalPhysicsRequest> ActiveAPIRequest() {
    const auto now = NowMs();
    std::scoped_lock lock(apiLock);
    for (auto it = apiRequests.begin(); it != apiRequests.end();) {
        if (it->second.expiresAtMs > 0 && now >= it->second.expiresAtMs) {
            ++apiRequestsReleased;
            it = apiRequests.erase(it);
        } else {
            ++it;
        }
    }
    if (apiRequests.empty()) return std::nullopt;
    const auto selected = std::ranges::max_element(apiRequests, {}, [](const auto& entry) {
        return entry.first;
    });
    return selected->second;
}

void ClearAPIRequests() {
    std::size_t cleared = 0;
    {
        std::scoped_lock lock(apiLock);
        cleared = apiRequests.size();
        apiRequests.clear();
    }
    if (cleared > 0) {
        apiRequestsReleased.fetch_add(static_cast<unsigned>(cleared));
        Record(fmt::format("Compatibility API released {} request(s) for the new game session", cleared));
    }
}

SPS::API::ControlSource CurrentAPIControlSource() {
    if (ActiveAPIRequest()) return SPS::API::ControlSource::ExternalAPI;
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (SexLabHasPriority(copy)) return SPS::API::ControlSource::Scene;
    if (stateKnown.load()) return SPS::API::ControlSource::SPS;
    return SPS::API::ControlSource::None;
}

void QueueAPIEvaluation(bool force) {
    if (auto* tasks = SKSE::GetTaskInterface()) {
        tasks->AddTask([force] {
            if (force) {
                const auto request = ActiveAPIRequest();
                const bool alreadyApplied = request && stateKnown.load() &&
                    usingCBPC.load() == (request->state == SPS::API::PhysicsState::CBPC);
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
    std::vector<APIStateListener> listeners;
    {
        std::scoped_lock lock(apiLock);
        listeners.reserve(apiListeners.size());
        for (const auto& [_, listener] : apiListeners) listeners.push_back(listener);
    }
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
    snapshot->state = stateKnown.load() ?
        (usingCBPC.load() ? SPS::API::PhysicsState::CBPC : SPS::API::PhysicsState::SMP) :
        SPS::API::PhysicsState::Unknown;
    snapshot->source = request ? SPS::API::ControlSource::ExternalAPI : CurrentAPIControlSource();
    snapshot->arousal = arousalValid.load() ? arousal.load() : 0.0F;
    snapshot->activeRequest = request ? request->handle : 0;
    {
        std::scoped_lock lock(apiLock);
        snapshot->activeRequestCount = static_cast<std::uint32_t>(apiRequests.size());
    }
    return stateKnown.load() ? SPS::API::Result::Success : SPS::API::Result::NotReady;
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

    ExternalPhysicsRequest stored;
    stored.handle = nextAPIRequestHandle.fetch_add(1);
    if (stored.handle == 0) stored.handle = nextAPIRequestHandle.fetch_add(1);
    stored.state = request->state;
    stored.expiresAtMs = request->durationMilliseconds > 0 ?
        NowMs() + request->durationMilliseconds : 0;
    if (request->requesterName && request->requesterName[0] != '\0') {
        stored.requester = request->requesterName;
        if (stored.requester.size() > 64) stored.requester.resize(64);
    }
    {
        std::scoped_lock lock(apiLock);
        apiRequests.emplace(stored.handle, stored);
    }
    ++apiRequestsAccepted;
    Record(fmt::format("Compatibility API: {} requested {} for the player{}",
        stored.requester,
        stored.state == SPS::API::PhysicsState::CBPC ? "CBPC" : "SMP",
        request->durationMilliseconds > 0 ? fmt::format(" for {} ms", request->durationMilliseconds) : " until released"));
    QueueAPIEvaluation(true);
    return stored.handle;
}

SPS::API::Result __cdecl APIReleasePhysics(SPS::API::RequestHandle request) {
    if (request == 0) return SPS::API::Result::InvalidArgument;
    std::string requester;
    {
        std::scoped_lock lock(apiLock);
        const auto it = apiRequests.find(request);
        if (it == apiRequests.end()) return SPS::API::Result::NotFound;
        requester = it->second.requester;
        apiRequests.erase(it);
    }
    ++apiRequestsReleased;
    Record(fmt::format("Compatibility API: {} released player physics control", requester));
    QueueAPIEvaluation(false);
    return SPS::API::Result::Success;
}

SPS::API::Result __cdecl APINotifyPhysicsReset(std::uint32_t actorFormID) {
    if (!APIIsActorSupported(actorFormID)) return SPS::API::Result::UnsupportedActor;
    {
        std::scoped_lock lock(settingsLock);
        if (!settings.enabled) return SPS::API::Result::NotReady;
    }
    if (!stateKnown.load()) return SPS::API::Result::NotReady;
    ScheduleExternalOwnerRepair(750, "a compatibility API reset notification");
    return SPS::API::Result::Success;
}

SPS::API::ListenerHandle __cdecl APIRegisterStateListener(
    SPS::API::StateChangedCallback callback, void* userData) {
    if (!callback) return 0;
    auto handle = nextAPIListenerHandle.fetch_add(1);
    if (handle == 0) handle = nextAPIListenerHandle.fetch_add(1);
    std::scoped_lock lock(apiLock);
    apiListeners.emplace(handle, APIStateListener{ callback, userData });
    return handle;
}

SPS::API::Result __cdecl APIUnregisterStateListener(SPS::API::ListenerHandle listener) {
    if (listener == 0) return SPS::API::Result::InvalidArgument;
    std::scoped_lock lock(apiLock);
    return apiListeners.erase(listener) > 0 ?
        SPS::API::Result::Success : SPS::API::Result::NotFound;
}

void ResetPPASceneTracking(int ignoreMs = 0) {
    ppaSceneActive.store(false);
    ppaSceneRole.store(0);
    ppaSceneRoleValid.store(false);
    ppaLastUpdateMs.store(0);
    ppaLastTopMs.store(0);
    ppaBottomCandidateSinceMs.store(0);
    ppaIgnoreUntilMs.store(ignoreMs > 0 ? NowMs() + ignoreMs : 0);
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
    if (!event->ending && now < ppaIgnoreUntilMs.load()) return;

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
        if (ppaSceneRoleValid.load() && ppaSceneRole.load() != observedRole) return;
        ppaBottomCandidateSinceMs.store(0);
        ppaLastUpdateMs.store(now);
        changed = ppaSceneActive.exchange(false) || ppaSceneRoleValid.load();
        ppaSceneRole.store(0);
        ppaSceneRoleValid.store(false);
    } else {
        if (observedRole == 2) {
            // One PPA update pass can describe the player as both a receiver
            // and a penetrating partner. Penetrating wins, and suppresses the
            // companion receiver record so SPS cannot alternate every frame.
            ppaLastTopMs.store(now);
            ppaBottomCandidateSinceMs.store(0);
        } else {
            if (now - ppaLastTopMs.load() < 500) return;
            const auto candidateSince = ppaBottomCandidateSinceMs.load();
            if (candidateSince == 0) {
                ppaBottomCandidateSinceMs.store(now);
                return;
            }
            // Require a short run of receiver-only updates before switching to
            // SMP. This filters paired PPA records without delaying real scenes.
            if (now - candidateSince < 100) return;
        }

        ppaLastUpdateMs.store(now);
        const int previous = ppaSceneRole.exchange(observedRole);
        const bool wasValid = ppaSceneRoleValid.exchange(true);
        ppaSceneActive.store(true);
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
        ppaApiConnected.store(false);
        return;
    }
    ppaAPI = api;
    ppaListener = ppaAPI->RegisterAnimationUpdateListener(OnPPAUpdate, nullptr);
    ppaApiConnected.store(ppaListener != 0);
    if (ppaListener) Record("PPA V1 listener connected; live scene hand-off enabled");
}

std::string LoadedDllVersion(const wchar_t* name) {
    const auto module = ::GetModuleHandleW(name);
    if (!module) return "not loaded";
    std::array<wchar_t, 32768> path{};
    if (::GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size())) == 0)
        return "loaded (version unavailable)";
    if (const auto version = REL::GetFileVersion(path.data()))
        return version->string(".");
    return "loaded (version unavailable)";
}

bool SloArousedLoaded() {
    return ::GetModuleHandleW(L"SexlabArousedNG.dll") != nullptr;
}

bool OslArousedLoaded() {
    return ::GetModuleHandleW(L"OSLAroused.dll") != nullptr;
}

bool SosAeNativeLoaded() {
    return ::GetModuleHandleW(L"SOSAE.dll") != nullptr;
}

bool ClassicArousedLoaded() {
    return !SloArousedLoaded() && !OslArousedLoaded() &&
        PluginLoaded({ "SexLabAroused.esm" });
}

std::string ArousalProviderName() {
    if (SloArousedLoaded()) return "SLO Aroused NG";
    if (OslArousedLoaded()) return "OSL Aroused";
    if (ClassicArousedLoaded()) return "SexLab Aroused Redux";
    return "none";
}

bool TngLoaded() {
    // Compatibility mods sometimes ship a tiny TheNewGentleman.esp master stub.
    // The DLL is the actual TNG runtime and is the component that supplies the
    // SOS-style position events we rely on.
    return ::GetModuleHandleW(L"TheNewGentleman.dll") != nullptr;
}

bool LegacySosLoaded() {
    // Schlongs of Skyrim.esp is also commonly supplied as a dependency stub.
    // Require the framework's Papyrus API before advertising its event backend.
    return PluginLoaded({ "Schlongs of Skyrim.esp" }) &&
        (fs::exists("Data/Scripts/SOS_API.pex") || fs::exists("Data/Scripts/SOS_SKSE.pex"));
}

std::string PositionBackendName() {
    const bool sosAe = SosAeNativeLoaded();
    if (TngLoaded() && sosAe) return "SOS AE native + TNG events";
    if (TngLoaded()) return "TNG animation events";
    if (sosAe) return "SOS AE native / events";
    if (LegacySosLoaded()) return "Legacy SOS animation events";
    return "none";
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
    {
        std::scoped_lock lock(activityLock);
        lastAction = message;
        if (error) lastError = message;
        activity.push_front(std::move(message));
        while (activity.size() > 12) activity.pop_back();
    }
}

void CaptureState(std::string_view reason) {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const auto line = fmt::format(
        "{} | engine={} arousal={:.1f}/{} provider={} connected={} SexLab={} role={} SMP={} CBPC={} bend={}/{} method={} animating={} guard={} suspended={}",
        reason, stateKnown.load() ? (usingCBPC.load() ? "CBPC" : "SMP") : "unknown",
        arousal.load(), arousalValid.load(), ArousalProviderName(), oslConnected.load(), sexLabActive.load(),
        SexLabRoleName(sexLabRole.load()),
        smpConnected.load(), cbpcConnected.load(), requestedBend.load(), appliedBend.load(),
        lastBendMethod.load(), erectionAnimating.load(), NowMs() < bendGuardUntilMs.load(),
        positionAutoSuspended.load());
    if (copy.verboseLogging || DebugCaptureActive()) logger::debug("{}", line);
    AppendCaptureLine("STATE", line);
}

std::vector<std::pair<std::string, std::string>> SuggestedFixes(const Diagnostics& d) {
    std::vector<std::pair<std::string, std::string>> fixes;
    if (!d.menuFrameworkLoaded)
        fixes.emplace_back("SPS-001", "Install or update SKSE Menu Framework 3, then fully restart Skyrim.");
    if (!d.oslModuleLoaded || !d.oslPluginLoaded)
        fixes.emplace_back("SPS-002", "Install OSL Aroused, SLO Aroused NG, or classic SexLab Aroused and enable its plugin.");
    if (!d.fsmpModuleLoaded)
        fixes.emplace_back("SPS-003", "Install Faster HDT-SMP and its requirements.");
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
    if (!d.sosScriptPresent && !d.sosPluginLoaded && !d.tngPluginLoaded)
        fixes.emplace_back("SPS-009", "No supported position backend was found. Install SOS AE-NG, legacy SOS, or The New Gentleman.");
    if (d.sexLabModuleLoaded && d.sexLabPluginLoaded && !d.sexLabRoleBridgePresent)
        fixes.emplace_back("SPS-013", "The SPS SexLab role bridge is missing. Reinstall version 1.7 so bottom/top scene switching can work.");
    if (d.physicsEditorLoaded)
        fixes.emplace_back("SPS-014", "Physics Editor is loaded and can control the same SMP/CBPC systems as SPS. Disable Physics Editor before using SPS.");
    if (switchFailures.load() > 0)
        fixes.emplace_back("SPS-010", "A physics handoff failed. Check SchlongPhysicsSwapper.log and confirm both FSMP and CBPC load correctly.");
    if (positionAutoSuspended.load() || (!lastBendSucceeded.load() && requestedBend.load() >= 0))
        fixes.emplace_back("SPS-011", "The erect angle could not be applied. Use Repair current state, then check for another mod controlling the angle.");
    if (fixes.empty())
        fixes.emplace_back("SPS-000", "Everything appears ready.");
    return fixes;
}

class ArousalCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    explicit ArousalCallback(std::uint64_t generation) : generation_(generation) {}

    void operator()(RE::BSScript::Variable a_result) override {
        if (generation_ != arousalQueryGeneration.load()) return;
        bool valid = false;
        float value = 0.0F;
        if (a_result.IsFloat()) {
            value = std::clamp(a_result.GetFloat(), 0.0F, 100.0F);
            valid = true;
        } else if (a_result.IsInt()) {
            value = static_cast<float>(std::clamp(a_result.GetSInt(), 0, 100));
            valid = true;
        }
        if (valid) {
            const auto previous = arousal.exchange(value);
            const auto wasValid = arousalValid.exchange(true);
            oslConnected.store(true);
            if (!wasValid || std::abs(previous - value) >= 0.5F)
                logger::info("{} arousal: {:.1f}", ArousalProviderName(), value);
        } else {
            arousalValid.store(false);
            arousalRetryAfterMs.store(NowMs() + 5000);
            Record("SPS-002: Arousal provider returned an invalid value", true);
        }
        queryPending.store(false);
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { Evaluate(); });
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    std::uint64_t generation_;
};

class SexLabCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    explicit SexLabCallback(std::uint64_t generation) : generation_(generation) {}

    void operator()(RE::BSScript::Variable a_result) override {
        if (generation_ != sexLabQueryGeneration.load()) return;
        if (a_result.IsBool()) {
            const bool active = a_result.GetBool();
            const bool previous = sexLabActive.exchange(active);
            sexLabValid.store(true);
            sexLabConnected.store(true);
            if (previous && !active) {
                const auto now = NowMs();
                sexLabEndedMs.store(now);
                sexLabEntryStateValid.store(false);
                sexLabRoleGeneration.fetch_add(1);
                sexLabRole.store(0);
                sexLabRoleValid.store(false);
                sexLabRoleQueryPending.store(false);
                sexLabLastTopMs.store(0);
                sexLabBottomCandidateSinceMs.store(0);
                // SexLab is the authoritative scene lifetime. PPA may still
                // publish the tail of its last frame, so release its position
                // ownership and ignore that stale tail before restoring soft.
                ResetPPASceneTracking(2000);
                if (stateKnown.load() && !usingCBPC.load())
                    softConfirmationDueMs.store(now + 250);
                Record("SexLab scene ended; post-scene hold started");
            } else if (!previous && active) {
                sexLabEntryCBPC.store(usingCBPC.load());
                sexLabEntryStateValid.store(stateKnown.load());
                sexLabLastTopMs.store(0);
                sexLabBottomCandidateSinceMs.store(0);
                Record(fmt::format("SexLab scene detected; receiving state locked as {}",
                    usingCBPC.load() ? "hard (CBPC)" : "flaccid (SMP)"));
            }
            if (previous != active && stateKnown.load() && usingCBPC.load()) {
                CancelErectionAnimation();
                Settings copy;
                { std::scoped_lock lock(settingsLock); copy = settings; }
                if ((copy.useSexLabBend || PPAOwnsPosition() ||
                    ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr) && copy.positionControl) {
                    appliedBend.store(-1);
                    bendSettleDueMs.store(NowMs() + copy.settleDelayMs);
                }
            }
        } else {
            sexLabValid.store(false);
            sexLabQueryRetryAfterMs.store(NowMs() + 5000);
        }
        sexLabQueryPending.store(false);
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] {
            if (sexLabActive.load()) QuerySexLabRole();
            Evaluate();
        });
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    std::uint64_t generation_;
};

class SexLabRoleCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    explicit SexLabRoleCallback(std::uint64_t generation) : generation_(generation) {}

    void operator()(RE::BSScript::Variable a_result) override {
        if (generation_ != sexLabRoleGeneration.load()) return;
        bool valid = false;
        int role = 0;
        if (a_result.IsInt()) {
            role = std::clamp(a_result.GetSInt(), 0, 2);
            valid = true;
        }
        const auto now = NowMs();
        if (valid && role == 2) {
            sexLabLastTopMs.store(now);
            sexLabBottomCandidateSinceMs.store(0);
        } else if (valid && role == 1 && sexLabRoleValid.load() && sexLabRole.load() == 2) {
            // P+ can briefly describe the player as receiving while a stage is
            // rebuilding, then report penetrating again on the next update.
            // Require a stable bottom result before giving SMP control back.
            auto candidateSince = sexLabBottomCandidateSinceMs.load();
            if (candidateSince == 0) {
                sexLabBottomCandidateSinceMs.store(now);
                sexLabRoleQueryPending.store(false);
                return;
            }
            if (now - candidateSince < 1250 || now - sexLabLastTopMs.load() < 1250) {
                sexLabRoleQueryPending.store(false);
                return;
            }
        } else if (!valid || role != 1) {
            sexLabBottomCandidateSinceMs.store(0);
        }
        const int previous = sexLabRole.exchange(role);
        const bool wasValid = sexLabRoleValid.exchange(valid);
        sexLabRoleQueryPending.store(false);
        const bool roleChanged = valid && (!wasValid || previous != role);
        if (roleChanged)
            Record(fmt::format("SexLab role: {}", SexLabRoleName(role)));
        // Do not send a position command merely because the player became the
        // bottom. Evaluate decides from arousal/settings, and a real CBPC/SMP
        // handoff schedules the one required position confirmation itself.
        if (!valid)
            logger::warn("SexLab role bridge returned an invalid value");
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { Evaluate(); });
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    std::uint64_t generation_;
};

auto VM() { return RE::BSScript::Internal::VirtualMachine::GetSingleton(); }

bool PapyrusReadyForDispatch() {
    if (NowMs() < papyrusDispatchAllowedAfterMs.load()) return false;
    const auto* main = RE::Main::GetSingleton();
    if (!main || !main->GetRuntimeData().gameActive) return false;
    if (auto* ui = RE::UI::GetSingleton();
        ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) return false;
    const auto* vm = VM();
    return vm && vm->initialized && !vm->overstressed && vm->handlePolicy && vm->objectBindPolicy &&
        !vm->IsCompletelyFrozen();
}

void InvalidatePapyrusQueries(int delayMs) {
    papyrusDispatchAllowedAfterMs.store(NowMs() + std::max(delayMs, 0));
    arousalQueryGeneration.fetch_add(1);
    sexLabQueryGeneration.fetch_add(1);
    sexLabRoleGeneration.fetch_add(1);
    queryPending.store(false);
    sexLabQueryPending.store(false);
    sexLabRoleQueryPending.store(false);
    arousalRetryAfterMs.store(0);
    sexLabQueryRetryAfterMs.store(0);
    sexLabRoleRetryAfterMs.store(0);
}

const std::vector<RE::BSFixedString>& PhysicsBones() {
    static const std::vector<RE::BSFixedString> bones(kBones.begin(), kBones.end());
    return bones;
}

template <class... Args>
bool Call(const char* script, const char* function, Args... values) {
    auto* vm = VM();
    if (!vm || !PapyrusReadyForDispatch()) return false;
    auto* args = RE::MakeFunctionArguments(std::move(values)...);
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    return vm->DispatchStaticCall(script, function, args, callback);
}

bool CallSosAeBend(RE::Actor* actor, int bend) {
    // Never dispatch the native SOS AE Papyrus function unless its DLL is
    // actually loaded. A loose SOSAE_SKSE.pex can exist on legacy setups and
    // dispatching its unregistered native function can crash the Papyrus VM.
    return actor && SosAeNativeLoaded() &&
        Call("SOSAE_SKSE", "SetSchlongBend", actor, std::clamp(bend, 0, 20));
}

bool SetCBPCPhysics(RE::Actor* actor, bool enabled) {
    bool ok = true;
    for (const auto& bone : PhysicsBones()) {
        ok &= Call("CBPCPluginScript", enabled ? "StartPhysics" : "StopPhysics", actor, bone);
    }
    return ok;
}

bool ConfirmCurrentPhysicsOwner(std::string_view reason) {
    externalOwnerRepairDueMs.store(0);
    if (!PapyrusReadyForDispatch()) {
        externalOwnerRepairDueMs.store(NowMs() + 1000);
        return false;
    }
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.enabled || !stateKnown.load()) return false;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;

    auto* actor = static_cast<RE::Actor*>(player);
    const bool expectCBPC = usingCBPC.load();
    bool smpOK = false;
    bool cbpcOK = false;
    if (expectCBPC) {
        smpOK = Call("DynamicHDT", "TogglePhysics", actor, PhysicsBones(), false);
        cbpcOK = SetCBPCPhysics(actor, true);
    } else {
        cbpcOK = SetCBPCPhysics(actor, false);
        smpOK = Call("DynamicHDT", "TogglePhysics", actor, PhysicsBones(), true);
    }
    smpConnected.store(smpOK);
    cbpcConnected.store(cbpcOK);
    if (!smpOK || !cbpcOK) {
        Record(fmt::format("SPS-015: Could not restore {} after {} (FSMP: {}, CBPC: {})",
            expectCBPC ? "CBPC" : "SMP", reason,
            smpOK ? "OK" : "no response", cbpcOK ? "OK" : "no response"), true);
        return false;
    }

    const auto now = NowMs();
    lastExternalOwnerRepairMs.store(now);
    ignoreNodeEventsUntilMs.store(now + 2000);
    if (expectCBPC) cbpcConfirmationDueMs.store(now + 750);
    ++externalOwnerRepairs;
    Record(fmt::format("{} ownership restored after {}",
        expectCBPC ? "CBPC" : "SMP", reason));
    return true;
}

void Load() {
    const bool legacyExists = fs::exists(kLegacyIni);
    const bool newExists = fs::exists(kIni);
    bool preferLegacy = false;
    if (newExists) {
        CSimpleIniA probe;
        probe.SetUnicode();
        probe.LoadFile(kIni);
        preferLegacy = probe.GetBoolValue("Migration", "PreferLegacyIfPresent", false);
    }
    const bool migrateLegacy = legacyExists && (!newExists || preferLegacy);
    CSimpleIniA ini;
    ini.SetUnicode();
    ini.LoadFile(migrateLegacy ? kLegacyIni : kIni);
    {
        std::scoped_lock lock(settingsLock);
        settings.enabled = ini.GetBoolValue("General", "Enabled", true);
        settings.threshold = std::clamp(static_cast<float>(ini.GetDoubleValue("General", "ArousalThreshold", 60)), 0.0F, 100.0F);
        settings.hysteresis = std::clamp(static_cast<float>(ini.GetDoubleValue("General", "Hysteresis", 5)), 0.0F, 25.0F);
        settings.mode = std::clamp(static_cast<int>(ini.GetLongValue("General", "Mode", 0)), 0, 2);
        settings.erectBend = std::clamp(static_cast<int>(ini.GetLongValue("General", "ErectBend", 14)), 0, 20);
        settings.pollMs = std::clamp(static_cast<int>(ini.GetLongValue("General", "PollMilliseconds", 1000)), 250, 10000);
        settings.sexLabOverride = ini.GetBoolValue("Compatibility", "SexLabPPlusOverride", true);
        settings.sexLabRoleSwitching = ini.GetBoolValue("Compatibility", "SexLabRoleSwitching", true);
        settings.sexLabBottomBehavior = std::clamp(static_cast<int>(ini.GetLongValue("Compatibility", "SexLabBottomBehavior", 0)), 0, 3);
        settings.sexLabUnknownRole = std::clamp(static_cast<int>(ini.GetLongValue("Compatibility", "SexLabUnknownRole", 0)), 0, 2);
        settings.sceneEndDelayMs = std::clamp(static_cast<int>(ini.GetLongValue("Compatibility", "SexLabEndDelayMilliseconds", 1500)), 0, 10000);
        settings.switchCooldownMs = std::clamp(static_cast<int>(ini.GetLongValue("Reliability", "SwitchCooldownMilliseconds", 750)), 0, 5000);
        settings.resetSMPAfterLoad = ini.GetBoolValue("Reliability", "ResetSMPAfterLoad", true);
        settings.loadResetDelayMs = std::clamp(static_cast<int>(ini.GetLongValue("Reliability", "SMPResetDelayMilliseconds", 10000)), 1000, 60000);
        settings.positionControl = ini.GetBoolValue("Position", "Enabled", true);
        settings.bendMethod = std::clamp(static_cast<int>(ini.GetLongValue("Position", "Method", 2)), 0, 2);
        settings.animatePosition = ini.GetBoolValue("Position", "AnimateChanges", true);
        settings.gradualErection = ini.GetBoolValue("Position", "GradualErection", true);
        settings.erectionDurationMs = std::clamp(static_cast<int>(ini.GetLongValue("Position", "ErectionDurationMilliseconds", 3000)), 500, 10000);
        settings.bounceGuard = ini.GetBoolValue("Position", "BounceGuard", true);
        settings.useSexLabBend = ini.GetBoolValue("Position", "UseSeparateSexLabBend", false);
        settings.sexLabBend = std::clamp(static_cast<int>(ini.GetLongValue("Position", "SexLabBend", 14)), 0, 20);
        settings.settleDelayMs = std::clamp(static_cast<int>(ini.GetLongValue("Position", "SettleDelayMilliseconds", 350)), 0, 5000);
        settings.maxBendFailures = std::clamp(static_cast<int>(ini.GetLongValue("Position", "MaxAutomaticFailures", 3)), 1, 10);
        settings.verboseLogging = ini.GetBoolValue("Debug", "VerboseLogging", false);
    }
    spdlog::set_level(settings.verboseLogging ? spdlog::level::debug : spdlog::level::info);
    if (migrateLegacy || !newExists) {
        Save();
    }
    if (migrateLegacy) {
        Record("Existing UBE Physics Switch settings migrated to Schlong Physics Swapper");
    }
}

void Save() {
    std::scoped_lock lock(settingsLock);
    CSimpleIniA ini;
    ini.SetUnicode();
    ini.SetBoolValue("General", "Enabled", settings.enabled);
    ini.SetDoubleValue("General", "ArousalThreshold", settings.threshold);
    ini.SetDoubleValue("General", "Hysteresis", settings.hysteresis);
    ini.SetLongValue("General", "Mode", settings.mode);
    ini.SetLongValue("General", "ErectBend", settings.erectBend);
    ini.SetLongValue("General", "PollMilliseconds", settings.pollMs);
    ini.SetBoolValue("Compatibility", "SexLabPPlusOverride", settings.sexLabOverride);
    ini.SetBoolValue("Compatibility", "SexLabRoleSwitching", settings.sexLabRoleSwitching);
    ini.SetLongValue("Compatibility", "SexLabBottomBehavior", settings.sexLabBottomBehavior);
    ini.SetLongValue("Compatibility", "SexLabUnknownRole", settings.sexLabUnknownRole);
    ini.SetLongValue("Compatibility", "SexLabEndDelayMilliseconds", settings.sceneEndDelayMs);
    ini.SetLongValue("Reliability", "SwitchCooldownMilliseconds", settings.switchCooldownMs);
    ini.SetBoolValue("Reliability", "ResetSMPAfterLoad", settings.resetSMPAfterLoad);
    ini.SetLongValue("Reliability", "SMPResetDelayMilliseconds", settings.loadResetDelayMs);
    ini.SetBoolValue("Position", "Enabled", settings.positionControl);
    ini.SetLongValue("Position", "Method", settings.bendMethod);
    ini.SetBoolValue("Position", "AnimateChanges", settings.animatePosition);
    ini.SetBoolValue("Position", "GradualErection", settings.gradualErection);
    ini.SetLongValue("Position", "ErectionDurationMilliseconds", settings.erectionDurationMs);
    ini.SetBoolValue("Position", "BounceGuard", settings.bounceGuard);
    ini.SetBoolValue("Position", "UseSeparateSexLabBend", settings.useSexLabBend);
    ini.SetLongValue("Position", "SexLabBend", settings.sexLabBend);
    ini.SetLongValue("Position", "SettleDelayMilliseconds", settings.settleDelayMs);
    ini.SetLongValue("Position", "MaxAutomaticFailures", settings.maxBendFailures);
    ini.SetBoolValue("Debug", "VerboseLogging", settings.verboseLogging);
    fs::create_directories(fs::path(kIni).parent_path());
    ini.SaveFile(kIni);
}

void QueryArousal() {
    const auto now = NowMs();
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;

    // SLO Aroused NG and classic SexLab Aroused keep the public cached value
    // in sla_Arousal. Reading it directly avoids queueing a Papyrus call every
    // second and remains compatible with their normal update event.
    if (SloArousedLoaded() || ClassicArousedLoaded()) {
        arousalQueryGeneration.fetch_add(1);
        queryPending.store(false);
        arousalRetryAfterMs.store(0);
        auto* faction = RE::TESForm::LookupByEditorID<RE::TESFaction>("sla_Arousal");
        if (!faction) {
            arousalValid.store(false);
            oslConnected.store(false);
            Record("SPS-002: The arousal faction was not found", true);
            return;
        }
        const float value = static_cast<float>(std::clamp(player->GetFactionRank(faction, true), 0, 100));
        const auto previous = arousal.exchange(value);
        const auto wasValid = arousalValid.exchange(true);
        oslConnected.store(true);
        if (!wasValid || std::abs(previous - value) >= 0.5F)
            logger::info("{} arousal: {:.1f}", ArousalProviderName(), value);
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { Evaluate(); });
        return;
    }

    if (!OslArousedLoaded()) {
        queryPending.store(false);
        arousalValid.store(false);
        oslConnected.store(false);
        return;
    }
    if (now < arousalRetryAfterMs.load()) return;
    if (queryPending.load()) {
        if (now - queryStartedMs.load() < 5000) return;
        arousalQueryGeneration.fetch_add(1);
        queryPending.store(false);
        arousalValid.store(false);
        arousalRetryAfterMs.store(now + 5000);
        Record("SPS-002: Arousal check timed out; waiting before trying again", true);
        return;
    }
    if (!PapyrusReadyForDispatch()) {
        arousalRetryAfterMs.store(now + 1000);
        return;
    }
    if (queryPending.exchange(true)) return;
    queryStartedMs.store(now);
    auto* vm = VM();
    if (!player || !vm) { queryPending.store(false); return; }
    const auto generation = arousalQueryGeneration.fetch_add(1) + 1;

    const auto dispatch = [&](const char* script) {
        auto* args = RE::MakeFunctionArguments(static_cast<RE::Actor*>(player));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new ArousalCallback(generation) };
        return vm->DispatchStaticCall(script, "GetArousal", args, callback);
    };
    // Use OSL's native API first. The older scripted wrapper is retained only
    // as a fallback for unusual OSL installations.
    if (!dispatch("OSLArousedNative") && !dispatch("OSLAroused_ModInterface")) {
        queryPending.store(false);
        arousalValid.store(false);
        oslConnected.store(false);
        arousalRetryAfterMs.store(now + 5000);
        Record("SPS-002: Could not connect to an arousal provider", true);
    }
}

void QuerySexLab() {
    if (::GetModuleHandleW(L"SexLabUtil.dll") == nullptr) {
        sexLabValid.store(false);
        sexLabConnected.store(false);
        return;
    }
    const auto now = NowMs();
    if (now < sexLabQueryRetryAfterMs.load()) return;
    if (sexLabQueryPending.load()) {
        if (now - sexLabQueryStartedMs.load() < 5000) return;
        sexLabQueryGeneration.fetch_add(1);
        sexLabQueryPending.store(false);
        sexLabValid.store(false);
        sexLabQueryRetryAfterMs.store(now + 5000);
        return;
    }
    if (!PapyrusReadyForDispatch()) {
        sexLabQueryRetryAfterMs.store(now + 1000);
        return;
    }
    if (sexLabQueryPending.exchange(true)) return;
    sexLabQueryStartedMs.store(now);
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* vm = VM();
    if (!player || !vm) { sexLabQueryPending.store(false); return; }
    const auto generation = sexLabQueryGeneration.fetch_add(1) + 1;
    auto* args = RE::MakeFunctionArguments(static_cast<RE::Actor*>(player));
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new SexLabCallback(generation) };
    if (!vm->DispatchStaticCall("SexLabUtil", "IsActorActive", args, callback)) {
        sexLabQueryPending.store(false);
        sexLabValid.store(false);
        sexLabConnected.store(false);
        sexLabQueryRetryAfterMs.store(now + 5000);
    }
}

int DesiredBend(const Settings& copy) {
    return copy.useSexLabBend && SexLabHasPriority(copy) ? copy.sexLabBend : copy.erectBend;
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
    positionAutoSuspended.store(false);
    bendConsecutiveFailures.store(0);
    bendGuardCount.store(0);
    bendGuardUntilMs.store(0);
    bendGuardWindowStartMs.store(0);
}

bool AutomaticBendAllowed(const Settings& copy) {
    if (positionAutoSuspended.load()) return false;
    if (!copy.bounceGuard) return true;
    const auto now = NowMs();
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

bool ApplyBend(RE::Actor* actor, int bend, bool flaccid = false, bool animate = false, bool automatic = false) {
    bend = std::clamp(bend, 0, 20);
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    requestedBend.store(bend);
    if (!copy.positionControl) {
        return true;
    }
    if (automatic && !AutomaticBendAllowed(copy)) {
        return true;
    }

    const auto legacyBend = std::clamp(static_cast<int>(std::lround(bend * 9.0 / 20.0)), 0, 9);
    // A loose SOSAE_SKSE.pex does not prove that its native functions were
    // registered. Calling it without SOSAE.dll loaded can crash the Papyrus VM,
    // particularly on 1.5.97 legacy SOS setups. Legacy SOS and TNG use their
    // normal animation events instead.
    const bool nativeBackend = SosAeNativeLoaded();
    const bool eventBackend = TngLoaded() || LegacySosLoaded() || nativeBackend;
    const bool useGraph = eventBackend && (!nativeBackend ||
        (copy.bendMethod != 0 && animate && copy.animatePosition));
    const bool useNative = nativeBackend &&
        (copy.bendMethod != 1 || (copy.bendMethod == 1 && !copy.animatePosition));
    if (!useGraph && !useNative) {
        // Event-only mode intentionally has no periodic repair because replaying
        // the event is exactly what causes visible bouncing.
        return true;
    }

    bool graphOK = false;
    if (useGraph) {
        graphOK = flaccid
            ? actor->NotifyAnimationGraph("SOSFlaccid")
            : actor->NotifyAnimationGraph(RE::BSFixedString(fmt::format("SOSBend{}", legacyBend)));
    }
    const bool nativeOK = useNative && CallSosAeBend(actor, bend);
    lastBendMethod.store(useGraph && useNative ? 2 : (useGraph ? 1 : 0));
    if (nativeOK) {
        sosConnected.store(true);
    }
    if (graphOK || nativeOK) {
        appliedBend.store(bend);
        const auto now = NowMs();
        lastBendApplyMs.store(now);
        // SOS bend updates can emit a NiNode update. Ignore that echo so it
        // cannot be mistaken for a rebuilt skeleton and start a repair loop.
        ignoreNodeEventsUntilMs.store(now + 2000);
        lastBendSucceeded.store(true);
        bendConsecutiveFailures.store(0);
    } else {
        lastBendSucceeded.store(false);
        const int failures = bendConsecutiveFailures.fetch_add(1) + 1;
        if (automatic && failures >= copy.maxBendFailures && !positionAutoSuspended.exchange(true))
            Record("SPS-011: Automatic position recovery stopped after repeated SOS failures", true);
    }
    return graphOK || nativeOK;
}

void CancelErectionAnimation() {
    erectionAnimating.store(false);
    erectionAnimationGeneration.fetch_add(1);
}

void StartGradualErection(int targetBend, int durationMs) {
    targetBend = std::clamp(targetBend, 0, 20);
    durationMs = std::clamp(durationMs, 500, 10000);
    CancelErectionAnimation();
    const auto generation = erectionAnimationGeneration.load();
    erectionAnimationTargetBend.store(targetBend);
    erectionAnimationLastQueuedBend.store(-1);
    erectionAnimationStartMs.store(NowMs());
    erectionAnimating.store(true);
    requestedBend.store(targetBend);
    appliedBend.store(-1);
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const bool nativeBackend = SosAeNativeLoaded();
    const bool useGraphEvents = (TngLoaded() || LegacySosLoaded()) &&
        (!nativeBackend || copy.bendMethod == 1);

    erectionAnimationThread = std::jthread([generation, targetBend, durationMs, useGraphEvents](std::stop_token token) {
        while (!token.stop_requested() && erectionAnimating.load() &&
            generation == erectionAnimationGeneration.load()) {
            const auto elapsed = std::max<std::int64_t>(0, NowMs() - erectionAnimationStartMs.load());
            const float t = std::clamp(static_cast<float>(elapsed) / static_cast<float>(durationMs), 0.0F, 1.0F);
            const float eased = t * t * (3.0F - 2.0F * t);
            const int bend = std::clamp(static_cast<int>(std::lround(targetBend * eased)), 0, targetBend);
            const int eventBend = std::clamp(static_cast<int>(std::lround(bend * 9.0 / 20.0)), 0, 9);
            const int queueKey = useGraphEvents ? eventBend : bend;
            const int previousQueueKey = erectionAnimationLastQueuedBend.exchange(queueKey);
            if (queueKey != previousQueueKey || t >= 1.0F) {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([generation, bend, eventBend, targetBend, useGraphEvents] {
                        if (!erectionAnimating.load() || generation != erectionAnimationGeneration.load() ||
                            !stateKnown.load() || !usingCBPC.load()) return;
                        auto* player = RE::PlayerCharacter::GetSingleton();
                        if (!player) return;
                        const bool ok = useGraphEvents
                            ? player->NotifyAnimationGraph(RE::BSFixedString(fmt::format("SOSBend{}", eventBend)))
                            : CallSosAeBend(static_cast<RE::Actor*>(player), bend);
                        if (!ok) {
                            CancelErectionAnimation();
                            appliedBend.store(-1);
                            Record("SPS-009: Gradual erection unavailable; using the normal position method", true);
                            ApplyRequestedBend(true, true, false);
                            return;
                        }
                        if (!useGraphEvents) sosConnected.store(true);
                        appliedBend.store(bend);
                        lastBendMethod.store(useGraphEvents ? 1 : 0);
                        lastBendSucceeded.store(true);
                        lastBendApplyMs.store(NowMs());
                        ignoreNodeEventsUntilMs.store(NowMs() + 2000);
                        if (bend >= targetBend) {
                            erectionAnimating.store(false);
                            appliedBend.store(targetBend);
                            ++bendRepairs;
                            Record(fmt::format("Gradual erection completed: {}/20", targetBend));
                        }
                    });
                }
            }
            if (t >= 1.0F) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    Record(fmt::format("Gradual erection started: 0 to {}/20 over {:.1f} seconds",
        targetBend, durationMs / 1000.0F));
}

void ApplyRequestedBend(bool force = false, bool animate = false, bool automatic = false) {
    if (!stateKnown.load() || !usingCBPC.load()) return;
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.positionControl) return;
    // PPA directly rotates the same genital chain during an active scene.
    // Let it own position then restore the user's SOS angle after the scene.
    if (PPAOwnsPosition()) return;
    const auto now = NowMs();
    const int desired = DesiredBend(copy);
    requestedBend.store(desired);
    if (erectionAnimating.load()) return;
    if (!force && bendSettleDueMs.load() > now) return;
    // There is no read-back API for the live SOS bend. Once the requested value
    // was accepted, repeatedly sending it is not verification; it only restarts
    // SOS/CBPC movement and produces visible bouncing.
    if (!force && appliedBend.load() == desired) return;
    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        const int previous = appliedBend.load();
        const bool ok = ApplyBend(static_cast<RE::Actor*>(player), desired, false, animate, automatic);
        if (lastBendMethod.load() < 0) return;
        if (ok) {
            ++bendRepairs;
            if (previous != desired)
                Record(fmt::format("Erect vertical bend applied: {}/20", desired));
        } else if (!automatic || !positionAutoSuspended.load()) {
            Record("SPS-011: SOS bend API did not accept the position update", true);
        }
    }
}

void ConfirmSoftState() {
    if (!stateKnown.load() || usingCBPC.load()) return;
    if (!PapyrusReadyForDispatch()) {
        softConfirmationDueMs.store(NowMs() + 1000);
        return;
    }
    // A live PPA scene owns these transforms. Retry instead of consuming the
    // only confirmation, otherwise the shaft can stay erect with SMP enabled.
    if (PPAOwnsPosition()) {
        softConfirmationDueMs.store(NowMs() + 500);
        return;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    auto* actor = static_cast<RE::Actor*>(player);
    const bool cbpcOK = SetCBPCPhysics(actor, false);
    const bool smpOK = Call("DynamicHDT", "TogglePhysics", actor, PhysicsBones(), true);
    cbpcConnected.store(cbpcOK);
    smpConnected.store(smpOK);
    if (!cbpcOK || !smpOK) {
        Record(fmt::format("SPS-010: Soft-state confirmation failed (FSMP: {}, CBPC: {})",
            smpOK ? "OK" : "no response", cbpcOK ? "OK" : "no response"), true);
        return;
    }
    ApplyBend(actor, 0, true, true, false);
    Record("Soft state confirmed after physics handoff");
}

void ConfirmCBPCState() {
    if (!stateKnown.load() || !usingCBPC.load()) return;
    if (!PapyrusReadyForDispatch()) {
        cbpcConfirmationDueMs.store(NowMs() + 1000);
        return;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        cbpcConfirmationDueMs.store(NowMs() + 1000);
        return;
    }

    // SetOwner reasserts SMP-off before this delayed pass. Starting CBPC on a
    // later game tick avoids the two asynchronous Papyrus calls racing while
    // FSMP or a SexLab/PPA skeleton rebuild is still settling.
    const bool cbpcOK = SetCBPCPhysics(static_cast<RE::Actor*>(player), true);
    cbpcConnected.store(cbpcOK);
    if (!cbpcOK) {
        cbpcConfirmationDueMs.store(NowMs() + 1000);
        Record("SPS-010: Erect-state confirmation failed; retry queued", true);
        return;
    }
    Record("CBPC state confirmed after physics handoff");
}

void RunLoadSMPReset() {
    if (!PapyrusReadyForDispatch()) {
        loadSMPResetDueMs.store(NowMs() + 1000);
        return;
    }
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) {
        Record("SPS-014: Delayed SMP reset skipped because the player was unavailable", true);
        return;
    }
    if (::GetModuleHandleW(L"hdtsmp64.dll") == nullptr) {
        Record("SPS-014: Delayed SMP reset skipped because Faster HDT-SMP was not loaded", true);
        return;
    }

    // FSMP 4 exposes an actor-scoped native reset. This avoids opening the
    // console and does not reload every SMP actor in the current cell.
    const bool dispatched = Call("DynamicHDT", "ResetPhysics",
        static_cast<RE::Actor*>(player), true);
    if (!dispatched) {
        Record("SPS-014: Faster HDT-SMP did not accept the delayed player reset", true);
        return;
    }

    const auto now = NowMs();
    lastLoadSMPResetMs.store(now);
    ++loadSMPResets;
    // ResetPhysics is executed by Papyrus and FSMP queues its mesh rebuild on
    // the game thread. Give it time to finish before restoring SPS ownership.
    loadSMPResetRestoreDueMs.store(now + 750);
    Record("Player SMP reset completed after loading; physics state re-check queued");
}

void ScheduleLoadSMPReset() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    loadSMPResetRestoreDueMs.store(0);
    if (!copy.enabled || !copy.resetSMPAfterLoad) {
        loadSMPResetDueMs.store(0);
        return;
    }
    loadSMPResetDueMs.store(NowMs() + copy.loadResetDelayMs);
    Record(fmt::format("Player SMP reset scheduled in {:.1f} seconds", copy.loadResetDelayMs / 1000.0F));
}

bool SetOwner(bool cbpc, bool force = false) {
    const auto now = NowMs();
    if (!PapyrusReadyForDispatch()) {
        retryAfterMs.store(now + 1000);
        return false;
    }
    if (!force && stateKnown.load() && usingCBPC.load() == cbpc) {
        if (cbpc) {
            // `smp reset` reloads FSMP meshes and restores their bones to the
            // XML's dynamic state. It has no public reset event, so reassert
            // only the SMP-off half of the confirmed erect owner. FSMP's API
            // is idempotent and does nothing when the bones are already off.
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                smpConnected.store(Call("DynamicHDT", "TogglePhysics",
                    static_cast<RE::Actor*>(player), PhysicsBones(), false));
            }
        }
        return true;
    }
    if (!force && now < retryAfterMs.load()) return false;

    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!force && stateKnown.load() && now - lastSwitchMs.load() < copy.switchCooldownMs) return false;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return false;
    const auto previousState = stateKnown.load() ?
        (usingCBPC.load() ? SPS::API::PhysicsState::CBPC : SPS::API::PhysicsState::SMP) :
        SPS::API::PhysicsState::Unknown;
    CancelErectionAnimation();
    auto* actor = static_cast<RE::Actor*>(player);
    const auto& bones = PhysicsBones();

    bool smpOK = false;
    bool cbpcOK = true;
    if (cbpc) {
        smpOK = Call("DynamicHDT", "TogglePhysics", actor, bones, false);
        cbpcOK = SetCBPCPhysics(actor, true);
    } else {
        cbpcOK = SetCBPCPhysics(actor, false);
        smpOK = Call("DynamicHDT", "TogglePhysics", actor, bones, true);
    }

    smpConnected.store(smpOK);
    cbpcConnected.store(cbpcOK);
    if (!smpOK || !cbpcOK) {
        // Best-effort rollback keeps the previously confirmed owner in control
        // when only part of an external handoff was accepted.
        if (stateKnown.load()) {
            if (usingCBPC.load()) {
                Call("DynamicHDT", "TogglePhysics", actor, bones, false);
                SetCBPCPhysics(actor, true);
            } else {
                SetCBPCPhysics(actor, false);
                Call("DynamicHDT", "TogglePhysics", actor, bones, true);
            }
        }
        ++switchFailures;
        retryAfterMs.store(now + 1000);
        Record(fmt::format("SPS-010: Physics handoff to {} failed (FSMP: {}, CBPC: {}); retry queued",
            cbpc ? "CBPC" : "SMP", smpOK ? "OK" : "no response", cbpcOK ? "OK" : "no response"), true);
        return false;
    }

    usingCBPC.store(cbpc);
    stateKnown.store(true);
    lastSwitchMs.store(now);
    ignoreNodeEventsUntilMs.store(now + 4000);
    retryAfterMs.store(0);
    ++switchSuccesses;
    appliedBend.store(-1);
    softConfirmationDueMs.store(cbpc ? 0 : now + 750);
    cbpcConfirmationDueMs.store(cbpc ? now + 750 : 0);
    if (copy.positionControl) {
        if (cbpc) {
            requestedBend.store(DesiredBend(copy));
            bendSettleDueMs.store(now + copy.settleDelayMs);
            bendConfirmationDueMs.store(copy.gradualErection ? 0 : now + copy.settleDelayMs + 1500);
            if (copy.settleDelayMs == 0) {
                if (copy.gradualErection && !SexLabHasPriority(copy) && !PPAOwnsPosition())
                    StartGradualErection(DesiredBend(copy), copy.erectionDurationMs);
                else
                    ApplyRequestedBend(true, true, false);
            }
        } else {
            bendSettleDueMs.store(0);
            bendConfirmationDueMs.store(0);
            if (!PPAOwnsPosition()) ApplyBend(actor, 0, true, true, false);
        }
    }
    Record(fmt::format("Physics switched to {} ({})", cbpc ? "CBPC" : "SMP", cbpc ? "erect" : "soft"));
    const auto currentState = cbpc ? SPS::API::PhysicsState::CBPC : SPS::API::PhysicsState::SMP;
    if (previousState != currentState)
        NotifyAPIStateChanged(previousState, currentState);
    return true;
}

bool SexLabHasPriority(const Settings& copy) {
    if (!copy.sexLabOverride || !sexLabValid.load()) return false;
    if (sexLabActive.load()) return true;
    return sexLabEndedMs.load() > 0 && NowMs() - sexLabEndedMs.load() < copy.sceneEndDelayMs;
}

bool NormalSettingsWantCBPC(const Settings& copy) {
    if (copy.mode == 1) return false;
    if (copy.mode == 2) return true;
    if (!arousalValid.load()) return stateKnown.load() ? usingCBPC.load() : false;
    if (usingCBPC.load())
        return arousal.load() > copy.threshold - copy.hysteresis;
    return arousal.load() >= copy.threshold;
}

bool SexLabSceneWantsCBPC(const Settings& copy) {
    // Preserve the old "always erect in scenes" behavior when role switching
    // is disabled. During the post-scene delay, keep the confirmed owner so
    // normal arousal control cannot cause an immediate visible pop.
    if (!copy.sexLabRoleSwitching) return true;
    if (!sexLabActive.load()) return usingCBPC.load();
    const auto bottomWantsCBPC = [&copy] {
        if (copy.sexLabBottomBehavior == 1) return NormalSettingsWantCBPC(copy);
        if (copy.sexLabBottomBehavior == 2) return false;
        if (copy.sexLabBottomBehavior == 3) return true;
        return sexLabEntryStateValid.load() ? sexLabEntryCBPC.load() : NormalSettingsWantCBPC(copy);
    };
    const bool recentPPAUpdate = ppaSceneRoleValid.load() &&
        NowMs() - ppaLastUpdateMs.load() < 3000;

    // Penetrating wins when either bridge can positively identify it. During
    // stage changes P+ can briefly report bottom while PPA still has an exact
    // penetrating relationship; accepting that transient value caused the
    // visible CBPC -> SMP -> CBPC fight seen in the 1.8 test report.
    if (recentPPAUpdate && ppaSceneRole.load() == 2) return true;
    if (sexLabRoleValid.load()) {
        // Receiving never changes the state merely because a scene started.
        // Flaccid remains flaccid and erect remains erect until the role changes.
        if (sexLabRole.load() == 1) return bottomWantsCBPC();
        if (sexLabRole.load() == 2) return true;
        if (recentPPAUpdate && ppaSceneRole.load() == 1) return bottomWantsCBPC();
        if (copy.sexLabUnknownRole == 1) return false;
        if (copy.sexLabUnknownRole == 2) return true;
        return usingCBPC.load();
    }
    if (recentPPAUpdate && ppaSceneRole.load() == 1) return bottomWantsCBPC();
    if (copy.sexLabUnknownRole == 1) return false;
    if (copy.sexLabUnknownRole == 2) return true;
    return usingCBPC.load();
}

void QuerySexLabRole() {
    if (!sexLabActive.load() || !fs::exists("Data/Scripts/SPS_SexLabBridge.pex")) {
        sexLabRoleValid.store(false);
        sexLabRoleQueryPending.store(false);
        return;
    }
    const auto now = NowMs();
    if (now < sexLabRoleRetryAfterMs.load()) return;
    if (sexLabRoleQueryPending.load()) {
        if (now - sexLabRoleQueryStartedMs.load() < 3000) return;
        sexLabRoleGeneration.fetch_add(1);
        sexLabRoleQueryPending.store(false);
        sexLabRoleValid.store(false);
        sexLabRoleRetryAfterMs.store(now + 3000);
        return;
    }
    if (!PapyrusReadyForDispatch()) {
        sexLabRoleRetryAfterMs.store(now + 1000);
        return;
    }
    if (sexLabRoleQueryPending.exchange(true)) return;
    sexLabRoleQueryStartedMs.store(now);
    auto* vm = VM();
    if (!vm) {
        sexLabRoleQueryPending.store(false);
        return;
    }
    const auto generation = sexLabRoleGeneration.load();
    auto* args = RE::MakeFunctionArguments();
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new SexLabRoleCallback(generation) };
    if (!vm->DispatchStaticCall("SPS_SexLabBridge", "GetPlayerRole", args, callback)) {
        sexLabRoleQueryPending.store(false);
        sexLabRoleValid.store(false);
        sexLabRoleRetryAfterMs.store(now + 3000);
    }
}

void Evaluate(bool force) {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.enabled) {
        if (force && stateKnown.load()) SetOwner(false, true);
        return;
    }

    if (const auto request = ActiveAPIRequest()) {
        SetOwner(request->state == SPS::API::PhysicsState::CBPC, force);
        return;
    }

    bool cbpc = usingCBPC.load();
    if (SexLabHasPriority(copy)) {
        cbpc = SexLabSceneWantsCBPC(copy);
    } else {
        cbpc = NormalSettingsWantCBPC(copy);
    }
    SetOwner(cbpc, force);
}

void Tick() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.enabled) return;
    if (copy.mode == 0) QueryArousal();
    if (copy.sexLabOverride) QuerySexLab();
    if (copy.sexLabOverride && copy.sexLabRoleSwitching && sexLabActive.load()) QuerySexLabRole();
    Evaluate();
    CaptureState("poll");

    const auto now = NowMs();
    auto ownerRepairDue = externalOwnerRepairDueMs.load();
    if (ownerRepairDue > 0 && now >= ownerRepairDue &&
        externalOwnerRepairDueMs.compare_exchange_strong(ownerRepairDue, 0)) {
        ConfirmCurrentPhysicsOwner("an external physics reset");
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
        appliedBend.store(-1);
        Evaluate(true);
        if (stateKnown.load() && !usingCBPC.load())
            softConfirmationDueMs.store(now + 250);
        Record("Physics state restored after the delayed SMP reset");
    }

    auto softDue = softConfirmationDueMs.load();
    if (!usingCBPC.load() && softDue > 0 && now >= softDue &&
        softConfirmationDueMs.compare_exchange_strong(softDue, 0)) {
        ConfirmSoftState();
    }

    auto cbpcDue = cbpcConfirmationDueMs.load();
    if (usingCBPC.load() && cbpcDue > 0 && now >= cbpcDue &&
        cbpcConfirmationDueMs.compare_exchange_strong(cbpcDue, 0)) {
        ConfirmCBPCState();
    }

    bool settledNow = false;
    auto settleDue = bendSettleDueMs.load();
    if (usingCBPC.load() && settleDue > 0 && now >= settleDue && bendSettleDueMs.compare_exchange_strong(settleDue, 0)) {
        if (copy.gradualErection && !SexLabHasPriority(copy) && !PPAOwnsPosition())
            StartGradualErection(DesiredBend(copy), copy.erectionDurationMs);
        else
            ApplyRequestedBend(true, true, false);
        settledNow = true;
    }

    // CBPC starts through Papyrus and may finish after the first bend request.
    // Confirm the final value once, after it has settled, without replaying an
    // animation or creating the old continuous repair loop.
    auto confirmDue = bendConfirmationDueMs.load();
    if (usingCBPC.load() && confirmDue > 0 && now >= confirmDue &&
        bendConfirmationDueMs.compare_exchange_strong(confirmDue, 0)) {
        ApplyRequestedBend(true, false, false);
    }

    if (!settledNow && usingCBPC.load() && copy.positionControl) {
        const int desired = DesiredBend(copy);
        if (appliedBend.load() != desired && bendSettleDueMs.load() == 0)
            ApplyRequestedBend(true, copy.animatePosition, false);
    }

    auto nodeDue = nodeRefreshDueMs.load();
    if (nodeDue > 0 && now >= nodeDue && nodeRefreshDueMs.compare_exchange_strong(nodeDue, 0)) {
        // A real later rebuild may discard the bend, but it does not justify a
        // state decision. Re-confirm the existing owner once, then restore only
        // the position data that the rebuilt skeleton may have discarded.
        if (stateKnown.load()) ConfirmCurrentPhysicsOwner("a player skeleton rebuild");
        if (stateKnown.load() && usingCBPC.load() && copy.positionControl) {
            CancelErectionAnimation();
            appliedBend.store(-1);
            bendSettleDueMs.store(now + copy.settleDelayMs);
            bendConfirmationDueMs.store(now + copy.settleDelayMs + 1500);
            Record("Player skeleton changed; erect position queued for one repair");
        } else if (stateKnown.load() && !usingCBPC.load()) {
            softConfirmationDueMs.store(now + 750);
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

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string ReadText(const fs::path& path) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec || size > 8 * 1024 * 1024) return {};
    std::ifstream stream(path, std::ios::binary);
    return stream ? std::string(std::istreambuf_iterator<char>(stream), {}) : std::string{};
}

bool ContainsAll(const std::string& lower, const std::array<std::string, 6>& values) {
    return std::ranges::all_of(values, [&](const auto& value) { return lower.contains(value); });
}

void AddSummary(std::string& summary, const fs::path& path, int count) {
    if (count == 1) summary.clear();
    if (!summary.empty()) summary += ", ";
    summary += path.filename().string();
}

bool PluginLoaded(std::initializer_list<std::string_view> names) {
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) return false;
    return std::ranges::any_of(names, [&](auto name) { return data->LookupModByName(name) != nullptr; });
}

void RefreshDiagnostics() {
    Diagnostics result;
    result.menuFrameworkLoaded = SKSEMenuFramework::IsInstalled() && ::GetModuleHandleW(L"SKSEMenuFramework.dll") != nullptr;
    result.classicArousedPluginLoaded = PluginLoaded({ "SexLabAroused.esm" }) &&
        !OslArousedLoaded() && !SloArousedLoaded();
    result.oslModuleLoaded = OslArousedLoaded() || SloArousedLoaded() || result.classicArousedPluginLoaded;
    result.fsmpModuleLoaded = ::GetModuleHandleW(L"hdtsmp64.dll") != nullptr;
    result.cbpcModuleLoaded = ::GetModuleHandleW(L"cbp.dll") != nullptr;
    result.sexLabModuleLoaded = ::GetModuleHandleW(L"SexLabUtil.dll") != nullptr;
    result.sexLabPluginLoaded = PluginLoaded({ "SexLab.esm" });
    result.sexLabRoleBridgePresent = fs::exists("Data/Scripts/SPS_SexLabBridge.pex");
    result.oslPluginLoaded = PluginLoaded({ "OSLAroused.esp", "OAroused.esp", "SexLabAroused.esm" });
    result.tngPluginLoaded = TngLoaded();
    result.supportedAddonLoaded = PluginLoaded({
        "UBE_SOS_Addon.esp", "UBE_AllRace.esp", "3BBB UBE patch.esp",
        "SOS - Dw3BA - Futanari Addon.esp", "TheNewGentleman.esp"
    });
    result.sosPluginLoaded = LegacySosLoaded();
    result.sosScriptPresent = fs::exists("Data/Scripts/SOSAE_SKSE.pex");
    result.physicsEditorLoaded = ::GetModuleHandleW(L"PhysicsEditor.dll") != nullptr;
    result.autoPhysicsResetLoaded = ::GetModuleHandleW(L"AutoSMPReset.dll") != nullptr ||
        ::GetModuleHandleW(L"AutoPhysicsReset.dll") != nullptr ||
        ::GetModuleHandleW(L"AutoPhysicsResetNG.dll") != nullptr;
    result.crashLoggerLoaded = ::GetModuleHandleW(L"CrashLogger.dll") != nullptr;

    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        if (auto* root = player->Get3D()) {
            for (auto bone : kBones)
                if (root->GetObjectByName(RE::BSFixedString(bone))) ++result.playerBonesFound;
        }
    }

    const std::array<std::string, 6> boneKeys{
        "npc genitals01 [gen01]", "npc genitals02 [gen02]", "npc genitals03 [gen03]",
        "npc genitals04 [gen04]", "npc genitals05 [gen05]", "npc genitals06 [gen06]"
    };
    const std::array<std::string, 6> parameterKeys{ "ubeps01", "ubeps02", "ubeps03", "ubeps04", "ubeps05", "ubeps06" };
    std::error_code ec;
    const fs::path xmlRoot{ "Data/SKSE/Plugins/hdtSkinnedMeshConfigs" };
    if (fs::exists(xmlRoot, ec)) {
        for (fs::recursive_directory_iterator it(xmlRoot, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            if (!it->is_regular_file(ec) || Lower(it->path().extension().string()) != ".xml") continue;
            ++result.xmlFiles;
            const auto text = Lower(ReadText(it->path()));
            if (ContainsAll(text, boneKeys) && text.contains("<system") && text.contains("</system>")) {
                ++result.compatibleXmlFiles;
                AddSummary(result.xmlSummary, it->path(), result.compatibleXmlFiles);
            }
        }
    }

    const fs::path pluginRoot{ "Data/SKSE/Plugins" };
    if (fs::exists(pluginRoot, ec)) {
        for (fs::directory_iterator it(pluginRoot, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            if (!it->is_regular_file(ec) || Lower(it->path().extension().string()) != ".txt") continue;
            const auto filename = Lower(it->path().filename().string());
            const auto text = Lower(ReadText(it->path()));
            if (filename.contains("cbpcmasterconfig")) {
                ++result.cbpcMapFiles;
                if (ContainsAll(text, boneKeys)) {
                    ++result.compatibleCbpcMaps;
                    AddSummary(result.cbpcMapSummary, it->path(), result.compatibleCbpcMaps);
                }
            } else if (filename.starts_with("cbpconfig")) {
                ++result.cbpcParameterFiles;
                if (ContainsAll(text, parameterKeys)) {
                    ++result.compatibleCbpcParameters;
                    AddSummary(result.cbpcParameterSummary, it->path(), result.compatibleCbpcParameters);
                }
            }
        }
    }
    result.checkedAtMs = NowMs();
    { std::scoped_lock lock(diagnosticsLock); diagnostics = std::move(result); }
    Record("Compatibility health check completed");
}

void StatusLine(const char* label, const char* status, int level) {
    ImGuiMCP::Text("%s", label);
    ImGuiMCP::SameLine(245.0F);
    const ImGuiMCP::ImVec4 color = level == 2 ? ImGuiMCP::ImVec4(0.35F, 1.0F, 0.45F, 1.0F)
        : level == 1 ? ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F)
        : ImGuiMCP::ImVec4(1.0F, 0.35F, 0.35F, 1.0F);
    ImGuiMCP::TextColored(color, "%s", status);
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
    value.positionControl = true;
    value.bendMethod = 2;
    value.animatePosition = true;
    value.gradualErection = true;
    value.erectionDurationMs = 3000;
    value.bounceGuard = true;
    value.settleDelayMs = 350;
    value.maxBendFailures = 3;
}

void SaveSettingsAndApply(const Settings& copy, const Settings& previous) {
    const bool positionChanged = copy.erectBend != previous.erectBend ||
        copy.sexLabBend != previous.sexLabBend || copy.useSexLabBend != previous.useSexLabBend ||
        copy.positionControl != previous.positionControl || copy.bendMethod != previous.bendMethod ||
        copy.animatePosition != previous.animatePosition || copy.gradualErection != previous.gradualErection ||
        copy.erectionDurationMs != previous.erectionDurationMs;
    { std::scoped_lock lock(settingsLock); settings = copy; }
    if (previous.enabled && !copy.enabled) {
        ClearAPIRequests();
        externalOwnerRepairDueMs.store(0);
    }
    spdlog::set_level(copy.verboseLogging ? spdlog::level::debug : spdlog::level::info);
    Save();
    if (positionChanged) {
        CancelErectionAnimation();
        ResetPositionRecovery();
        appliedBend.store(-1);
        if (stateKnown.load() && usingCBPC.load())
            bendConfirmationDueMs.store(NowMs() + 1000);
    }
    if (auto* tasks = SKSE::GetTaskInterface()) {
        tasks->AddTask([positionChanged, enabled = copy.enabled, positionEnabled = copy.positionControl] {
            if (positionChanged && positionEnabled) ApplyRequestedBend(true, true, false);
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
    const bool coreReady = d.menuFrameworkLoaded && d.oslModuleLoaded && d.oslPluginLoaded &&
        d.fsmpModuleLoaded && d.cbpcModuleLoaded && d.playerBonesFound == 6 &&
        d.compatibleXmlFiles > 0 && d.compatibleCbpcMaps > 0 && d.compatibleCbpcParameters > 0 &&
        !d.physicsEditorLoaded;

    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "RIGHT NOW");
    StatusLine("SPS", !healthChecked ? "Checking your setup..." : (coreReady ? (stateKnown.load() ? "Ready" : "Waiting for the player") : "Needs attention"), !healthChecked || !stateKnown.load() ? 1 : (coreReady ? 2 : 0));
    const auto arousalText = arousalValid.load() ? fmt::format("Current arousal: {:.0f} / 100", arousal.load()) : "Waiting for your arousal mod";
    ImGuiMCP::ProgressBar(arousalValid.load() ? arousal.load() / 100.0F : 0.0F, ImGuiMCP::ImVec2(-1.0F, 0.0F), arousalText.c_str());
    StatusLine("Current state", stateKnown.load() ? (usingCBPC.load() ? "Erect - CBPC" : "Soft - SMP") : "Not decided yet", stateKnown.load() ? 2 : 1);
    if (sexLabActive.load())
        StatusLine("Current scene role", sexLabRoleValid.load() ? SexLabRoleName(sexLabRole.load()) : "Checking...", sexLabRoleValid.load() ? 2 : 1);
    if (healthChecked && !coreReady)
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F), "Open Help and reports to see what needs attention.");

    ImGuiMCP::Separator();
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "EVERYDAY SETTINGS");
    changed |= ImGuiMCP::Checkbox("Turn SPS on", &copy.enabled);
    const char* modes[]{ "Automatic - follow arousal", "Keep soft - SMP", "Keep erect - CBPC" };
    changed |= ImGuiMCP::Combo("What should SPS do?", &copy.mode, modes, 3);

    ImGuiMCP::BeginDisabled(copy.mode != 0);
    changed |= ImGuiMCP::SliderFloat("Arousal needed to become erect", &copy.threshold, 0, 100, "%.0f");
    ImGuiMCP::EndDisabled();
    if (copy.mode == 0)
        ImGuiMCP::TextWrapped("Becomes erect at %.0f arousal. Returns to soft below %.0f.", copy.threshold, std::max(0.0F, copy.threshold - copy.hysteresis));

    ImGuiMCP::Separator();
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "ERECT LOOK");
    ImGuiMCP::BeginDisabled(!copy.positionControl);
    changed |= ImGuiMCP::SliderInt("How high when erect", &copy.erectBend, 0, 20);
    ImGuiMCP::EndDisabled();
    ImGuiMCP::TextWrapped("0 is straight out. 20 is the highest position.");
    if (!copy.positionControl)
        ImGuiMCP::TextWrapped("Angle control is turned off on the Fine tuning page.");
    changed |= ImGuiMCP::Checkbox("Raise gradually instead of popping up", &copy.gradualErection);
    ImGuiMCP::BeginDisabled(!copy.gradualErection);
    float erectionSeconds = copy.erectionDurationMs / 1000.0F;
    if (ImGuiMCP::SliderFloat("Time to fully raise", &erectionSeconds, 0.5F, 10.0F, "%.1f seconds")) {
        copy.erectionDurationMs = static_cast<int>(std::lround(erectionSeconds * 1000.0F));
        changed = true;
    }
    ImGuiMCP::EndDisabled();
    ImGuiMCP::BeginDisabled(!copy.positionControl || !stateKnown.load() || !usingCBPC.load());
    if (ImGuiMCP::Button("Apply this angle now")) {
        ResetPositionRecovery();
        appliedBend.store(-1);
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { ApplyRequestedBend(true, true, false); });
    }
    ImGuiMCP::EndDisabled();

    ImGuiMCP::Separator();
    if (ImGuiMCP::Button("Check now"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { QueryArousal(); QuerySexLab(); QuerySexLabRole(); Evaluate(); });
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Reset to recommended settings")) { UseRecommendedSettings(copy); changed = true; }
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
    const bool ppaLoaded = ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr;

    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "SCENE STATUS");
    StatusLine("SexLab P+", sexLabLoaded ? (sexLabConnected.load() ? "Ready" : "Loading...") : "Not installed (optional)", sexLabLoaded ? (sexLabConnected.load() ? 2 : 1) : 1);
    StatusLine("Player scene", sexLabActive.load() ? (sexLabRoleValid.load() ? SexLabRoleName(sexLabRole.load()) : "Running - checking role") : "Not running", sexLabActive.load() ? (sexLabRoleValid.load() ? 2 : 1) : 2);
    StatusLine("PPA", ppaLoaded ? (PPAOwnsPosition() ? "Controlling the scene angle" : "Ready") : "Not installed (optional)", ppaLoaded ? 2 : 1);

    ImGuiMCP::Separator();
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "HOW SPS HANDLES SCENES");
    changed |= ImGuiMCP::Checkbox("Let SPS manage physics during player scenes", &copy.sexLabOverride);
    ImGuiMCP::BeginDisabled(!copy.sexLabOverride);
    changed |= ImGuiMCP::Checkbox("Use the player's role in the scene", &copy.sexLabRoleSwitching);
    ImGuiMCP::BeginDisabled(!copy.sexLabRoleSwitching);
    const char* bottomBehaviors[]{
        "Keep whatever state I had (recommended)",
        "Follow live arousal",
        "Always stay soft (SMP)",
        "Always stay erect (CBPC)"
    };
    changed |= ImGuiMCP::Combo("When receiving / bottom", &copy.sexLabBottomBehavior, bottomBehaviors, 4);
    ImGuiMCP::EndDisabled();
    float returnDelaySeconds = copy.sceneEndDelayMs / 1000.0F;
    if (ImGuiMCP::SliderFloat("Wait before returning to normal", &returnDelaySeconds, 0.0F, 10.0F, "%.1f seconds")) {
        copy.sceneEndDelayMs = static_cast<int>(std::lround(returnDelaySeconds * 1000.0F));
        changed = true;
    }
    ImGuiMCP::EndDisabled();
    ImGuiMCP::TextWrapped("Recommended: receiving keeps the state from just before the scene. Penetrating uses CBPC.");

    ImGuiMCP::Separator();
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "SCENE ANGLE");
    const bool sexLabPositionChanged = ImGuiMCP::Checkbox("Use a different erect angle in scenes", &copy.useSexLabBend);
    changed |= sexLabPositionChanged;
    if (sexLabPositionChanged && copy.useSexLabBend) copy.sexLabOverride = true;
    ImGuiMCP::BeginDisabled(!copy.useSexLabBend || !copy.positionControl);
    changed |= ImGuiMCP::SliderInt("Scene erect angle", &copy.sexLabBend, 0, 20);
    ImGuiMCP::EndDisabled();
    if (ppaLoaded)
        ImGuiMCP::TextWrapped("PPA controls the live angle during its scenes. SPS only decides whether physics should be soft or erect.");

    if (ImGuiMCP::CollapsingHeader("Unusual or unrecognised scenes")) {
        const char* unknownRoles[]{ "Keep the current state (recommended)", "Use soft physics", "Use erect physics" };
        changed |= ImGuiMCP::Combo("If SPS cannot identify the role", &copy.sexLabUnknownRole, unknownRoles, 3);
        ImGuiMCP::TextWrapped("Keeping the current state avoids a sudden visible change when a scene does not report a clear role.");
    }

    if (changed) SaveSettingsAndApply(copy, previous);
}

void __stdcall RenderAdvanced() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const Settings previous = copy;
    bool changed = false;

    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "FINE TUNING");
    ImGuiMCP::TextWrapped("Most people can leave this page alone. The recommended settings are designed to work without extra tuning.");

    ImGuiMCP::Separator();
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "AFTER LOADING A SAVE");
    changed |= ImGuiMCP::Checkbox("Fix player physics once after loading", &copy.resetSMPAfterLoad);
    ImGuiMCP::BeginDisabled(!copy.resetSMPAfterLoad);
    float loadResetSeconds = copy.loadResetDelayMs / 1000.0F;
    if (ImGuiMCP::SliderFloat("Wait before the load fix", &loadResetSeconds, 1.0F, 30.0F, "%.0f seconds")) {
        copy.loadResetDelayMs = static_cast<int>(std::lround(loadResetSeconds * 1000.0F));
        changed = true;
    }
    ImGuiMCP::EndDisabled();
    ImGuiMCP::TextWrapped("SPS resets the player's SMP once, then restores the correct soft or erect state.");

    if (ImGuiMCP::CollapsingHeader("Arousal switching timing")) {
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
    }

    ImGuiMCP::Separator();
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "ANGLE CONTROL");
    changed |= ImGuiMCP::Checkbox("Let SPS control the erect angle", &copy.positionControl);
    ImGuiMCP::TextWrapped("Turn this off only when another mod should control the angle. Soft/erect physics switching will still work.");
    ImGuiMCP::BeginDisabled(!copy.positionControl);
    const char* bendMethods[]{ "SOS AE direct control", "SOS / TNG animation stages", "Choose automatically (recommended)" };
    changed |= ImGuiMCP::Combo("How SPS sets the angle", &copy.bendMethod, bendMethods, 3);
    changed |= ImGuiMCP::Checkbox("Smooth angle changes", &copy.animatePosition);
    changed |= ImGuiMCP::Checkbox("Stop repeated bouncing", &copy.bounceGuard);
    ImGuiMCP::EndDisabled();

    if (positionAutoSuspended.load() && ImGuiMCP::Button("Resume position recovery")) {
        ResetPositionRecovery();
        appliedBend.store(-1);
    }

    if (ImGuiMCP::CollapsingHeader("Angle recovery timing")) {
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
    if (ImGuiMCP::Button("Reset fine tuning to recommended")) { UseRecommendedSettings(copy); changed = true; }

    if (changed) SaveSettingsAndApply(copy, previous);
}

std::string BuildReport() {
    Diagnostics d;
    { std::scoped_lock lock(diagnosticsLock); d = diagnostics; }
    Settings s;
    { std::scoped_lock lock(settingsLock); s = settings; }
    std::string recent;
    std::string error;
    { std::scoped_lock lock(activityLock); recent = lastAction; error = lastError; }
    std::string fixes;
    const auto activeAPIRequest = ActiveAPIRequest();
    std::size_t activeAPIRequestCount = 0;
    { std::scoped_lock lock(apiLock); activeAPIRequestCount = apiRequests.size(); }
    for (const auto& [code, suggestion] : SuggestedFixes(d))
        fixes += fmt::format("{}: {}\n", code, suggestion);
    return fmt::format(
        "Schlong Physics Swapper {} diagnostics\n"
        "SkyrimRuntime={} SKSE={}\n"
        "DLLs: MenuFramework={} OSL={} SLO={} FSMP={} CBPC={} SexLab={} SOSAE={}\n"
        "Compatibility: TNG={} PositionBackend={} ClassicSexLabAroused={} SexLabRoleBridge={} PPA={} PhysicsEditor={} AutoPhysicsReset={} CrashLogger={}\n"
        "Engine={} StateKnown={} Arousal={:.1f} Provider={} ProviderConnected={} SexLabActive={} SexLabConnected={} SexLabRole={} RoleValid={}\n"
        "CompatibilityAPI=V{} ActiveRequests={} ActiveRequester={} Accepted={} Released={} ResetNotices={} OwnerRepairs={} LastOwnerRepairMs={}\n"
        "MenuFramework={} ArousalProvider={} FSMP={} CBPC={} SexLabPPlus={} PositionBackendReady={} SupportedAddon={}\n"
        "PlayerBones={}/6 XML={}/{} [{}]\nCBPCMap={}/{} [{}]\nCBPCParameters={}/{} [{}]\n"
        "SwitchSuccesses={} SwitchFailures={} BendApplies={} LoadSMPResets={} LastLoadSMPResetMs={} LastAction={} LastError={}\n"
        "Position: Enabled={} Requested={} Applied={} LastMethod={} LastSucceeded={} AutoSuspended={} GuardRemainingMs={}\n"
        "Settings: Enabled={} Mode={} Threshold={:.0f} Hysteresis={:.0f} Bend={} PollMs={} SexLabOverride={} SexLabRoleSwitching={} BottomBehavior={} UnknownRole={} EndDelayMs={} CooldownMs={} ResetSMPAfterLoad={} LoadResetDelayMs={} BendMethod={} Animate={} Gradual={} ErectionMs={} BounceGuard={} SettleMs={} SeparateSexLabBend={} SexLabBend={} MaxFailures={} PPA={} VerboseLogging={}\n"
        "\nSuggested fixes\n{}",
        kVersion, runtimeVersion, skseVersion,
        LoadedDllVersion(L"SKSEMenuFramework.dll"), LoadedDllVersion(L"OSLAroused.dll"),
        LoadedDllVersion(L"SexlabArousedNG.dll"),
        LoadedDllVersion(L"hdtsmp64.dll"), LoadedDllVersion(L"cbp.dll"),
        LoadedDllVersion(L"SexLabUtil.dll"), LoadedDllVersion(L"SOSAE.dll"),
        d.tngPluginLoaded, PositionBackendName(), d.classicArousedPluginLoaded, d.sexLabRoleBridgePresent,
        ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr, d.physicsEditorLoaded,
        d.autoPhysicsResetLoaded, d.crashLoggerLoaded,
        stateKnown.load() ? (usingCBPC.load() ? "CBPC" : "SMP") : "unknown", stateKnown.load(), arousal.load(), ArousalProviderName(), oslConnected.load(), sexLabActive.load(), sexLabConnected.load(), SexLabRoleName(sexLabRole.load()), sexLabRoleValid.load(),
        SPS::API::kVersion, activeAPIRequestCount, activeAPIRequest ? activeAPIRequest->requester : "none", apiRequestsAccepted.load(), apiRequestsReleased.load(),
        externalResetNotices.load(), externalOwnerRepairs.load(), lastExternalOwnerRepairMs.load(),
        d.menuFrameworkLoaded, d.oslModuleLoaded && d.oslPluginLoaded, d.fsmpModuleLoaded, d.cbpcModuleLoaded, d.sexLabModuleLoaded && d.sexLabPluginLoaded,
        d.sosScriptPresent || d.sosPluginLoaded || d.tngPluginLoaded || sosConnected.load(), d.supportedAddonLoaded,
        d.playerBonesFound, d.compatibleXmlFiles, d.xmlFiles, d.xmlSummary, d.compatibleCbpcMaps, d.cbpcMapFiles, d.cbpcMapSummary,
        d.compatibleCbpcParameters, d.cbpcParameterFiles, d.cbpcParameterSummary, switchSuccesses.load(), switchFailures.load(), bendRepairs.load(), loadSMPResets.load(), lastLoadSMPResetMs.load(), recent, error.empty() ? "none" : error,
        s.positionControl, requestedBend.load(), appliedBend.load(), BendMethodName(lastBendMethod.load()), lastBendSucceeded.load(), positionAutoSuspended.load(), std::max<std::int64_t>(0, bendGuardUntilMs.load() - NowMs()),
        s.enabled, s.mode, s.threshold, s.hysteresis, s.erectBend, s.pollMs, s.sexLabOverride, s.sexLabRoleSwitching, s.sexLabBottomBehavior, s.sexLabUnknownRole, s.sceneEndDelayMs, s.switchCooldownMs, s.resetSMPAfterLoad, s.loadResetDelayMs,
        s.bendMethod, s.animatePosition, s.gradualErection, s.erectionDurationMs, s.bounceGuard, s.settleDelayMs, s.useSexLabBend, s.sexLabBend, s.maxBendFailures,
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
    if (!SetOwner(cbpc, true)) return;
    if (!cbpc) ConfirmSoftState();
    CaptureState(cbpc ? "manual erect test" : "manual soft test");
}

void RepairPhysics() {
    ResetPositionRecovery();
    retryAfterMs.store(0);
    appliedBend.store(-1);
    Record("Physics repair requested");
    Evaluate(true);
    RefreshDiagnostics();
    CaptureState("manual repair");
}

void __stdcall RenderDebug() {
    Diagnostics d;
    { std::scoped_lock lock(diagnosticsLock); d = diagnostics; }
    Settings debugSettings;
    { std::scoped_lock lock(settingsLock); debugSettings = settings; }
    const Settings previousDebugSettings = debugSettings;

    const bool coreReady = d.menuFrameworkLoaded && d.oslModuleLoaded && d.oslPluginLoaded &&
        d.fsmpModuleLoaded && d.cbpcModuleLoaded && d.playerBonesFound == 6 &&
        d.compatibleXmlFiles > 0 && d.compatibleCbpcMaps > 0 && d.compatibleCbpcParameters > 0 &&
        !d.physicsEditorLoaded;
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "HELP AND REPORTS");
    StatusLine("Setup", d.checkedAtMs == 0 ? "Not checked yet" : (coreReady ? "Everything looks good" : "Something needs attention"), d.checkedAtMs == 0 ? 1 : (coreReady ? 2 : 0));
    if (ImGuiMCP::Button("Check my setup again"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask(RefreshDiagnostics);
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Repair settings")) {
        Settings fixed;
        { std::scoped_lock lock(settingsLock); fixed = settings; }
        const Settings previous = fixed;
        UseRecommendedSettings(fixed);
        SaveSettingsAndApply(fixed, previous);
    }
    ImGuiMCP::TextWrapped("Green means ready, yellow means optional or still checking, and red means something needs fixing. Repair settings restores SPS's safe choices but cannot install missing mods.");
    ImGuiMCP::Separator();

    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "QUICK FIXES");
    if (ImGuiMCP::Button("Show soft physics"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { TestPhysicsState(false); });
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Show erect physics"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { TestPhysicsState(true); });
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Repair current state"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask(RepairPhysics);
    ImGuiMCP::TextWrapped("The two test buttons are temporary. Automatic control takes over again on the next check.");
    ImGuiMCP::Separator();

    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "WHAT SPS FOUND");
    const auto providerName = ArousalProviderName();
    StatusLine("Arousal mod", d.oslModuleLoaded && d.oslPluginLoaded ? (oslConnected.load() ? fmt::format("{} - ready", providerName).c_str() : fmt::format("{} - still checking", providerName).c_str()) : "Missing", d.oslModuleLoaded && d.oslPluginLoaded ? (oslConnected.load() ? 2 : 1) : 0);
    StatusLine("Soft physics", d.fsmpModuleLoaded ? (smpConnected.load() ? "SMP - ready" : "SMP found - not tested yet") : "Faster HDT-SMP is missing", d.fsmpModuleLoaded ? (smpConnected.load() ? 2 : 1) : 0);
    StatusLine("Erect physics", d.cbpcModuleLoaded ? (cbpcConnected.load() ? "CBPC - ready" : "CBPC found - not tested yet") : "CBPC is missing", d.cbpcModuleLoaded ? (cbpcConnected.load() ? 2 : 1) : 0);
    StatusLine("Compatible schlong", d.playerBonesFound == 6 ? "All 6 physics bones found" : fmt::format("Only {}/6 physics bones found", d.playerBonesFound).c_str(), d.playerBonesFound == 6 ? 2 : 0);
    const bool positionBackendFound = d.sosScriptPresent || d.sosPluginLoaded || d.tngPluginLoaded;
    StatusLine("Erect angle control", positionBackendFound ? fmt::format("{} - ready", PositionBackendName()).c_str() : "SOS AE, SOS or TNG not found", positionBackendFound ? 2 : 0);

    if (SloArousedLoaded())
        ImGuiMCP::TextWrapped("SLO Aroused users: leave SLO's Use SOS option off so both mods do not change the angle.");
    if (d.classicArousedPluginLoaded)
        ImGuiMCP::TextWrapped("SexLab Aroused Redux users: leave Enable SOS off so both mods do not change the angle.");

    if (d.physicsEditorLoaded)
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.35F, 0.35F, 1.0F), "Physics Editor is running. Disable it because it controls the same physics as SPS.");
    if (d.autoPhysicsResetLoaded)
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F), "Auto Physics Reset is also running. Turn off its load, cell or scene resets if the state changes unexpectedly.");

    if (ImGuiMCP::CollapsingHeader("Optional scene mods")) {
        StatusLine("SexLab P+", d.sexLabModuleLoaded && d.sexLabPluginLoaded ? (sexLabConnected.load() ? "Ready" : "Found - still checking") : "Not installed", d.sexLabModuleLoaded && d.sexLabPluginLoaded ? (sexLabConnected.load() ? 2 : 1) : 1);
        if (sexLabActive.load())
            StatusLine("Current scene role", sexLabRoleValid.load() ? SexLabRoleName(sexLabRole.load()) : "Checking...", sexLabRoleValid.load() ? 2 : 1);
        const bool ppaLoaded = ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr;
        StatusLine("PPA", ppaLoaded ? (PPAOwnsPosition() ? "Controlling the scene angle" : "Ready") : "Not installed", ppaLoaded ? 2 : 1);
        if (d.sexLabModuleLoaded && d.sexLabPluginLoaded)
            StatusLine("Scene role support", d.sexLabRoleBridgePresent ? "Ready" : "Missing - reinstall SPS", d.sexLabRoleBridgePresent ? 2 : 0);
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
        std::size_t activeRequests = 0;
        { std::scoped_lock lock(apiLock); activeRequests = apiRequests.size(); }
        StatusLine("Mod compatibility API", "V1 - ready", 2);
        ImGuiMCP::Text("Other mods currently controlling physics: %zu", activeRequests);
        ImGuiMCP::Text("External reset repairs: %u", externalOwnerRepairs.load());
        ImGuiMCP::Text("Successful physics changes: %u", switchSuccesses.load());
        ImGuiMCP::Text("Failed physics changes: %u", switchFailures.load());
        ImGuiMCP::Text("Erect angle applications: %u", bendRepairs.load());
        ImGuiMCP::Text("Requested / applied angle: %d / %d", requestedBend.load(), appliedBend.load());
        ImGuiMCP::Text("Last angle method: %s", BendMethodName(lastBendMethod.load()));
        StatusLine("Gradual erection", erectionAnimating.load() ? fmt::format("Moving: {}/{}", appliedBend.load(), erectionAnimationTargetBend.load()).c_str() : "Not moving", erectionAnimating.load() ? 1 : 2);
        StatusLine("Angle repair", positionAutoSuspended.load() ? "Paused after failures" : (NowMs() < bendGuardUntilMs.load() ? "Waiting for bouncing to stop" : "Ready"), positionAutoSuspended.load() ? 0 : (NowMs() < bendGuardUntilMs.load() ? 1 : 2));
    }

    ImGuiMCP::Separator();
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "WHAT TO DO NEXT");
    const auto suggestions = SuggestedFixes(d);
    if (suggestions.size() == 1 && suggestions.front().first == "SPS-000")
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.35F, 1.0F, 0.45F, 1.0F), "No fixes are currently needed.");
    else {
        for (const auto& item : suggestions)
            ImGuiMCP::TextWrapped("- %s", item.second.c_str());
    }
    {
        std::scoped_lock lock(activityLock);
        if (!lastError.empty())
            ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.45F, 0.35F, 1.0F), "Most recent problem: %s", lastError.c_str());
        if (ImGuiMCP::CollapsingHeader("Recent SPS activity")) {
            ImGuiMCP::TextWrapped("Last action: %s", lastAction.c_str());
            for (const auto& entry : activity) ImGuiMCP::TextWrapped("- %s", entry.c_str());
        }
    }

    ImGuiMCP::Separator();
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "NEED HELP?");
    ImGuiMCP::TextWrapped("If you can repeat a problem, record the next 30 seconds, close the menu, make it happen, then return here and save the report.");
    if (DebugCaptureActive()) {
        const auto remaining = std::max<std::int64_t>(0, debugCaptureUntilMs.load() - NowMs());
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F), "Recording... reproduce the problem now (%lld seconds left)", (remaining + 999) / 1000);
    } else if (ImGuiMCP::Button("Record the next 30 seconds")) {
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

    if (ImGuiMCP::CollapsingHeader("Extra logging")) {
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
    SKSEMenuFramework::AddSectionItem("Scene behaviour", RenderScenes);
    SKSEMenuFramework::AddSectionItem("Fine tuning", RenderAdvanced);
    SKSEMenuFramework::AddSectionItem("Help and reports", RenderDebug);
}

class ModEventSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        const std::string_view name = event->eventName.c_str();
        if (name == "HookAnimationStart" || name == "HookAnimationStarting" ||
            name == "HookStageStart" || name == "HookStageEnd" ||
            name == "HookActorsRelocated" || name == "HookActorChangeEnd" ||
            name == "HookAnimationEnding" || name == "HookAnimationEnd" ||
            name == "AnimationStart" || name == "AnimationEnd") {
            sexLabRoleGeneration.fetch_add(1);
            sexLabRoleQueryPending.store(false);
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] {
                QuerySexLab();
                if (sexLabActive.load()) QuerySexLabRole();
            });
        } else if (name == "OSLA_ActorArousalUpdated" && event->sender == RE::PlayerCharacter::GetSingleton()) {
            // The callback query will evaluate the new value. Arousal updates
            // must not invalidate an already-applied bend or they create a
            // position replay loop while Automatic mode is erect.
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask(QueryArousal);
        } else if (name == "sla_UpdateComplete") {
            // SLO Aroused NG reports a completed update globally. Querying its
            // OSL compatibility stub is cheap and avoids waiting for the next poll.
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask(QueryArousal);
        } else if (name == "SexLabDisabled") {
            sexLabActive.store(false);
            sexLabValid.store(false);
            sexLabEntryStateValid.store(false);
            sexLabRoleGeneration.fetch_add(1);
            sexLabRole.store(0);
            sexLabRoleValid.store(false);
            sexLabRoleQueryPending.store(false);
            sexLabLastTopMs.store(0);
            sexLabBottomCandidateSinceMs.store(0);
            ResetPPASceneTracking(2000);
            if (stateKnown.load() && !usingCBPC.load())
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
        if (event && event->reference == RE::PlayerCharacter::GetSingleton() && now >= ignoreNodeEventsUntilMs.load())
            nodeRefreshDueMs.store(now + 750);
        return RE::BSEventNotifyControl::kContinue;
    }
};

ModEventSink modEventSink;
NiNodeSink niNodeSink;

void OnMessage(SKSE::MessagingInterface::Message* message) {
    if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
        // Any callback still queued belongs to the old game state. Give the VM
        // time to finish rebuilding before SPS sends another scripted request.
        InvalidatePapyrusQueries(3000);
        arousalValid.store(false);
        sexLabValid.store(false);
        sexLabRoleValid.store(false);
        sexLabLastTopMs.store(0);
        sexLabBottomCandidateSinceMs.store(0);
        return;
    }
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Load();
        RegisterMenu();
        RegisterPPAAPI();
        if (auto* source = SKSE::GetModCallbackEventSource()) source->AddEventSink(&modEventSink);
        if (auto* source = SKSE::GetNiNodeUpdateEventSource()) source->AddEventSink(&niNodeSink);
        RefreshDiagnostics();
        return;
    }
    if (message->type == SKSE::MessagingInterface::kPostLoadGame ||
        message->type == SKSE::MessagingInterface::kNewGame) {
        InvalidatePapyrusQueries(3000);
        ClearAPIRequests();
        externalOwnerRepairDueMs.store(0);
        StartPolling();
        arousalValid.store(false);
        sexLabValid.store(false);
        sexLabEntryStateValid.store(false);
        sexLabRole.store(0);
        sexLabRoleValid.store(false);
        sexLabLastTopMs.store(0);
        sexLabBottomCandidateSinceMs.store(0);
        ResetPPASceneTracking(2000);
        stateKnown.store(false);
        ScheduleLoadSMPReset();
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([] {
                SetOwner(false, true);
                QueryArousal();
                QuerySexLab();
                RefreshDiagnostics();
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
    SKSE::GetMessagingInterface()->RegisterListener(Mod::OnMessage);
    logger::info("Schlong Physics Swapper {} loaded", Mod::kVersion);
    return true;
}
