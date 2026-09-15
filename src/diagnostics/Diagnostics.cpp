#include "Diagnostics.h"
#include "PhysicsFileScan.h"

#include "runtime/Compatibility.h"

#include <RE/Skyrim.h>
#include <SKSEMenuFramework.h>
#include <Windows.h>

#include <array>
#include <filesystem>

namespace SPS::Diagnostics {

namespace {

namespace fs = std::filesystem;

constexpr std::array<const char*, 6> kBones{
    "NPC Genitals01 [Gen01]", "NPC Genitals02 [Gen02]", "NPC Genitals03 [Gen03]",
    "NPC Genitals04 [Gen04]", "NPC Genitals05 [Gen05]", "NPC Genitals06 [Gen06]"
};

}

int CountPlayerBones()
{
    int found = 0;
    if (auto* player = RE::PlayerCharacter::GetSingleton()) {
        if (auto* root = player->Get3D()) {
            for (auto bone : kBones) {
                if (root->GetObjectByName(RE::BSFixedString(bone))) {
                    ++found;
                }
            }
        }
    }
    return found;
}

Snapshot Scan(std::int64_t nowMs)
{
    using namespace SPS::Runtime;
    Snapshot result;
    result.menuFrameworkLoaded = SKSEMenuFramework::IsInstalled() && ModuleLoaded(L"SKSEMenuFramework.dll");
    result.classicArousedPluginLoaded = PluginLoaded({ "SexLabAroused.esm" }) &&
        !OslArousedLoaded() && !SloArousedLoaded();
    result.oslModuleLoaded = OslArousedLoaded() || SloArousedLoaded() || result.classicArousedPluginLoaded;
    result.fsmpModuleLoaded = ModuleLoaded(L"hdtsmp64.dll");
    result.fsmpActorApiAvailable = FsmpActorApiAvailable();
    result.fsmpBridgePresent = fs::exists("Data/Scripts/SPS_FSMPBridge.pex");
    result.cbpcModuleLoaded = ModuleLoaded(L"cbp.dll");
    result.sexLabModuleLoaded = ModuleLoaded(L"SexLabUtil.dll");
    result.sexLabPluginLoaded = PluginLoaded({ "SexLab.esm" });
    result.sexLabRoleBridgePresent = fs::exists("Data/Scripts/SPS_SexLabBridge.pex");
    result.ostimPluginLoaded = PluginLoaded({ "OStim.esp" }) || ModuleLoaded(L"OStim.dll");
    result.ostimRoleBridgePresent = fs::exists("Data/Scripts/SPS_OStimBridge.pex");
    result.oslPluginLoaded = PluginLoaded({ "OSLAroused.esp", "OAroused.esp", "SexLabAroused.esm" });
    result.tngPluginLoaded = TngLoaded();
    result.supportedAddonLoaded = PluginLoaded({
        "UBE_SOS_Addon.esp", "UBE_AllRace.esp", "3BBB UBE patch.esp",
        "SOS - Dw3BA - Futanari Addon.esp", "TheNewGentleman.esp"
    });
    result.sosPluginLoaded = LegacySosLoaded();
    result.sosPhysicsManagerLoaded = PluginLoaded({ "SOSPhysicsManager.esp" });
    result.physicsEditorLoaded = ModuleLoaded(L"PhysicsEditor.dll");
    result.autoPhysicsResetLoaded = ModuleLoaded(L"AutoSMPReset.dll") ||
        ModuleLoaded(L"AutoPhysicsReset.dll") || ModuleLoaded(L"AutoPhysicsResetNG.dll");
    result.softbodyLoaded = SoftbodyLoaded();
    result.crashLoggerLoaded = ModuleLoaded(L"CrashLogger.dll");

    result.playerBonesFound = CountPlayerBones();

    ScanPhysicsFiles(result, "Data/SKSE/Plugins");
    result.checkedAtMs = nowMs;
    return result;
}

}
