#pragma once

#include "ActivityLog.h"
#include "Diagnostics.h"
#include "Settings.h"
#include "api/ExternalControlRegistry.h"
#include "controllers/PhysicsOwnershipController.h"
#include "controllers/PositionController.h"
#include "controllers/RecoveryController.h"

#include <cstdint>
#include <string>

namespace SPS::Diagnostics {

struct SceneReportSnapshot {
    bool sexLabActive{ false };
    bool sexLabConnected{ false };
    std::string sexLabRole;
    bool sexLabRoleValid{ false };
    bool ostimActive{ false };
    bool ostimConnected{ false };
    std::string ostimRole;
    bool ostimRoleValid{ false };
};

struct ComponentVersionSnapshot {
    std::string menuFramework;
    std::string osl;
    std::string slo;
    std::string fsmp;
    std::string cbpc;
    std::string sexLab;
    std::string ostim;
    std::string sos;
};

struct SupportReportInput {
    std::string version;
    std::string runtimeVersion;
    std::string skseVersion;
    ComponentVersionSnapshot components;
    Snapshot diagnostics;
    Controllers::PhysicsOwnershipSnapshot ownership;
    Controllers::PositionSnapshot position;
    Controllers::RecoverySnapshot recovery;
    SceneReportSnapshot scenes;
    ActivitySnapshot activity;
    APIControl::Stats apiStats;
    Core::Settings settings;
    std::string activeAPIRequester{ "none" };
    std::string arousalProvider;
    std::string positionBackend;
    std::string lastBendMethod;
    std::string fixes;
    float arousal{ 0.0F };
    bool arousalConnected{ false };
    bool positionBackendReady{ false };
    bool ppaLoaded{ false };
    unsigned apiVersion{ 0 };
    unsigned externalResetNotices{ 0 };
    unsigned ownerRestorations{ 0 };
    std::int64_t nowMs{ 0 };
};

[[nodiscard]] std::string FormatSupportReport(const SupportReportInput& input);

}
