#include "Diagnostics.h"

#include "runtime/Compatibility.h"

#include <RE/Skyrim.h>
#include <SKSEMenuFramework.h>
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <ranges>

namespace SPS::Diagnostics {

namespace {

namespace fs = std::filesystem;

constexpr std::array<const char*, 6> kBones{
    "NPC Genitals01 [Gen01]", "NPC Genitals02 [Gen02]", "NPC Genitals03 [Gen03]",
    "NPC Genitals04 [Gen04]", "NPC Genitals05 [Gen05]", "NPC Genitals06 [Gen06]"
};

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string ReadText(const fs::path& path)
{
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec || size > 8 * 1024 * 1024) {
        return {};
    }
    std::ifstream stream(path, std::ios::binary);
    return stream ? std::string(std::istreambuf_iterator<char>(stream), {}) : std::string{};
}

bool ContainsAll(const std::string& lower, const std::array<std::string, 6>& values)
{
    return std::ranges::all_of(values, [&](const auto& value) {
        return lower.contains(value);
    });
}

void AddSummary(std::string& summary, const fs::path& path, int count)
{
    if (count == 1) {
        summary.clear();
    }
    if (!summary.empty()) {
        summary += ", ";
    }
    summary += path.filename().string();
}

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
    result.crashLoggerLoaded = ModuleLoaded(L"CrashLogger.dll");

    result.playerBonesFound = CountPlayerBones();

    const std::array<std::string, 6> boneKeys{
        "npc genitals01 [gen01]", "npc genitals02 [gen02]", "npc genitals03 [gen03]",
        "npc genitals04 [gen04]", "npc genitals05 [gen05]", "npc genitals06 [gen06]"
    };
    const std::array<std::string, 6> parameterKeys{
        "ubeps01", "ubeps02", "ubeps03", "ubeps04", "ubeps05", "ubeps06"
    };
    std::error_code ec;
    const fs::path xmlRoot{ "Data/SKSE/Plugins/hdtSkinnedMeshConfigs" };
    if (fs::exists(xmlRoot, ec)) {
        for (fs::recursive_directory_iterator it(xmlRoot, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file(ec) || Lower(it->path().extension().string()) != ".xml") {
                continue;
            }
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
        for (fs::directory_iterator it(pluginRoot, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file(ec) || Lower(it->path().extension().string()) != ".txt") {
                continue;
            }
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
    result.checkedAtMs = nowMs;
    return result;
}

}
