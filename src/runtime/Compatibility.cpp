#include "Compatibility.h"

#include <RE/Skyrim.h>
#include <Windows.h>

#include <array>
#include <filesystem>
#include <ranges>

namespace SPS::Runtime {

bool ModuleLoaded(const wchar_t* name)
{
    return ::GetModuleHandleW(name) != nullptr;
}

std::optional<REL::Version> LoadedDllVersionValue(const wchar_t* name)
{
    const auto module = ::GetModuleHandleW(name);
    if (!module) {
        return std::nullopt;
    }
    std::array<wchar_t, 32768> path{};
    if (::GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size())) == 0) {
        return std::nullopt;
    }
    return REL::GetFileVersion(path.data());
}

std::string LoadedDllVersion(const wchar_t* name)
{
    if (!ModuleLoaded(name)) {
        return "not loaded";
    }
    if (const auto version = LoadedDllVersionValue(name)) {
        return version->string(".");
    }
    return "loaded (version unavailable)";
}

bool PluginLoaded(std::initializer_list<std::string_view> names)
{
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) {
        return false;
    }
    return std::ranges::any_of(names, [&](auto name) {
        return data->LookupLoadedModByName(name) != nullptr ||
            data->LookupLoadedLightModByName(name) != nullptr;
    });
}

bool FsmpActorApiAvailable()
{
    const auto version = LoadedDllVersionValue(L"hdtsmp64.dll");
    return version && *version >= REL::Version{ 4, 0, 1, 0 };
}

bool SloArousedLoaded()
{
    return ModuleLoaded(L"SexlabArousedNG.dll");
}

bool OslArousedLoaded()
{
    return ModuleLoaded(L"OSLAroused.dll");
}

bool ClassicArousedLoaded()
{
    return !SloArousedLoaded() && !OslArousedLoaded() &&
        PluginLoaded({ "SexLabAroused.esm" });
}

std::string ArousalProviderName()
{
    if (SloArousedLoaded()) {
        return "SLO Aroused NG";
    }
    if (OslArousedLoaded()) {
        return "OSL Aroused";
    }
    if (ClassicArousedLoaded()) {
        return "SexLab Aroused Redux";
    }
    return "none";
}

const wchar_t* SosAeNativeModuleName()
{
    if (ModuleLoaded(L"SOSAE.dll")) {
        return L"SOSAE.dll";
    }
    if (REL::Module::get().version() >= REL::Version{ 1, 6, 0, 0 } &&
        ModuleLoaded(L"SchlongsOfSkyrim.dll")) {
        return L"SchlongsOfSkyrim.dll";
    }
    return nullptr;
}

bool SosAeNativeLoaded()
{
    return SosAeNativeModuleName() != nullptr;
}

bool TngLoaded()
{
    return ModuleLoaded(L"TheNewGentleman.dll");
}

bool LegacySosLoaded()
{
    return PluginLoaded({ "Schlongs of Skyrim.esp" }) &&
        (std::filesystem::exists("Data/Scripts/SOS_API.pex") ||
            std::filesystem::exists("Data/Scripts/SOS_SKSE.pex"));
}

bool PositionBackendAvailable()
{
    return SosAeNativeLoaded() || TngLoaded() || LegacySosLoaded();
}

std::string PositionBackendName()
{
    const bool sosAe = SosAeNativeLoaded();
    if (TngLoaded() && sosAe) {
        return "SOS AE native + TNG events";
    }
    if (TngLoaded()) {
        return "TNG animation events";
    }
    if (sosAe) {
        return "SOS AE native / events";
    }
    if (LegacySosLoaded()) {
        return "Legacy SOS animation events";
    }
    return "none";
}

}
