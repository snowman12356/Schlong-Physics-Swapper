#include "Compatibility.h"

#include <RE/Skyrim.h>
#include <Windows.h>

#include <array>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <ranges>
#include <unordered_map>

namespace SPS::Runtime {

namespace {

std::atomic<bool> compatibilityFrozen{ false };

// -1 = not computed yet. Before FreezeCompatibilityCache the value is always
// recomputed, so nothing is cached while the load order is still being read.
template <class Compute>
bool CachedCheck(std::atomic<int>& slot, Compute compute)
{
    if (!compatibilityFrozen.load()) {
        return compute();
    }
    const int cached = slot.load();
    if (cached >= 0) {
        return cached != 0;
    }
    const bool result = compute();
    slot.store(result ? 1 : 0);
    return result;
}

std::atomic<int> legacySosCache{ -1 };
std::atomic<int> softbodyCache{ -1 };
std::atomic<int> classicArousedPluginCache{ -1 };
std::atomic<int> positionBridgeCache{ -1 };

}

void FreezeCompatibilityCache()
{
    compatibilityFrozen.store(true);
}

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
    // A loaded module's file version cannot change while it stays loaded, so
    // read the version resource once per module instead of on every call.
    static std::mutex cacheLock;
    static std::unordered_map<HMODULE, std::optional<REL::Version>> cache;
    {
        std::scoped_lock lock(cacheLock);
        if (const auto found = cache.find(module); found != cache.end()) {
            return found->second;
        }
    }
    std::array<wchar_t, 32768> path{};
    std::optional<REL::Version> version;
    if (::GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size())) != 0) {
        version = REL::GetFileVersion(path.data());
    }
    std::scoped_lock lock(cacheLock);
    cache.insert_or_assign(module, version);
    return version;
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

std::optional<float> ReadOslArousal(RE::Actor* actor)
{
    using GetArousalFunction = float (*)(RE::Actor*);

    const auto module = ::GetModuleHandleW(L"OSLAroused.dll");
    if (!module || !actor) {
        return std::nullopt;
    }
    const auto procedure = ::GetProcAddress(module, "GetArousalExt");
    if (!procedure) {
        return std::nullopt;
    }
    const auto getArousal = reinterpret_cast<GetArousalFunction>(procedure);
    const float reading = getArousal(actor);
    return std::isfinite(reading) ? std::optional<float>{ reading } : std::nullopt;
}

bool ClassicArousedLoaded()
{
    return !SloArousedLoaded() && !OslArousedLoaded() &&
        CachedCheck(classicArousedPluginCache, [] { return PluginLoaded({ "SexLabAroused.esm" }); });
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
    return CachedCheck(legacySosCache, [] {
        std::error_code ec;
        return PluginLoaded({ "Schlongs of Skyrim.esp" }) &&
            (std::filesystem::exists("Data/Scripts/SOS_API.pex", ec) ||
                std::filesystem::exists("Data/Scripts/SOS_SKSE.pex", ec));
    });
}

bool SoftbodyLoaded()
{
    return CachedCheck(softbodyCache, [] { return PluginLoaded({ "HDT SMP Object - Simple.esp" }); });
}

bool PositionBridgeScriptPresent()
{
    return CachedCheck(positionBridgeCache, [] {
        std::error_code ec;
        return std::filesystem::exists("Data/Scripts/SPS_PositionBridge.pex", ec);
    });
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
