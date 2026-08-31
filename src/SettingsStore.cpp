#include "SettingsStore.h"

#include <SimpleIni.h>

#include <algorithm>

namespace SPS::Core {

namespace {

Settings ReadSettings(CSimpleIniA& ini)
{
    Settings settings;
    settings.enabled = ini.GetBoolValue("General", "Enabled", true);
    settings.threshold = std::clamp(static_cast<float>(ini.GetDoubleValue("General", "ArousalThreshold", 60)), 0.0F, 100.0F);
    settings.hysteresis = std::clamp(static_cast<float>(ini.GetDoubleValue("General", "Hysteresis", 5)), 0.0F, 25.0F);
    settings.mode = std::clamp(static_cast<int>(ini.GetLongValue("General", "Mode", 0)), 0, 2);
    settings.erectBend = std::clamp(static_cast<int>(ini.GetLongValue("General", "ErectBend", 14)), 0, 20);
    settings.flaccidAngleControl = ini.GetBoolValue("Position", "FlaccidAngleControl", false);
    settings.flaccidBend = std::clamp(static_cast<int>(ini.GetLongValue("Position", "FlaccidBend", 0)), 0, 20);
    settings.pollMs = std::clamp(static_cast<int>(ini.GetLongValue("General", "PollMilliseconds", 1000)), 250, 10000);
    settings.sexLabOverride = ini.GetBoolValue("Compatibility", "SexLabPPlusOverride", true);
    settings.sexLabRoleSwitching = ini.GetBoolValue("Compatibility", "SexLabRoleSwitching", true);
    settings.sexLabBottomBehavior = std::clamp(static_cast<int>(ini.GetLongValue("Compatibility", "SexLabBottomBehavior", 0)), 0, 3);
    settings.sexLabUnknownRole = std::clamp(static_cast<int>(ini.GetLongValue("Compatibility", "SexLabUnknownRole", 0)), 0, 2);
    settings.sceneEndDelayMs = std::clamp(static_cast<int>(ini.GetLongValue("Compatibility", "SexLabEndDelayMilliseconds", 1500)), 0, 10000);
    settings.ostimOverride = ini.GetBoolValue("Compatibility", "OStimOverride", true);
    settings.ostimRoleSwitching = ini.GetBoolValue("Compatibility", "OStimRoleSwitching", true);
    settings.switchCooldownMs = std::clamp(static_cast<int>(ini.GetLongValue("Reliability", "SwitchCooldownMilliseconds", 750)), 0, 5000);
    settings.resetSMPAfterLoad = ini.GetBoolValue("Reliability", "ResetSMPAfterLoad", true);
    settings.loadResetDelayMs = std::clamp(static_cast<int>(ini.GetLongValue("Reliability", "SMPResetDelayMilliseconds", 10000)), 1000, 60000);
    settings.positionControl = ini.GetBoolValue("Position", "Enabled", true);
    settings.bendMethod = std::clamp(static_cast<int>(ini.GetLongValue("Position", "Method", 2)), 0, 2);
    settings.animatePosition = ini.GetBoolValue("Position", "AnimateChanges", true);
    settings.gradualErection = ini.GetBoolValue("Position", "GradualErection", true);
    settings.arousalBasedErection = ini.GetBoolValue("NaturalBehaviour", "ArousalBasedErection", false);
    settings.erectionStartArousal = std::clamp(static_cast<float>(ini.GetDoubleValue("NaturalBehaviour", "ErectionStartArousal", 20)), 0.0F, 99.0F);
    settings.erectionDurationMs = std::clamp(static_cast<int>(ini.GetLongValue("Position", "ErectionDurationMilliseconds", 3000)), 500, 10000);
    settings.softeningDurationMs = std::clamp(static_cast<int>(ini.GetLongValue("Position", "SofteningDurationMilliseconds", 5000)), 500, 15000);
    settings.randomErections = ini.GetBoolValue("NaturalBehaviour", "RandomErections", false);
    settings.randomErectionMinMinutes = std::clamp(static_cast<int>(ini.GetLongValue("NaturalBehaviour", "RandomMinimumMinutes", 15)), 1, 180);
    settings.randomErectionMaxMinutes = std::clamp(static_cast<int>(ini.GetLongValue("NaturalBehaviour", "RandomMaximumMinutes", 45)), settings.randomErectionMinMinutes, 360);
    settings.randomErectionDurationSeconds = std::clamp(static_cast<int>(ini.GetLongValue("NaturalBehaviour", "RandomDurationSeconds", 60)), 5, 600);
    settings.randomErectionSafeMoments = ini.GetBoolValue("NaturalBehaviour", "RandomSafeMomentsOnly", true);
    settings.spontaneousRefractory = ini.GetBoolValue("NaturalBehaviour", "SpontaneousRefractory", true);
    settings.refractoryMinutes = std::clamp(static_cast<int>(ini.GetLongValue("NaturalBehaviour", "RefractoryMinutes", 5)), 1, 60);
    settings.morningErections = ini.GetBoolValue("NaturalBehaviour", "MorningErections", false);
    settings.morningErectionDurationSeconds = std::clamp(static_cast<int>(ini.GetLongValue("NaturalBehaviour", "MorningDurationSeconds", 90)), 10, 600);
    settings.equipmentChangeRecovery = ini.GetBoolValue("Reliability", "RepairAfterEquipmentChange", true);
    settings.bounceGuard = ini.GetBoolValue("Position", "BounceGuard", true);
    settings.useSexLabBend = ini.GetBoolValue("Position", "UseSeparateSexLabBend", false);
    settings.sexLabBend = std::clamp(static_cast<int>(ini.GetLongValue("Position", "SexLabBend", 14)), 0, 20);
    settings.settleDelayMs = std::clamp(static_cast<int>(ini.GetLongValue("Position", "SettleDelayMilliseconds", 350)), 0, 5000);
    settings.maxBendFailures = std::clamp(static_cast<int>(ini.GetLongValue("Position", "MaxAutomaticFailures", 3)), 1, 10);
    settings.verboseLogging = ini.GetBoolValue("Debug", "VerboseLogging", false);
    return settings;
}

}

SettingsLoadResult LoadSettings(
    const std::filesystem::path& currentPath,
    const std::filesystem::path& legacyPath)
{
    const bool legacyExists = std::filesystem::exists(legacyPath);
    const bool currentExists = std::filesystem::exists(currentPath);
    bool preferLegacy = false;
    if (currentExists) {
        CSimpleIniA probe;
        probe.SetUnicode();
        probe.LoadFile(currentPath.string().c_str());
        preferLegacy = probe.GetBoolValue("Migration", "PreferLegacyIfPresent", false);
    }

    SettingsLoadResult result;
    result.migratedLegacy = legacyExists && (!currentExists || preferLegacy);
    result.shouldWriteCurrent = result.migratedLegacy || !currentExists;

    CSimpleIniA ini;
    ini.SetUnicode();
    const auto& selectedPath = result.migratedLegacy ? legacyPath : currentPath;
    ini.LoadFile(selectedPath.string().c_str());
    result.settings = ReadSettings(ini);
    return result;
}

bool SaveSettings(const std::filesystem::path& path, const Settings& settings)
{
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
    ini.SetBoolValue("Compatibility", "OStimOverride", settings.ostimOverride);
    ini.SetBoolValue("Compatibility", "OStimRoleSwitching", settings.ostimRoleSwitching);
    ini.SetLongValue("Reliability", "SwitchCooldownMilliseconds", settings.switchCooldownMs);
    ini.SetBoolValue("Reliability", "ResetSMPAfterLoad", settings.resetSMPAfterLoad);
    ini.SetLongValue("Reliability", "SMPResetDelayMilliseconds", settings.loadResetDelayMs);
    ini.SetBoolValue("Position", "Enabled", settings.positionControl);
    ini.SetBoolValue("Position", "FlaccidAngleControl", settings.flaccidAngleControl);
    ini.SetLongValue("Position", "FlaccidBend", settings.flaccidBend);
    ini.SetLongValue("Position", "Method", settings.bendMethod);
    ini.SetBoolValue("Position", "AnimateChanges", settings.animatePosition);
    ini.SetBoolValue("Position", "GradualErection", settings.gradualErection);
    ini.SetBoolValue("NaturalBehaviour", "ArousalBasedErection", settings.arousalBasedErection);
    ini.SetDoubleValue("NaturalBehaviour", "ErectionStartArousal", settings.erectionStartArousal);
    ini.SetLongValue("Position", "ErectionDurationMilliseconds", settings.erectionDurationMs);
    ini.SetLongValue("Position", "SofteningDurationMilliseconds", settings.softeningDurationMs);
    ini.SetBoolValue("NaturalBehaviour", "RandomErections", settings.randomErections);
    ini.SetLongValue("NaturalBehaviour", "RandomMinimumMinutes", settings.randomErectionMinMinutes);
    ini.SetLongValue("NaturalBehaviour", "RandomMaximumMinutes", settings.randomErectionMaxMinutes);
    ini.SetLongValue("NaturalBehaviour", "RandomDurationSeconds", settings.randomErectionDurationSeconds);
    ini.SetBoolValue("NaturalBehaviour", "RandomSafeMomentsOnly", settings.randomErectionSafeMoments);
    ini.SetBoolValue("NaturalBehaviour", "SpontaneousRefractory", settings.spontaneousRefractory);
    ini.SetLongValue("NaturalBehaviour", "RefractoryMinutes", settings.refractoryMinutes);
    ini.SetBoolValue("NaturalBehaviour", "MorningErections", settings.morningErections);
    ini.SetLongValue("NaturalBehaviour", "MorningDurationSeconds", settings.morningErectionDurationSeconds);
    ini.SetBoolValue("Reliability", "RepairAfterEquipmentChange", settings.equipmentChangeRecovery);
    ini.SetBoolValue("Position", "BounceGuard", settings.bounceGuard);
    ini.SetBoolValue("Position", "UseSeparateSexLabBend", settings.useSexLabBend);
    ini.SetLongValue("Position", "SexLabBend", settings.sexLabBend);
    ini.SetLongValue("Position", "SettleDelayMilliseconds", settings.settleDelayMs);
    ini.SetLongValue("Position", "MaxAutomaticFailures", settings.maxBendFailures);
    ini.SetBoolValue("Debug", "VerboseLogging", settings.verboseLogging);

    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    return ini.SaveFile(path.string().c_str()) >= SI_OK;
}

}
