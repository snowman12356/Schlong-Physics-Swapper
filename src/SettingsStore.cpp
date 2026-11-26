#include "SettingsStore.h"

#include <SimpleIni.h>

#include <algorithm>

namespace SPS::Core {

namespace {

Settings ReadSettings(CSimpleIniA& ini)
{
    Settings settings;
    settings.enabled = ini.GetBoolValue("General", "Enabled", true);
    settings.basicNpcEnabled = ini.GetBoolValue("NPCs", "BasicSOSEnabled", false);
    settings.basicNpcProviderSosDisabled = ini.GetBoolValue("NPCs", "ProviderSOSDisabledConfirmed", false);
    settings.managedNpcEnabled = ini.GetBoolValue("NPCs", "ManagedNPCEnabled", false);
    settings.managedNpcUpToEight = ini.GetBoolValue("NPCs", "ManagedNPCUpToEight", false);
    settings.threshold = std::clamp(static_cast<float>(ini.GetDoubleValue("General", "ArousalThreshold", 60)), 0.0F, 100.0F);
    settings.hysteresis = std::clamp(static_cast<float>(ini.GetDoubleValue("General", "Hysteresis", 5)), 0.0F, 25.0F);
    settings.mode = std::clamp(static_cast<int>(ini.GetLongValue("General", "Mode", 0)), 0, 2);
    settings.erectBend = std::clamp(static_cast<int>(ini.GetLongValue("General", "ErectBend", 14)), 0, 20);
    settings.flaccidAngleControl = ini.GetBoolValue("Position", "FlaccidAngleControl", false);
    settings.probeSkipPostBakeFlaccid = ini.GetBoolValue("Debug", "ProbeSkipPostBakeFlaccid", true);
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
    settings.bendMethod = std::clamp(static_cast<int>(ini.GetLongValue("Position", "Method", 2)), 0, 3);
    settings.poseAngleDegrees = std::clamp(static_cast<float>(ini.GetDoubleValue("Position", "PoseAngleDegrees", 45)), 0.0F, 90.0F);
    settings.animatePosition = ini.GetBoolValue("Position", "AnimateChanges", true);
    settings.gradualErection = ini.GetBoolValue("Position", "GradualErection", true);
    settings.arousalBasedErection = ini.GetBoolValue("NaturalBehaviour", "ArousalBasedErection", false);
    settings.erectionStartArousal = std::clamp(static_cast<float>(ini.GetDoubleValue("NaturalBehaviour", "ErectionStartArousal", 20)), 0.0F, 99.0F);
    // 30 s allows a full 20-stage walk at a slow, visible pace. The old 10 s
    // ceiling made the smoothest setting people actually wanted impossible.
    settings.erectionDurationMs = std::clamp(static_cast<int>(ini.GetLongValue("Position", "ErectionDurationMilliseconds", 3000)), 500, 30000);
    settings.softeningDurationMs = std::clamp(static_cast<int>(ini.GetLongValue("Position", "SofteningDurationMilliseconds", 5000)), 500, 30000);
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
    // The marker is a one-shot: after this load the current file is written, and
    // it must not keep asking to import the legacy file again. Otherwise every
    // later launch would overwrite whatever the player has since configured.
    result.dropLegacyMarker = preferLegacy;
    return result;
}

// Every setting is written on save, so a file that has been through the menu
// still contains everything a user might want to edit by hand. Values are
// written with comments only where the key is new: SimpleIni keeps whatever the
// existing file says for a key it already knows about.
//
// The table below is the single list of section/key pairs. Recording them as
// they are applied means the shipped template can be checked against this
// build without a second hand-maintained list drifting out of date.
std::vector<std::string> recordedKeys;
std::once_flag recordedKeysOnce;

void RememberKey(const char* section, const char* key)
{
    recordedKeys.push_back(std::string(section) + "|" + key);
}

void ApplySettingsToIni(CSimpleIniA& ini, const Settings& settings)
{
    const auto setBool = [&ini](const char* section, const char* key, bool value, const char* comment = nullptr) {
        RememberKey(section, key);
        ini.SetBoolValue(section, key, value, comment);
    };
    const auto setLong = [&ini](const char* section, const char* key, long value, const char* comment = nullptr) {
        RememberKey(section, key);
        ini.SetLongValue(section, key, value, comment);
    };
    const auto setDouble = [&ini](const char* section, const char* key, double value, const char* comment = nullptr) {
        RememberKey(section, key);
        ini.SetDoubleValue(section, key, value, comment);
    };

    setBool("General", "Enabled", settings.enabled);
    setDouble("General", "ArousalThreshold", settings.threshold);
    setDouble("General", "Hysteresis", settings.hysteresis);
    setLong("General", "Mode", settings.mode);
    setLong("General", "ErectBend", settings.erectBend);
    setLong("General", "PollMilliseconds", settings.pollMs);
    setBool("NPCs", "BasicSOSEnabled", settings.basicNpcEnabled);
    setBool("NPCs", "ProviderSOSDisabledConfirmed", settings.basicNpcProviderSosDisabled);
    setBool("NPCs", "ManagedNPCEnabled", settings.managedNpcEnabled);
    setBool("NPCs", "ManagedNPCUpToEight", settings.managedNpcUpToEight);
    setBool("Compatibility", "SexLabPPlusOverride", settings.sexLabOverride);
    setBool("Compatibility", "SexLabRoleSwitching", settings.sexLabRoleSwitching);
    setLong("Compatibility", "SexLabBottomBehavior", settings.sexLabBottomBehavior);
    setLong("Compatibility", "SexLabUnknownRole", settings.sexLabUnknownRole);
    setLong("Compatibility", "SexLabEndDelayMilliseconds", settings.sceneEndDelayMs);
    setBool("Compatibility", "OStimOverride", settings.ostimOverride);
    setBool("Compatibility", "OStimRoleSwitching", settings.ostimRoleSwitching);
    setLong("Reliability", "SwitchCooldownMilliseconds", settings.switchCooldownMs);
    setBool("Reliability", "ResetSMPAfterLoad", settings.resetSMPAfterLoad);
    setLong("Reliability", "SMPResetDelayMilliseconds", settings.loadResetDelayMs);
    setBool("Reliability", "RepairAfterEquipmentChange", settings.equipmentChangeRecovery);
    setBool("Position", "Enabled", settings.positionControl);
    setBool("Position", "FlaccidAngleControl", settings.flaccidAngleControl);
    setLong("Position", "FlaccidBend", settings.flaccidBend);
    setLong("Position", "Method", settings.bendMethod);
    setDouble("Position", "PoseAngleDegrees", settings.poseAngleDegrees);
    setBool("Position", "AnimateChanges", settings.animatePosition);
    setBool("Position", "GradualErection", settings.gradualErection);
    setLong("Position", "ErectionDurationMilliseconds", settings.erectionDurationMs);
    setLong("Position", "SofteningDurationMilliseconds", settings.softeningDurationMs);
    setBool("Position", "BounceGuard", settings.bounceGuard);
    setBool("Position", "UseSeparateSexLabBend", settings.useSexLabBend);
    setLong("Position", "SexLabBend", settings.sexLabBend);
    setLong("Position", "SettleDelayMilliseconds", settings.settleDelayMs);
    setLong("Position", "MaxAutomaticFailures", settings.maxBendFailures);
    setBool("NaturalBehaviour", "ArousalBasedErection", settings.arousalBasedErection);
    setDouble("NaturalBehaviour", "ErectionStartArousal", settings.erectionStartArousal);
    setBool("NaturalBehaviour", "RandomErections", settings.randomErections);
    setLong("NaturalBehaviour", "RandomMinimumMinutes", settings.randomErectionMinMinutes);
    setLong("NaturalBehaviour", "RandomMaximumMinutes", settings.randomErectionMaxMinutes);
    setLong("NaturalBehaviour", "RandomDurationSeconds", settings.randomErectionDurationSeconds);
    setBool("NaturalBehaviour", "RandomSafeMomentsOnly", settings.randomErectionSafeMoments);
    setBool("NaturalBehaviour", "SpontaneousRefractory", settings.spontaneousRefractory);
    setLong("NaturalBehaviour", "RefractoryMinutes", settings.refractoryMinutes);
    setBool("NaturalBehaviour", "MorningErections", settings.morningErections);
    setLong("NaturalBehaviour", "MorningDurationSeconds", settings.morningErectionDurationSeconds);
    setBool("Debug", "VerboseLogging", settings.verboseLogging);
}

std::vector<std::string> SettingsKeyNames()
{
    std::call_once(recordedKeysOnce, [] {
        CSimpleIniA scratch;
        scratch.SetUnicode();
        ApplySettingsToIni(scratch, Settings{});
    });
    return recordedKeys;
}

bool SaveSettings(const std::filesystem::path& path, const Settings& settings, bool clearLegacyMarker)
{
    CSimpleIniA ini;
    ini.SetUnicode();
    ini.SetSpaces(false);
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    // Load the existing file first so the comments and section order that the
    // user is reading survive a save from the in-game menu. SimpleIni keeps a
    // key's comment when it updates an existing value, but drops it for a key
    // it has never seen, so this is the difference between "the menu edited two
    // numbers" and "the menu flattened the whole file".
    if (std::filesystem::exists(path)) {
        ini.LoadFile(path.string().c_str());
    }
    // Re-apply the values after loading, since the file on disk is authoritative
    // for nothing here: this save is explicitly writing the caller's settings.
    ApplySettingsToIni(ini, settings);
    if (clearLegacyMarker) {
        ini.Delete("Migration", "PreferLegacyIfPresent");
    }
    return ini.SaveFile(path.string().c_str()) >= SI_OK;
}

}
