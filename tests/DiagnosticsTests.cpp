#include "diagnostics/PhysicsFileScan.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <system_error>

namespace {
namespace fs = std::filesystem;
int failures = 0;

void Expect(bool condition, std::string_view name)
{
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

struct TestDirectory {
    fs::path path = fs::temp_directory_path() / ("sps-diagnostics-tests-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TestDirectory()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void Write(const fs::path& path, std::string_view content)
{
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    stream << content;
    if (!stream) {
        throw std::runtime_error("Could not write diagnostic test fixture");
    }
}

std::string Utf8(std::u8string_view value)
{
    return { reinterpret_cast<const char*>(value.data()), value.size() };
}
}

int main(int argc, char** argv)
{
    TestDirectory directory;
    try {
        const auto plugins = directory.path / u8"Plugins_\u65e5\u672c\U0001F9EA";
        SPS::Diagnostics::Snapshot missing;
        SPS::Diagnostics::ScanPhysicsFiles(missing, plugins);
        Expect(missing.xmlFiles == 0 && missing.cbpcMapFiles == 0 &&
            missing.cbpcParameterFiles == 0 && missing.xmlSummary == "None found",
            "missing plugin directory is harmless");

        // Startup used to convert every TXT filename, even an unrelated readme.
        Write(plugins / u8"Readme_\u65e5\u672c\U0001F9EA.TXT", "unrelated text");
        SPS::Diagnostics::Snapshot unrelated;
        SPS::Diagnostics::ScanPhysicsFiles(unrelated, plugins);
        Expect(unrelated.cbpcMapFiles == 0 && unrelated.cbpcParameterFiles == 0,
            "unrelated Unicode TXT filename does not crash or count as a config");
        if (argc > 1 && std::string_view(argv[1]) == "--reproduce-txt") {
            return failures;
        }

        const std::string bones =
            "NPC Genitals01 [Gen01]\nNPC Genitals02 [Gen02]\nNPC Genitals03 [Gen03]\n"
            "NPC Genitals04 [Gen04]\nNPC Genitals05 [Gen05]\nNPC Genitals06 [Gen06]\n";
        const std::string parameters = "UBEps01 UBEps02 UBEps03 UBEps04 UBEps05 UBEps06";
        const auto xmlRoot = plugins / "hdtSkinnedMeshConfigs";
        const auto xmlName = u8"MaleGenitals_\u65e5\u672c\U0001F9EA.XML";
        const auto mapName = u8"CBPCMasterConfig_\u00e9\u65e5\u672c.TXT";
        const auto parametersName = u8"CBPConfig_\U0001F9EA.txt";
        Write(xmlRoot / "MaleGenitals.xml", "<system>" + bones + "</system>");
        Write(xmlRoot / u8"Nested_\u65e5\u672c" / xmlName, "<system>" + bones + "</system>");
        Write(xmlRoot / "Incomplete.xml", "<system>NPC Genitals01 [Gen01]</system>");
        Write(xmlRoot / "NotASystem.xml", bones);
        Write(xmlRoot / u8"Ignored.\u65e5\u672c", "<system>" + bones + "</system>");
        Write(plugins / "CBPCMasterConfig.txt", bones);
        Write(plugins / mapName, bones);
        Write(plugins / "CBPCMasterConfig_Incomplete.txt", "NPC Genitals01 [Gen01]");
        Write(plugins / "CBPConfig.txt", parameters);
        Write(plugins / parametersName, parameters);
        Write(plugins / "CBPConfig_Incomplete.txt", "UBEps01");
        Write(plugins / u8"CBPCMasterConfig.\u65e5\u672c", bones);
        Write(plugins / "Nested" / "CBPCMasterConfig.txt", bones);

        SPS::Diagnostics::Snapshot result;
        SPS::Diagnostics::ScanPhysicsFiles(result, plugins);
        Expect(result.xmlFiles == 4 && result.compatibleXmlFiles == 2,
            "recursive XML checks retain system and six-bone requirements");
        Expect(result.cbpcMapFiles == 3 && result.compatibleCbpcMaps == 2,
            "map checks accept Unicode filenames and exclude incomplete or nested maps");
        Expect(result.cbpcParameterFiles == 3 && result.compatibleCbpcParameters == 2,
            "parameter checks retain six-section requirements");
        Expect(result.xmlSummary.contains("MaleGenitals.xml") && result.xmlSummary.contains(Utf8(xmlName)),
            "XML summaries retain ASCII and UTF-8 filenames");
        Expect(result.cbpcMapSummary.contains(Utf8(mapName)) &&
            result.cbpcParameterSummary.contains(Utf8(parametersName)),
            "CBPC summaries retain UTF-8 filenames");
        Expect(!result.xmlSummary.contains("Incomplete") && !result.cbpcMapSummary.contains("Incomplete"),
            "incompatible files do not appear in compatible summaries");
#ifdef _WIN32
        const auto malformedName = std::wstring(L"CBPConfig_") + wchar_t{ 0xD800 } + L".txt";
        Write(plugins / malformedName, parameters);
        SPS::Diagnostics::Snapshot malformed;
        SPS::Diagnostics::ScanPhysicsFiles(malformed, plugins);
        Expect(malformed.compatibleCbpcParameters == 3 &&
            malformed.cbpcParameterSummary.contains("<filename unavailable>") &&
            malformed.cbpcParameterSummary.contains(Utf8(parametersName)),
            "unpaired UTF-16 surrogate uses a display fallback without losing valid configs");
#endif
    } catch (const std::system_error& error) {
        std::cerr << "FAIL: filesystem scan threw code " << error.code().value() << ": " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    if (failures == 0) {
        std::cout << "All diagnostics tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
