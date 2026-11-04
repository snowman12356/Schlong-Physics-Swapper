#pragma once

#include "Settings.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SPS::Core {

struct SettingsLoadResult {
    Settings settings{};
    bool migratedLegacy{ false };
    bool shouldWriteCurrent{ false };
    // True when the current file still carries the one-time import marker and
    // must have it cleared, so a later launch does not re-import legacy values
    // over the player's current configuration.
    bool dropLegacyMarker{ false };
};

SettingsLoadResult LoadSettings(
    const std::filesystem::path& currentPath,
    const std::filesystem::path& legacyPath);

// The section and key names this build reads and writes, as "Section|Key".
// Used by the tests to prove the shipped INI template documents every setting.
std::vector<std::string> SettingsKeyNames();
bool SaveSettings(const std::filesystem::path& path, const Settings& settings,
    bool clearLegacyMarker = false);

}
