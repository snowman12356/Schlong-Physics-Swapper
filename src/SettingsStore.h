#pragma once

#include "Settings.h"

#include <filesystem>

namespace SPS::Core {

struct SettingsLoadResult {
    Settings settings{};
    bool migratedLegacy{ false };
    bool shouldWriteCurrent{ false };
};

SettingsLoadResult LoadSettings(
    const std::filesystem::path& currentPath,
    const std::filesystem::path& legacyPath);
bool SaveSettings(const std::filesystem::path& path, const Settings& settings);

}
