#include "PhysicsFileScan.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <ranges>
#include <system_error>

namespace SPS::Diagnostics {
namespace {
namespace fs = std::filesystem;
template <class Char>
std::basic_string<Char> Lower(std::basic_string<Char> value)
{
    // Config keys are ASCII; leave all other filename/code-unit values intact.
    std::transform(value.begin(), value.end(), value.begin(), [](Char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<Char>(c + ('a' - 'A')) : c;
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
    std::string filename;
    try {
        // path::string() uses the Windows code page and can throw for valid names.
        const auto utf8 = path.filename().u8string();
        filename.assign(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    } catch (const std::system_error&) {
        // Windows can also enumerate names containing unpaired UTF-16 surrogates.
        // An optional display label must not abort startup or discard other files.
        filename = "<filename unavailable>";
    }
    if (count == 1) {
        summary.clear();
    }
    if (!summary.empty()) {
        summary += ", ";
    }
    summary += filename;
}

}

void ScanPhysicsFiles(Snapshot& result, const std::filesystem::path& pluginRoot)
{
    const std::array<std::string, 6> boneKeys{
        "npc genitals01 [gen01]", "npc genitals02 [gen02]", "npc genitals03 [gen03]",
        "npc genitals04 [gen04]", "npc genitals05 [gen05]", "npc genitals06 [gen06]"
    };
    const std::array<std::string, 6> parameterKeys{
        "ubeps01", "ubeps02", "ubeps03", "ubeps04", "ubeps05", "ubeps06"
    };
    std::error_code ec;
    const auto xmlRoot = pluginRoot / "hdtSkinnedMeshConfigs";
    if (fs::exists(xmlRoot, ec)) {
        for (fs::recursive_directory_iterator it(xmlRoot, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file(ec) || Lower(it->path().extension().native()) != fs::path(".xml").native()) {
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

    if (fs::exists(pluginRoot, ec)) {
        for (fs::directory_iterator it(pluginRoot, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file(ec) || Lower(it->path().extension().native()) != fs::path(".txt").native()) {
                continue;
            }
            const auto filename = Lower(it->path().filename().native());
            const auto text = Lower(ReadText(it->path()));
            if (filename.contains(fs::path("cbpcmasterconfig").native())) {
                ++result.cbpcMapFiles;
                if (ContainsAll(text, boneKeys)) {
                    ++result.compatibleCbpcMaps;
                    AddSummary(result.cbpcMapSummary, it->path(), result.compatibleCbpcMaps);
                }
            } else if (filename.starts_with(fs::path("cbpconfig").native())) {
                ++result.cbpcParameterFiles;
                if (ContainsAll(text, parameterKeys)) {
                    ++result.compatibleCbpcParameters;
                    AddSummary(result.cbpcParameterSummary, it->path(), result.compatibleCbpcParameters);
                }
            }
        }
    }
}
}
