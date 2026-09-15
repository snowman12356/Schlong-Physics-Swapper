#pragma once

#include "Diagnostics.h"
#include <filesystem>

namespace SPS::Diagnostics {

// Adds file-only checks to a fresh snapshot; also used by the startup scan.
void ScanPhysicsFiles(Snapshot& result, const std::filesystem::path& pluginRoot);

}
