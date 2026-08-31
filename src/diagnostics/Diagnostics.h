#pragma once

#include <cstdint>
#include <string>

namespace SPS::Diagnostics {

struct Snapshot {
    bool menuFrameworkLoaded{ false };
    bool oslModuleLoaded{ false };
    bool fsmpModuleLoaded{ false };
    bool fsmpActorApiAvailable{ false };
    bool fsmpBridgePresent{ false };
    bool cbpcModuleLoaded{ false };
    bool sexLabModuleLoaded{ false };
    bool sexLabPluginLoaded{ false };
    bool sexLabRoleBridgePresent{ false };
    bool ostimPluginLoaded{ false };
    bool ostimRoleBridgePresent{ false };
    bool oslPluginLoaded{ false };
    bool classicArousedPluginLoaded{ false };
    bool supportedAddonLoaded{ false };
    bool sosPluginLoaded{ false };
    bool tngPluginLoaded{ false };
    bool sosPhysicsManagerLoaded{ false };
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

Snapshot Scan(std::int64_t nowMs);

}
