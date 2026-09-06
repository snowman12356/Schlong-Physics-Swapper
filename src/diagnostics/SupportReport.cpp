#include "SupportReport.h"

#include <fmt/format.h>

#include <algorithm>

namespace SPS::Diagnostics {

std::string FormatSupportReport(const SupportReportInput& input)
{
    const auto& d = input.diagnostics;
    const auto& ownership = input.ownership;
    const auto& position = input.position;
    const auto& recovery = input.recovery;
    const auto& s = input.settings;
    return fmt::format(
        "Schlong Physics Swapper {} diagnostics\n"
        "SkyrimRuntime={} SKSE={}\n"
        "DLLs: MenuFramework={} OSL={} SLO={} FSMP={} CBPC={} SexLab={} OStim={} SOS={}\n"
        "Compatibility: TNG={} PositionBackend={} ClassicSexLabAroused={} SexLabRoleBridge={} OStimRoleBridge={} SOFTBODY={} PPA={} PhysicsEditor={} SOSPhysicsManager={} AutoPhysicsReset={} CrashLogger={}\n"
        "OwnershipEvidence=completed-commands LiveMotionVerified=false (engine read-back unavailable)\n"
        "Engine={} StateKnown={} HandoffPending={} PendingTarget={} Arousal={:.1f} Provider={} ProviderConnected={} SexLabActive={} SexLabConnected={} SexLabRole={} SexLabRoleValid={} OStimActive={} OStimConnected={} OStimRole={} OStimRoleValid={}\n"
        "CompatibilityAPI=V{} ActiveRequests={} ActiveRequester={} Accepted={} Released={} ResetNotices={} OwnerRepairs={} LastOwnerRepairMs={}\n"
        "MenuFramework={} ArousalProvider={} FSMP={} FSMPActorAPI={} FSMPBridge={} CBPC={} SexLabPPlus={} PositionBackendReady={} SupportedAddon={}\n"
        "PlayerBones={}/6 XML={}/{} [{}]\nCBPCMap={}/{} [{}]\nCBPCParameters={}/{} [{}]\n"
        "SwitchSuccesses={} SwitchFailures={} BendApplies={} LoadSMPResets={} LastLoadSMPResetMs={} LastAction={} LastError={}\n"
        "Position: Enabled={} Requested={} Applied={} LastMethod={} LastSucceeded={} AutoSuspended={} GuardRemainingMs={}\n"
        "Settings: Enabled={} Mode={} Threshold={:.0f} Hysteresis={:.0f} ErectBend={} SoftAngle={} SoftBend={} PollMs={} SexLabOverride={} SexLabRoleSwitching={} OStimOverride={} OStimRoleSwitching={} BottomBehavior={} UnknownRole={} EndDelayMs={} CooldownMs={} ResetSMPAfterLoad={} LoadResetDelayMs={} EquipmentRecovery={} BendMethod={} Animate={} Gradual={} ArousalBased={} ErectionStart={:.0f} ErectionMs={} SofteningMs={} RandomErections={} RandomMinMinutes={} RandomMaxMinutes={} RandomDurationSeconds={} RandomSafeMoments={} Refractory={} RefractoryMinutes={} MorningErections={} MorningDurationSeconds={} BounceGuard={} SettleMs={} SeparateSexLabBend={} SexLabBend={} MaxFailures={} PPA={} VerboseLogging={}\n"
        "\nSuggested fixes\n{}",
        input.version, input.runtimeVersion, input.skseVersion,
        input.components.menuFramework, input.components.osl, input.components.slo,
        input.components.fsmp, input.components.cbpc, input.components.sexLab,
        input.components.ostim, input.components.sos,
        d.tngPluginLoaded, input.positionBackend, d.classicArousedPluginLoaded,
        d.sexLabRoleBridgePresent, d.ostimRoleBridgePresent, d.softbodyLoaded, input.ppaLoaded,
        d.physicsEditorLoaded, d.sosPhysicsManagerLoaded, d.autoPhysicsResetLoaded,
        d.crashLoggerLoaded,
        ownership.known ? (ownership.usingCBPC ? "CBPC" : "SMP") : "unknown",
        ownership.known, ownership.pending,
        ownership.pending ? (ownership.pendingCBPC ? "CBPC" : "SMP") : "none",
        input.arousal, input.arousalProvider, input.arousalConnected,
        input.scenes.sexLabActive, input.scenes.sexLabConnected,
        input.scenes.sexLabRole, input.scenes.sexLabRoleValid,
        input.scenes.ostimActive, input.scenes.ostimConnected,
        input.scenes.ostimRole, input.scenes.ostimRoleValid,
        input.apiVersion, input.apiStats.active, input.activeAPIRequester,
        input.apiStats.accepted, input.apiStats.released,
        input.externalResetNotices, input.ownerRestorations,
        recovery.lastOwnerRestorationMs,
        d.menuFrameworkLoaded, d.oslModuleLoaded && d.oslPluginLoaded,
        d.fsmpModuleLoaded, d.fsmpActorApiAvailable, d.fsmpBridgePresent,
        d.cbpcModuleLoaded, d.sexLabModuleLoaded && d.sexLabPluginLoaded,
        input.positionBackendReady, d.supportedAddonLoaded,
        d.playerBonesFound, d.compatibleXmlFiles, d.xmlFiles, d.xmlSummary,
        d.compatibleCbpcMaps, d.cbpcMapFiles, d.cbpcMapSummary,
        d.compatibleCbpcParameters, d.cbpcParameterFiles, d.cbpcParameterSummary,
        ownership.successes, ownership.failures, position.repairs,
        recovery.loadSmpResets, recovery.lastLoadSmpResetMs,
        input.activity.lastAction,
        input.activity.lastError.empty() ? "none" : input.activity.lastError,
        s.positionControl, position.requestedBend, position.appliedBend,
        input.lastBendMethod, position.lastSucceeded, position.automaticSuspended,
        std::max<std::int64_t>(0, position.guardUntilMs - input.nowMs),
        s.enabled, s.mode, s.threshold, s.hysteresis, s.erectBend,
        s.flaccidAngleControl, s.flaccidBend, s.pollMs, s.sexLabOverride,
        s.sexLabRoleSwitching, s.ostimOverride, s.ostimRoleSwitching,
        s.sexLabBottomBehavior, s.sexLabUnknownRole, s.sceneEndDelayMs,
        s.switchCooldownMs, s.resetSMPAfterLoad, s.loadResetDelayMs,
        s.equipmentChangeRecovery, s.bendMethod, s.animatePosition,
        s.gradualErection, s.arousalBasedErection, s.erectionStartArousal,
        s.erectionDurationMs, s.softeningDurationMs, s.randomErections,
        s.randomErectionMinMinutes, s.randomErectionMaxMinutes,
        s.randomErectionDurationSeconds, s.randomErectionSafeMoments,
        s.spontaneousRefractory, s.refractoryMinutes, s.morningErections,
        s.morningErectionDurationSeconds, s.bounceGuard, s.settleDelayMs,
        s.useSexLabBend, s.sexLabBend, s.maxBendFailures, input.ppaLoaded,
        s.verboseLogging, input.fixes);
}

}
