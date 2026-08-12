#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <SimpleIni.h>
#include <SKSEMenuFramework.h>
#include <fmt/format.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <Windows.h>
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
#include <string>
#include <thread>
#include <vector>

namespace logger = SKSE::log;

namespace Mod {
namespace fs = std::filesystem;

constexpr auto kName = "Schlong Physics Swapper";
constexpr auto kVersion = "1.4.0";
constexpr auto kIni = "Data/SKSE/Plugins/SchlongPhysicsSwapper.ini";
constexpr auto kLegacyIni = "Data/SKSE/Plugins/UBEPhysicsSwitch.ini";
constexpr auto kReport = "Data/SKSE/Plugins/SchlongPhysicsSwapper_Diagnostics.txt";
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
    int sceneEndDelayMs{ 1500 };
    int switchCooldownMs{ 750 };
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
};

struct Diagnostics {
    bool menuFrameworkLoaded{ false };
    bool oslModuleLoaded{ false };
    bool fsmpModuleLoaded{ false };
    bool cbpcModuleLoaded{ false };
    bool sexLabModuleLoaded{ false };
    bool sexLabPluginLoaded{ false };
    bool oslPluginLoaded{ false };
    bool supportedAddonLoaded{ false };
    bool sosPluginLoaded{ false };
    bool sosScriptPresent{ false };
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

Settings settings;
Diagnostics diagnostics;
std::mutex settingsLock;
std::mutex diagnosticsLock;
std::mutex activityLock;
std::deque<std::string> activity;
std::string lastAction{ "Waiting for a loaded game" };
std::string lastError;

std::atomic<float> arousal{ 0.0F };
std::atomic<bool> arousalValid{ false };
std::atomic<bool> queryPending{ false };
std::atomic<std::int64_t> queryStartedMs{ 0 };
std::atomic<bool> sexLabActive{ false };
std::atomic<bool> sexLabValid{ false };
std::atomic<bool> sexLabQueryPending{ false };
std::atomic<std::int64_t> sexLabQueryStartedMs{ 0 };
std::atomic<std::int64_t> sexLabEndedMs{ 0 };
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
std::atomic<std::int64_t> lastBendApplyMs{ 0 };
std::atomic<std::int64_t> bendSettleDueMs{ 0 };
std::atomic<std::int64_t> bendConfirmationDueMs{ 0 };
std::atomic<std::int64_t> softConfirmationDueMs{ 0 };
std::atomic<std::int64_t> erectionAnimationStartMs{ 0 };
std::atomic<std::int64_t> bendGuardUntilMs{ 0 };
std::atomic<std::int64_t> bendGuardWindowStartMs{ 0 };
std::atomic<std::int64_t> nodeRefreshDueMs{ 0 };
std::atomic<std::int64_t> ignoreNodeEventsUntilMs{ 0 };
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
std::jthread pollThread;
std::jthread erectionAnimationThread;

void Evaluate(bool force = false);
void QuerySexLab();
void RefreshDiagnostics();
void Save();
bool SexLabHasPriority(const Settings& copy);
void ApplyRequestedBend(bool force, bool animate, bool automatic);
void CancelErectionAnimation();

bool PPAOwnsPosition() {
    return ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr &&
        sexLabValid.load() && sexLabActive.load();
}

std::int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void Record(std::string message, bool error = false) {
    if (error) logger::error("{}", message);
    else logger::info("{}", message);
    std::scoped_lock lock(activityLock);
    lastAction = message;
    if (error) lastError = message;
    activity.push_front(std::move(message));
    while (activity.size() > 12) activity.pop_back();
}

class ArousalCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    void operator()(RE::BSScript::Variable a_result) override {
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
                logger::info("OSL arousal: {:.1f}", value);
        } else {
            arousalValid.store(false);
            Record("OSL returned an invalid arousal value", true);
        }
        queryPending.store(false);
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { Evaluate(); });
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
};

class SexLabCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    void operator()(RE::BSScript::Variable a_result) override {
        if (a_result.IsBool()) {
            const bool active = a_result.GetBool();
            const bool previous = sexLabActive.exchange(active);
            sexLabValid.store(true);
            sexLabConnected.store(true);
            if (previous && !active) {
                sexLabEndedMs.store(NowMs());
                Record("SexLab scene ended; post-scene hold started");
            } else if (!previous && active) {
                Record("SexLab scene detected; CBPC override requested");
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
        }
        sexLabQueryPending.store(false);
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { Evaluate(); });
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
};

auto VM() { return RE::BSScript::Internal::VirtualMachine::GetSingleton(); }

const std::vector<RE::BSFixedString>& PhysicsBones() {
    static const std::vector<RE::BSFixedString> bones(kBones.begin(), kBones.end());
    return bones;
}

template <class... Args>
bool Call(const char* script, const char* function, Args... values) {
    auto* vm = VM();
    if (!vm) return false;
    auto* args = RE::MakeFunctionArguments(std::move(values)...);
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    return vm->DispatchStaticCall(script, function, args, callback);
}

bool SetCBPCPhysics(RE::Actor* actor, bool enabled) {
    bool ok = true;
    for (const auto& bone : PhysicsBones()) {
        ok &= Call("CBPCPluginScript", enabled ? "StartPhysics" : "StopPhysics", actor, bone);
    }
    return ok;
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
        settings.sceneEndDelayMs = std::clamp(static_cast<int>(ini.GetLongValue("Compatibility", "SexLabEndDelayMilliseconds", 1500)), 0, 10000);
        settings.switchCooldownMs = std::clamp(static_cast<int>(ini.GetLongValue("Reliability", "SwitchCooldownMilliseconds", 750)), 0, 5000);
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
    }
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
    ini.SetLongValue("Compatibility", "SexLabEndDelayMilliseconds", settings.sceneEndDelayMs);
    ini.SetLongValue("Reliability", "SwitchCooldownMilliseconds", settings.switchCooldownMs);
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
    fs::create_directories(fs::path(kIni).parent_path());
    ini.SaveFile(kIni);
}

void QueryArousal() {
    if (queryPending.load()) {
        if (NowMs() - queryStartedMs.load() < 5000) return;
        Record("OSL query timed out; retrying", true);
        queryPending.store(false);
        arousalValid.store(false);
    }
    if (queryPending.exchange(true)) return;
    queryStartedMs.store(NowMs());
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* vm = VM();
    if (!player || !vm) { queryPending.store(false); return; }
    const auto dispatch = [&](const char* script) {
        auto* args = RE::MakeFunctionArguments(static_cast<RE::Actor*>(player));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new ArousalCallback() };
        return vm->DispatchStaticCall(script, "GetArousal", args, callback);
    };
    if (!dispatch("OSLAroused_ModInterface") && !dispatch("OSLArousedNative")) {
        queryPending.store(false);
        arousalValid.store(false);
        oslConnected.store(false);
        Record("Could not connect to OSL Aroused", true);
    }
}

void QuerySexLab() {
    if (::GetModuleHandleW(L"SexLabUtil.dll") == nullptr) {
        sexLabValid.store(false);
        sexLabConnected.store(false);
        return;
    }
    if (sexLabQueryPending.load()) {
        if (NowMs() - sexLabQueryStartedMs.load() < 5000) return;
        sexLabQueryPending.store(false);
        sexLabValid.store(false);
    }
    if (sexLabQueryPending.exchange(true)) return;
    sexLabQueryStartedMs.store(NowMs());
    auto* player = RE::PlayerCharacter::GetSingleton();
    auto* vm = VM();
    if (!player || !vm) { sexLabQueryPending.store(false); return; }
    auto* args = RE::MakeFunctionArguments(static_cast<RE::Actor*>(player));
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new SexLabCallback() };
    if (!vm->DispatchStaticCall("SexLabUtil", "IsActorActive", args, callback)) {
        sexLabQueryPending.store(false);
        sexLabValid.store(false);
        sexLabConnected.store(false);
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
    const bool useGraph = animate && copy.animatePosition && copy.bendMethod != 0;
    const bool useNative = copy.bendMethod != 1 || (copy.bendMethod == 1 && !copy.animatePosition);
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
    const bool nativeOK = useNative && Call("SOSAE_SKSE", "SetSchlongBend", actor, bend);
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
            Record("Automatic position recovery stopped after repeated SOS failures", true);
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

    erectionAnimationThread = std::jthread([generation, targetBend, durationMs](std::stop_token token) {
        while (!token.stop_requested() && erectionAnimating.load() &&
            generation == erectionAnimationGeneration.load()) {
            const auto elapsed = std::max<std::int64_t>(0, NowMs() - erectionAnimationStartMs.load());
            const float t = std::clamp(static_cast<float>(elapsed) / static_cast<float>(durationMs), 0.0F, 1.0F);
            const float eased = t * t * (3.0F - 2.0F * t);
            const int bend = std::clamp(static_cast<int>(std::lround(targetBend * eased)), 0, targetBend);
            if (bend != erectionAnimationLastQueuedBend.exchange(bend)) {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([generation, bend, targetBend] {
                        if (!erectionAnimating.load() || generation != erectionAnimationGeneration.load() ||
                            !stateKnown.load() || !usingCBPC.load()) return;
                        auto* player = RE::PlayerCharacter::GetSingleton();
                        if (!player) return;
                        const bool ok = Call("SOSAE_SKSE", "SetSchlongBend",
                            static_cast<RE::Actor*>(player), bend);
                        if (!ok) {
                            CancelErectionAnimation();
                            appliedBend.store(-1);
                            Record("Gradual erection unavailable; using the normal SOS position method", true);
                            ApplyRequestedBend(true, true, false);
                            return;
                        }
                        sosConnected.store(true);
                        appliedBend.store(bend);
                        lastBendMethod.store(0);
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
            Record("SOS bend API did not accept the position update", true);
        }
    }
}

void ConfirmSoftState() {
    if (!stateKnown.load() || usingCBPC.load()) return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    auto* actor = static_cast<RE::Actor*>(player);
    const bool cbpcOK = SetCBPCPhysics(actor, false);
    const bool smpOK = Call("DynamicHDT", "TogglePhysics", actor, PhysicsBones(), true);
    cbpcConnected.store(cbpcOK);
    smpConnected.store(smpOK);
    if (!cbpcOK || !smpOK) {
        Record(fmt::format("Soft-state confirmation failed (FSMP: {}, CBPC: {})",
            smpOK ? "OK" : "no response", cbpcOK ? "OK" : "no response"), true);
        return;
    }
    ApplyBend(actor, 0, true, true, false);
    Record("Soft state confirmed after physics handoff");
}

bool SetOwner(bool cbpc, bool force = false) {
    const auto now = NowMs();
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
        Record(fmt::format("Physics handoff to {} failed (FSMP: {}, CBPC: {}); retry queued",
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
            ApplyBend(actor, 0, true, true, false);
        }
    }
    Record(fmt::format("Physics switched to {} ({})", cbpc ? "CBPC" : "SMP", cbpc ? "erect" : "soft"));
    return true;
}

bool SexLabHasPriority(const Settings& copy) {
    if (!copy.sexLabOverride || !sexLabValid.load()) return false;
    if (sexLabActive.load()) return true;
    return sexLabEndedMs.load() > 0 && NowMs() - sexLabEndedMs.load() < copy.sceneEndDelayMs;
}

void Evaluate(bool force) {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.enabled) {
        if (force && stateKnown.load()) SetOwner(false, true);
        return;
    }

    bool cbpc = usingCBPC.load();
    if (SexLabHasPriority(copy)) {
        cbpc = true;
    } else if (copy.mode == 1) {
        cbpc = false;
    } else if (copy.mode == 2) {
        cbpc = true;
    } else if (!arousalValid.load()) {
        if (!stateKnown.load()) cbpc = false;
    } else if (usingCBPC.load()) {
        cbpc = arousal.load() > copy.threshold - copy.hysteresis;
    } else {
        cbpc = arousal.load() >= copy.threshold;
    }
    SetOwner(cbpc, force);
}

void Tick() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    if (!copy.enabled) return;
    if (copy.mode == 0) QueryArousal();
    if (copy.sexLabOverride) QuerySexLab();
    Evaluate();

    const auto now = NowMs();
    auto softDue = softConfirmationDueMs.load();
    if (!usingCBPC.load() && softDue > 0 && now >= softDue &&
        softConfirmationDueMs.compare_exchange_strong(softDue, 0)) {
        ConfirmSoftState();
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
        // complete physics hand-off. Reapply the position once and keep the
        // already-confirmed SMP/CBPC owner unchanged.
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
    result.oslModuleLoaded = ::GetModuleHandleW(L"OSLAroused.dll") != nullptr;
    result.fsmpModuleLoaded = ::GetModuleHandleW(L"hdtsmp64.dll") != nullptr;
    result.cbpcModuleLoaded = ::GetModuleHandleW(L"cbp.dll") != nullptr;
    result.sexLabModuleLoaded = ::GetModuleHandleW(L"SexLabUtil.dll") != nullptr;
    result.sexLabPluginLoaded = PluginLoaded({ "SexLab.esm" });
    result.oslPluginLoaded = PluginLoaded({ "OSLAroused.esp", "OAroused.esp", "SexLabAroused.esm" });
    result.supportedAddonLoaded = PluginLoaded({
        "UBE_SOS_Addon.esp", "UBE_AllRace.esp", "3BBB UBE patch.esp",
        "SOS - Dw3BA - Futanari Addon.esp"
    });
    result.sosPluginLoaded = PluginLoaded({ "Schlongs of Skyrim.esp" });
    result.sosScriptPresent = fs::exists("Data/Scripts/SOSAE_SKSE.pex");

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
    value.sexLabOverride = true;
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
        d.compatibleXmlFiles > 0 && d.compatibleCbpcMaps > 0 && d.compatibleCbpcParameters > 0;

    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "STATUS");
    StatusLine("Overall", !healthChecked ? "Checking..." : (coreReady ? (stateKnown.load() ? "Working" : "Waiting for game") : "Needs attention"), !healthChecked || !stateKnown.load() ? 1 : (coreReady ? 2 : 0));
    const auto arousalText = arousalValid.load() ? fmt::format("Arousal: {:.0f} / 100", arousal.load()) : "Arousal: waiting for OSL";
    ImGuiMCP::ProgressBar(arousalValid.load() ? arousal.load() / 100.0F : 0.0F, ImGuiMCP::ImVec2(-1.0F, 0.0F), arousalText.c_str());
    StatusLine("Current physics", stateKnown.load() ? (usingCBPC.load() ? "CBPC (erect)" : "SMP (soft)") : "Waiting", stateKnown.load() ? 2 : 1);

    ImGuiMCP::Separator();
    changed |= ImGuiMCP::Checkbox("Enable physics switching", &copy.enabled);
    const char* modes[]{ "Automatic (recommended)", "Always soft (SMP)", "Always erect (CBPC)" };
    changed |= ImGuiMCP::Combo("Physics mode", &copy.mode, modes, 3);

    ImGuiMCP::BeginDisabled(copy.mode != 0);
    changed |= ImGuiMCP::SliderFloat("Become erect at arousal", &copy.threshold, 0, 100, "%.0f");
    ImGuiMCP::EndDisabled();
    ImGuiMCP::TextWrapped("Automatic mode uses SMP below %.0f and CBPC at %.0f or above.", copy.threshold, copy.threshold);

    ImGuiMCP::BeginDisabled(!copy.positionControl);
    changed |= ImGuiMCP::SliderInt("Erect upward position", &copy.erectBend, 0, 20);
    ImGuiMCP::EndDisabled();
    changed |= ImGuiMCP::Checkbox("Gradually become erect", &copy.gradualErection);
    ImGuiMCP::BeginDisabled(!copy.gradualErection);
    float erectionSeconds = copy.erectionDurationMs / 1000.0F;
    if (ImGuiMCP::SliderFloat("Time to become fully erect", &erectionSeconds, 0.5F, 10.0F, "%.1f seconds")) {
        copy.erectionDurationMs = static_cast<int>(std::lround(erectionSeconds * 1000.0F));
        changed = true;
    }
    ImGuiMCP::EndDisabled();
    ImGuiMCP::TextWrapped("Automatic mode smoothly raises the SOS AE position after CBPC takes control.");
    ImGuiMCP::BeginDisabled(!copy.positionControl || !stateKnown.load() || !usingCBPC.load());
    if (ImGuiMCP::Button("Test position now")) {
        ResetPositionRecovery();
        appliedBend.store(-1);
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { ApplyRequestedBend(true, true, false); });
    }
    ImGuiMCP::EndDisabled();

    ImGuiMCP::Separator();
    if (ImGuiMCP::Button("Refresh now"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { QueryArousal(); QuerySexLab(); Evaluate(); });
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Use recommended settings")) { UseRecommendedSettings(copy); changed = true; }
    if (changed) SaveSettingsAndApply(copy, previous);
}

void __stdcall RenderAdvanced() {
    Settings copy;
    { std::scoped_lock lock(settingsLock); copy = settings; }
    const Settings previous = copy;
    bool changed = false;

    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "ADVANCED PHYSICS");
    ImGuiMCP::TextWrapped("The recommended defaults work for most setups. Change these only when troubleshooting or tuning.");
    changed |= ImGuiMCP::SliderFloat("Switch buffer", &copy.hysteresis, 0, 25, "%.0f");
    ImGuiMCP::TextWrapped("Returns to soft below %.0f arousal to prevent rapid switching.", std::max(0.0F, copy.threshold - copy.hysteresis));
    changed |= ImGuiMCP::SliderInt("Arousal check interval (ms)", &copy.pollMs, 250, 5000);
    changed |= ImGuiMCP::SliderInt("Switch cooldown (ms)", &copy.switchCooldownMs, 0, 5000);

    ImGuiMCP::Separator();
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "SEXLAB P+");
    changed |= ImGuiMCP::Checkbox("Keep erect during player scenes", &copy.sexLabOverride);
    ImGuiMCP::BeginDisabled(!copy.sexLabOverride);
    changed |= ImGuiMCP::SliderInt("Return to normal after scene (ms)", &copy.sceneEndDelayMs, 0, 10000);
    ImGuiMCP::EndDisabled();
    const bool sexLabPositionChanged = ImGuiMCP::Checkbox("Use a different position during SexLab", &copy.useSexLabBend);
    changed |= sexLabPositionChanged;
    if (sexLabPositionChanged && copy.useSexLabBend) copy.sexLabOverride = true;
    ImGuiMCP::BeginDisabled(!copy.useSexLabBend || !copy.positionControl);
    changed |= ImGuiMCP::SliderInt("SexLab erect position", &copy.sexLabBend, 0, 20);
    ImGuiMCP::EndDisabled();

    ImGuiMCP::Separator();
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "POSITION BEHAVIOR");
    changed |= ImGuiMCP::Checkbox("Let this mod control the erect position", &copy.positionControl);
    ImGuiMCP::TextWrapped("Turn this off if another mod should control the angle. Physics switching will continue.");
    ImGuiMCP::BeginDisabled(!copy.positionControl);
    const char* bendMethods[]{ "SOS AE native only", "SOS animation event only", "Compatibility (recommended)" };
    changed |= ImGuiMCP::Combo("Position method", &copy.bendMethod, bendMethods, 3);
    changed |= ImGuiMCP::Checkbox("Animate real position changes", &copy.animatePosition);
    changed |= ImGuiMCP::Checkbox("Prevent repeated bouncing", &copy.bounceGuard);
    changed |= ImGuiMCP::SliderInt("Wait for CBPC to settle (ms)", &copy.settleDelayMs, 0, 5000);
    changed |= ImGuiMCP::SliderInt("Stop recovery after failures", &copy.maxBendFailures, 1, 10);
    ImGuiMCP::EndDisabled();
    ImGuiMCP::Text("Last method used: %s", BendMethodName(lastBendMethod.load()));

    if (positionAutoSuspended.load() && ImGuiMCP::Button("Resume position recovery")) {
        ResetPositionRecovery();
        appliedBend.store(-1);
    }
    ImGuiMCP::Separator();
    if (ImGuiMCP::Button("Apply recommended advanced settings")) { UseRecommendedSettings(copy); changed = true; }
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Restore defaults")) { copy = Settings{}; changed = true; }

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
    return fmt::format(
        "Schlong Physics Swapper {} diagnostics\n"
        "Engine={} StateKnown={} Arousal={:.1f} OSLConnected={} SexLabActive={} SexLabConnected={}\n"
        "MenuFramework={} OSL={} FSMP={} CBPC={} SexLabPPlus={} SOSAE={} SupportedAddon={}\n"
        "PlayerBones={}/6 XML={}/{} [{}]\nCBPCMap={}/{} [{}]\nCBPCParameters={}/{} [{}]\n"
        "SwitchSuccesses={} SwitchFailures={} BendApplies={} LastAction={} LastError={}\n"
        "Position: Enabled={} Requested={} Applied={} LastMethod={} LastSucceeded={} AutoSuspended={} GuardRemainingMs={}\n"
        "Settings: Enabled={} Mode={} Threshold={:.0f} Hysteresis={:.0f} Bend={} PollMs={} SexLabOverride={} EndDelayMs={} CooldownMs={} BendMethod={} Animate={} Gradual={} ErectionMs={} BounceGuard={} SettleMs={} SeparateSexLabBend={} SexLabBend={} MaxFailures={} PPA={}\n",
        kVersion, stateKnown.load() ? (usingCBPC.load() ? "CBPC" : "SMP") : "unknown", stateKnown.load(), arousal.load(), oslConnected.load(), sexLabActive.load(), sexLabConnected.load(),
        d.menuFrameworkLoaded, d.oslModuleLoaded && d.oslPluginLoaded, d.fsmpModuleLoaded, d.cbpcModuleLoaded, d.sexLabModuleLoaded && d.sexLabPluginLoaded, d.sosScriptPresent || sosConnected.load(), d.supportedAddonLoaded,
        d.playerBonesFound, d.compatibleXmlFiles, d.xmlFiles, d.xmlSummary, d.compatibleCbpcMaps, d.cbpcMapFiles, d.cbpcMapSummary,
        d.compatibleCbpcParameters, d.cbpcParameterFiles, d.cbpcParameterSummary, switchSuccesses.load(), switchFailures.load(), bendRepairs.load(), recent, error.empty() ? "none" : error,
        s.positionControl, requestedBend.load(), appliedBend.load(), BendMethodName(lastBendMethod.load()), lastBendSucceeded.load(), positionAutoSuspended.load(), std::max<std::int64_t>(0, bendGuardUntilMs.load() - NowMs()),
        s.enabled, s.mode, s.threshold, s.hysteresis, s.erectBend, s.pollMs, s.sexLabOverride, s.sceneEndDelayMs, s.switchCooldownMs,
        s.bendMethod, s.animatePosition, s.gradualErection, s.erectionDurationMs, s.bounceGuard, s.settleDelayMs, s.useSexLabBend, s.sexLabBend, s.maxBendFailures,
        ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr);
}

void __stdcall RenderDebug() {
    Diagnostics d;
    { std::scoped_lock lock(diagnosticsLock); d = diagnostics; }

    const bool coreReady = d.menuFrameworkLoaded && d.oslModuleLoaded && d.oslPluginLoaded &&
        d.fsmpModuleLoaded && d.cbpcModuleLoaded && d.playerBonesFound == 6 &&
        d.compatibleXmlFiles > 0 && d.compatibleCbpcMaps > 0 && d.compatibleCbpcParameters > 0;
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "TROUBLESHOOTING");
    StatusLine("Overall health", d.checkedAtMs == 0 ? "Not checked yet" : (coreReady ? "Everything required was found" : "One or more items need attention"), d.checkedAtMs == 0 ? 1 : (coreReady ? 2 : 0));
    if (ImGuiMCP::Button("Run health check"))
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask(RefreshDiagnostics);
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Fix recommended settings")) {
        Settings fixed;
        { std::scoped_lock lock(settingsLock); fixed = settings; }
        const Settings previous = fixed;
        UseRecommendedSettings(fixed);
        SaveSettingsAndApply(fixed, previous);
    }
    ImGuiMCP::TextWrapped("Red items usually indicate a missing dependency or configuration. The settings button repairs this mod's safe defaults but does not install missing mods.");
    ImGuiMCP::Separator();

    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "DEPENDENCIES AND CONNECTIONS");
    StatusLine("SKSE Menu Framework", d.menuFrameworkLoaded ? "Loaded" : "Missing", d.menuFrameworkLoaded ? 2 : 0);
    StatusLine("OSL Aroused", d.oslModuleLoaded && d.oslPluginLoaded ? (oslConnected.load() ? "Connected and running" : "Loaded; waiting for response") : "Missing", d.oslModuleLoaded && d.oslPluginLoaded ? (oslConnected.load() ? 2 : 1) : 0);
    StatusLine("Faster HDT-SMP", d.fsmpModuleLoaded ? (smpConnected.load() ? "Connected and running" : "Loaded; not tested yet") : "Missing", d.fsmpModuleLoaded ? (smpConnected.load() ? 2 : 1) : 0);
    StatusLine("CBPC", d.cbpcModuleLoaded ? (cbpcConnected.load() ? "Connected and running" : "Loaded; not tested yet") : "Missing", d.cbpcModuleLoaded ? (cbpcConnected.load() ? 2 : 1) : 0);
    StatusLine("SOS AE bend API", d.sosScriptPresent || sosConnected.load() ? (sosConnected.load() ? "Connected and running" : "Installed; not tested yet") : (d.sosPluginLoaded ? "Legacy fallback only" : "Missing"), sosConnected.load() ? 2 : ((d.sosScriptPresent || d.sosPluginLoaded) ? 1 : 0));
    StatusLine("Six-bone schlong addon", d.supportedAddonLoaded ? fmt::format("Loaded; {}/6 live bones", d.playerBonesFound).c_str() : fmt::format("Plugin not identified; {}/6 live bones", d.playerBonesFound).c_str(), d.playerBonesFound == 6 ? 2 : 1);
    StatusLine("SexLab P+", d.sexLabModuleLoaded && d.sexLabPluginLoaded ? (sexLabConnected.load() ? "Connected and running" : "Loaded; waiting for response") : "Not installed", d.sexLabModuleLoaded && d.sexLabPluginLoaded ? (sexLabConnected.load() ? 2 : 1) : 1);
    StatusLine("Player SexLab scene", sexLabValid.load() ? (sexLabActive.load() ? "Active" : "Not active") : "Unknown", sexLabValid.load() ? 2 : 1);
    const bool ppaLoaded = ::GetModuleHandleW(L"AccuratePenetration.dll") != nullptr;
    StatusLine("Procedural Penis Animations", ppaLoaded ? (PPAOwnsPosition() ? "Controlling scene position" : "Loaded; compatible hand-off ready") : "Not installed", ppaLoaded ? 2 : 1);

    ImGuiMCP::Separator();
    ImGuiMCP::TextColored(ImGuiMCP::ImVec4(0.45F, 0.80F, 1.0F, 1.0F), "CONFIGURATION HEALTH");
    StatusLine("SMP genital XML", d.compatibleXmlFiles > 0 ? fmt::format("Valid ({})", d.compatibleXmlFiles).c_str() : "No valid Gen01-Gen06 XML", d.compatibleXmlFiles > 0 ? 2 : 0);
    ImGuiMCP::TextWrapped("Found: %s", d.xmlSummary.c_str());
    StatusLine("CBPC bone map", d.compatibleCbpcMaps > 0 ? "Valid Gen01-Gen06 map" : "Missing Gen01-Gen06 map", d.compatibleCbpcMaps > 0 ? 2 : 0);
    ImGuiMCP::TextWrapped("Found: %s", d.cbpcMapSummary.c_str());
    StatusLine("CBPC physics values", d.compatibleCbpcParameters > 0 ? "Valid bundled values" : "Missing bundled physics values", d.compatibleCbpcParameters > 0 ? 2 : 0);
    ImGuiMCP::TextWrapped("Found: %s", d.cbpcParameterSummary.c_str());
    if (d.compatibleXmlFiles > 1)
        ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.78F, 0.25F, 1.0F), "Note: multiple compatible XML files are visible. The mesh path decides which one FSMP uses.");

    ImGuiMCP::Separator();
    ImGuiMCP::Text("Successful handoffs: %u", switchSuccesses.load());
    ImGuiMCP::Text("Failed handoffs: %u", switchFailures.load());
    ImGuiMCP::Text("Erect position applications: %u", bendRepairs.load());
    ImGuiMCP::Text("Requested / applied position: %d / %d", requestedBend.load(), appliedBend.load());
    ImGuiMCP::Text("Last position method: %s", BendMethodName(lastBendMethod.load()));
    StatusLine("Gradual erection", erectionAnimating.load() ? fmt::format("Animating: {}/{}", appliedBend.load(), erectionAnimationTargetBend.load()).c_str() : "Idle", erectionAnimating.load() ? 1 : 2);
    StatusLine("Position recovery", positionAutoSuspended.load() ? "Stopped after failures" : (NowMs() < bendGuardUntilMs.load() ? "Bounce guard pause" : "Ready"), positionAutoSuspended.load() ? 0 : (NowMs() < bendGuardUntilMs.load() ? 1 : 2));
    {
        std::scoped_lock lock(activityLock);
        ImGuiMCP::TextWrapped("Last action: %s", lastAction.c_str());
        if (!lastError.empty()) ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0F, 0.45F, 0.35F, 1.0F), "Last problem: %s", lastError.c_str());
        if (ImGuiMCP::CollapsingHeader("Recent events"))
            for (const auto& entry : activity) ImGuiMCP::TextWrapped("- %s", entry.c_str());
    }

    if (ImGuiMCP::Button("Copy diagnostic report")) {
        const auto report = BuildReport();
        ImGuiMCP::SetClipboardText(report.c_str());
    }
    ImGuiMCP::SameLine();
    if (ImGuiMCP::Button("Save report to file")) {
        fs::create_directories(fs::path(kReport).parent_path());
        std::ofstream(kReport) << BuildReport();
        Record(fmt::format("Diagnostic report saved to {}", kReport));
    }
}

void RegisterMenu() {
    if (!SKSEMenuFramework::IsInstalled()) { Record("SKSE Menu Framework not found", true); return; }
    SKSEMenuFramework::SetSection(kName);
    SKSEMenuFramework::AddSectionItem("Main settings", RenderMain);
    SKSEMenuFramework::AddSectionItem("Advanced", RenderAdvanced);
    SKSEMenuFramework::AddSectionItem("Troubleshooting", RenderDebug);
}

class ModEventSink final : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event, RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
        if (!event) return RE::BSEventNotifyControl::kContinue;
        const std::string_view name = event->eventName.c_str();
        if (name == "HookAnimationStart" || name == "HookAnimationStarting" || name == "HookAnimationEnd" || name == "AnimationStart" || name == "AnimationEnd") {
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([] { QuerySexLab(); });
        } else if (name == "OSLA_ActorArousalUpdated" && event->sender == RE::PlayerCharacter::GetSingleton()) {
            // The callback query will evaluate the new value. Arousal updates
            // must not invalidate an already-applied bend or they create a
            // position replay loop while Automatic mode is erect.
        } else if (name == "SexLabDisabled") {
            sexLabActive.store(false);
            sexLabValid.store(false);
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
    if (message->type == SKSE::MessagingInterface::kDataLoaded) {
        Load();
        RegisterMenu();
        if (auto* source = SKSE::GetModCallbackEventSource()) source->AddEventSink(&modEventSink);
        if (auto* source = SKSE::GetNiNodeUpdateEventSource()) source->AddEventSink(&niNodeSink);
        RefreshDiagnostics();
        return;
    }
    if (message->type == SKSE::MessagingInterface::kPostLoadGame ||
        message->type == SKSE::MessagingInterface::kNewGame) {
        StartPolling();
        arousalValid.store(false);
        sexLabValid.store(false);
        stateKnown.store(false);
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

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
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
