#include "pch.h"
#include "Globals.h"
#include "Processors/ProcessorInterface.h"
#include "Processors/DirProcessor.h"
#include <fstream>

namespace {

// Writes a test edgeviewer.ini to the test exe's directory before main runs.
// GlobalSettings() reads this on first call, populating the static singleton
// for all subsequent T2 and T3 tests. Includes the shipped config's relevant
// keys so isType / ForcedHtmlExt / etc. work as expected.
struct WriteTestIni {
    WriteTestIni() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        fs::path iniPath = fs::path(exePath).parent_path() / L"edgeviewer.ini";

        std::ofstream os(iniPath);
        os <<
            "[Extensions]\n"
            "HTML=HTM,HTML,XHTML,XML\n"
            "Markdown=MD,MARKDOWN\n"
            "AsciiDoc=ADOC,ASCIIDOC\n"
            "URL=URL\n"
            "MHTML=MHT,MHTML\n"
            "EML=EML\n"
            "RST=RST\n"
            "Images=PNG,GIF,BMP,JPG,JPEG,ICO,WEBP,SVG\n"
            "Other=PDF\n"
            "Dirs=1\n"
            "ForcedHtmlExt=xml|xhtml\n"
            "\n"
            "[Chromium]\n"
            "KeepZoom=1\n"
            "CleanupOnExit=0\n"
            "ShowErrorBoxes=1\n"
            "\n"
            "[Markdown]\n"
            "CSS=github.css\n"
            "CSSDark=github.dark.css\n"
            "\n"
            "[AsciiDoc]\n"
            "CSS=asciidoctor.css\n"
            "\n"
            "[Images]\n"
            "CSS=none.css\n"
            "CSSDark=style-dark.css\n"
            "FitToScreen=1\n"
            "\n"
            "[Directory]\n"
            "CSS=light.css\n"
            "CSSDark=dark.css\n"
            "DirImageExt=jpg|jpeg|png|gif|svg|bmp|webp\n"
            "DirOtherExt=mp4|avi|txt\n"
            "ShowNames=1\n"
            "ShowFolders=1\n"
            "FitToScreen=1\n"
            "TruncateNames=1\n"
            "NamesUnderThumbnails=1\n"
            "GenDirThumbs=1\n"
            "DirThumbSize=256\n";
        os.close();
    }
} g_writeTestIni;

} // namespace

TEST_CASE("GlobalSettings reads [Extensions] section", "[t2]") {
    auto& settings = GlobalSettings();

    SECTION("HTML extensions are HTM,HTML,XHTML,XML") {
        REQUIRE(settings["Extensions"]["HTML"] == "HTM,HTML,XHTML,XML");
    }
    SECTION("Markdown extensions are MD,MARKDOWN") {
        REQUIRE(settings["Extensions"]["Markdown"] == "MD,MARKDOWN");
    }
    SECTION("Dirs flag is 1") {
        REQUIRE(settings["Extensions"]["Dirs"] == "1");
    }
    SECTION("ForcedHtmlExt is xml|xhtml") {
        REQUIRE(settings["Extensions"]["ForcedHtmlExt"] == "xml|xhtml");
    }
}

TEST_CASE("GlobalSettings reads per-type CSS sections", "[t2]") {
    auto& settings = GlobalSettings();

    SECTION("Markdown CSS and CSSDark both returned") {
        REQUIRE(settings["Markdown"]["CSS"] == "github.css");
        REQUIRE(settings["Markdown"]["CSSDark"] == "github.dark.css");
    }
    SECTION("AsciiDoc only declares CSS (no CSSDark)") {
        REQUIRE(settings["AsciiDoc"]["CSS"] == "asciidoctor.css");
        REQUIRE(settings.get("AsciiDoc").has("CSS"));
        REQUIRE_FALSE(settings.get("AsciiDoc").has("CSSDark"));
    }
    SECTION("Images CSS, CSSDark, and FitToScreen all readable") {
        REQUIRE(settings["Images"]["CSS"] == "none.css");
        REQUIRE(settings["Images"]["CSSDark"] == "style-dark.css");
        REQUIRE(settings["Images"]["FitToScreen"] == "1");
    }
}

TEST_CASE("GlobalSettings reads [Directory] complete section", "[t2]") {
    auto& settings = GlobalSettings();
    const auto& dir = settings.get("Directory");

    REQUIRE(dir.get("DirImageExt") == "jpg|jpeg|png|gif|svg|bmp|webp");
    REQUIRE(dir.get("DirOtherExt") == "mp4|avi|txt");
    REQUIRE(dir.get("CSS") == "light.css");
    REQUIRE(dir.get("CSSDark") == "dark.css");
    REQUIRE(dir.get("ShowNames") == "1");
    REQUIRE(dir.get("ShowFolders") == "1");
    REQUIRE(dir.get("FitToScreen") == "1");
    REQUIRE(dir.get("TruncateNames") == "1");
    REQUIRE(dir.get("NamesUnderThumbnails") == "1");
    REQUIRE(dir.get("GenDirThumbs") == "1");
    REQUIRE(dir.get("DirThumbSize") == "256");
}

TEST_CASE("ProcessorInterface::isType reads GlobalSettings", "[t2]") {
    DirProcessor proc;

    SECTION(".md matches the Markdown section") {
        REQUIRE(proc.isType(L".md", "Markdown"));
        REQUIRE(proc.isType(L".markdown", "Markdown"));
    }
    SECTION(".htm and .html match the HTML section") {
        REQUIRE(proc.isType(L".htm", "HTML"));
        REQUIRE(proc.isType(L".html", "HTML"));
    }
    SECTION("case-insensitive: .MD matches Markdown") {
        REQUIRE(proc.isType(L".MD", "Markdown"));
    }
    SECTION(".txt does not match Markdown") {
        REQUIRE_FALSE(proc.isType(L".txt", "Markdown"));
    }
    SECTION(".pdf matches Other, not Markdown") {
        REQUIRE(proc.isType(L".pdf", "Other"));
        REQUIRE_FALSE(proc.isType(L".pdf", "Markdown"));
    }
}

TEST_CASE("ForcedHtmlExt regex matches xml and xhtml only", "[t2]") {
    auto& settings = GlobalSettings();
    auto forced = to_utf16(settings["Extensions"]["ForcedHtmlExt"]);
    auto re = std::wregex(std::format(L".+\\.({})$", forced), std::regex_constants::icase);

    REQUIRE(std::regex_match(std::wstring(L"file.xml"), re));
    REQUIRE(std::regex_match(std::wstring(L"file.xhtml"), re));
    REQUIRE_FALSE(std::regex_match(std::wstring(L"file.txt"), re));
    REQUIRE_FALSE(std::regex_match(std::wstring(L"file.html"), re));
}