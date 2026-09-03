#pragma once

#include <REL/Version.h>

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace RE {
class Actor;
}

namespace SPS::Runtime {

bool ModuleLoaded(const wchar_t* name);
std::optional<REL::Version> LoadedDllVersionValue(const wchar_t* name);
std::string LoadedDllVersion(const wchar_t* name);
bool PluginLoaded(std::initializer_list<std::string_view> names);
bool FsmpActorApiAvailable();
bool SloArousedLoaded();
bool OslArousedLoaded();
std::optional<float> ReadOslArousal(RE::Actor* actor);
bool ClassicArousedLoaded();
std::string ArousalProviderName();
const wchar_t* SosAeNativeModuleName();
bool SosAeNativeLoaded();
bool TngLoaded();
bool LegacySosLoaded();
bool SoftbodyLoaded();
bool PositionBackendAvailable();
std::string PositionBackendName();

}
