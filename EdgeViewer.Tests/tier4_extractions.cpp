#include "pch.h"
#include "Globals.h"
#include "WlxDetect.h"
#include "ZoomHotkey.h"
#include "Navigator.h"
#include "TestHelpers/IniBuilder.h"

TEST_CASE("BuildDetectString matches full shipped ini format", "[t4][smoke]") {
    auto ini = IniBuilder()
        .with("Extensions", "HTML", "HTM,HTML,XHTML,XML")
        .with("Extensions", "Markdown", "MD,MARKDOWN")
        .with("Extensions", "AsciiDoc", "ADOC,ASCIIDOC")
        .with("Extensions", "URL", "URL")
        .with("Extensions", "MHTML", "MHT,MHTML")
        .with("Extensions", "EML", "EML")
        .with("Extensions", "RST", "RST")
        .with("Extensions", "Images", "PNG,GIF,BMP,JPG,JPEG,ICO,WEBP,SVG")
        .with("Extensions", "Other", "PDF")
        .with("Extensions", "Dirs", "1")
        .build();

    auto expected = std::string(
        "EXT=\"HTM\"|EXT=\"HTML\"|EXT=\"XHTML\"|EXT=\"XML\""
        "|EXT=\"MD\"|EXT=\"MARKDOWN\""
        "|EXT=\"ADOC\"|EXT=\"ASCIIDOC\""
        "|EXT=\"URL\""
        "|EXT=\"MHT\"|EXT=\"MHTML\""
        "|EXT=\"EML\""
        "|EXT=\"RST\""
        "|EXT=\"PNG\"|EXT=\"GIF\"|EXT=\"BMP\"|EXT=\"JPG\"|EXT=\"JPEG\"|EXT=\"ICO\"|EXT=\"WEBP\"|EXT=\"SVG\""
        "|EXT=\"PDF\""
        "|EXT=\"\""
    );

    REQUIRE(BuildDetectString(ini) == expected);
}

TEST_CASE("BuildDetectString Dirs controls trailing empty ext", "[t4][smoke]") {
    auto ini_dirson = IniBuilder()
        .with("Extensions", "HTML", "HTM,HTML")
        .with("Extensions", "Markdown", "MD")
        .with("Extensions", "Dirs", "1")
        .build();
    auto ini_dirsoff = IniBuilder()
        .with("Extensions", "HTML", "HTM,HTML")
        .with("Extensions", "Markdown", "MD")
        .with("Extensions", "Dirs", "0")
        .build();

    SECTION("Dirs=1 produces one more empty extension than Dirs=0") {
        auto with_dirs = BuildDetectString(ini_dirson);
        auto without_dirs = BuildDetectString(ini_dirsoff);
        REQUIRE(with_dirs == without_dirs + "|EXT=\"\"");
    }
}

TEST_CASE("ZoomHotkeyHandled pure function", "[t4][smoke]") {
    double newZoom = -1.0;

    SECTION("Ctrl+VK_OEM_PLUS at zoom 1.0 snaps up to 1.1") {
        REQUIRE(ZoomHotkeyHandled(VK_OEM_PLUS, true, 1.0, newZoom));
        REQUIRE(newZoom == 1.1);
    }
    SECTION("Ctrl+VK_OEM_MINUS at zoom 1.0 snaps down to 0.9") {
        REQUIRE(ZoomHotkeyHandled(VK_OEM_MINUS, true, 1.0, newZoom));
        REQUIRE(newZoom == 0.9);
    }
    SECTION("Ctrl+'0' resets to 1.0 from any zoom") {
        REQUIRE(ZoomHotkeyHandled('0', true, 2.5, newZoom));
        REQUIRE(newZoom == 1.0);
        REQUIRE(ZoomHotkeyHandled(VK_NUMPAD0, true, 0.33, newZoom));
        REQUIRE(newZoom == 1.0);
    }
    SECTION("ceiling at 5.0 snaps no further up") {
        REQUIRE(ZoomHotkeyHandled(VK_OEM_PLUS, true, 5.0, newZoom));
        REQUIRE(newZoom == 5.0);
    }
    SECTION("floor at 0.25 snaps no further down") {
        REQUIRE(ZoomHotkeyHandled(VK_OEM_MINUS, true, 0.25, newZoom));
        REQUIRE(newZoom == 0.25);
    }
    SECTION("VK_ADD and VK_SUBTRACT are also zoom hotkeys") {
        REQUIRE(ZoomHotkeyHandled(VK_ADD, true, 1.0, newZoom));
        REQUIRE(newZoom == 1.1);
        REQUIRE(ZoomHotkeyHandled(VK_SUBTRACT, true, 1.0, newZoom));
        REQUIRE(newZoom == 0.9);
    }
    SECTION("non-zoom key returns false and leaves zoom untouched") {
        newZoom = 3.14;
        REQUIRE_FALSE(ZoomHotkeyHandled('A', true, 1.0, newZoom));
        REQUIRE(newZoom == 3.14);
    }
    SECTION("zoom keys without Ctrl return false") {
        REQUIRE_FALSE(ZoomHotkeyHandled(VK_OEM_PLUS, false, 1.0, newZoom));
        REQUIRE_FALSE(ZoomHotkeyHandled('0', false, 1.0, newZoom));
        REQUIRE_FALSE(ZoomHotkeyHandled(VK_OEM_MINUS, false, 1.0, newZoom));
    }
}

TEST_CASE("BuildFindScript default no flags", "[t4][smoke]") {
    auto script = BuildFindScript(L"hello", 0);
    REQUIRE(script == L"window.find('hello', false, false, false, false, false, false);");
}

TEST_CASE("BuildFindScript matchcase flag", "[t4][smoke]") {
    auto script = BuildFindScript(L"x", lcs_matchcase);
    REQUIRE(script == L"window.find('x', true, false, false, false, false, false);");
}

TEST_CASE("BuildFindScript backwards flag", "[t4][smoke]") {
    auto script = BuildFindScript(L"x", lcs_backwards);
    REQUIRE(script == L"window.find('x', false, true, false, false, false, false);");
}

TEST_CASE("BuildFindScript wholewords flag", "[t4][smoke]") {
    auto script = BuildFindScript(L"x", lcs_wholewords);
    REQUIRE(script == L"window.find('x', false, false, false, true, false, false);");
}

TEST_CASE("BuildFindScript all three flags combined", "[t4][smoke]") {
    auto script = BuildFindScript(L"x", lcs_matchcase | lcs_backwards | lcs_wholewords);
    REQUIRE(script == L"window.find('x', true, true, false, true, false, false);");
}

TEST_CASE("BuildFindScript findfirst loop form", "[t4][smoke]") {
    SECTION("findfirst alone produces while-backwards script") {
        auto script = BuildFindScript(L"x", lcs_findfirst);
        REQUIRE(script.find(L"while(window.find(") == 0);
        // findfirst uses !backwards (false becomes !false = true in JS)
        REQUIRE(script == L"while(window.find('x', false, !false, false, false, false, false));");
    }
    SECTION("findfirst + matchcase keeps case sensitivity in while loop") {
        auto script = BuildFindScript(L"x", lcs_findfirst | lcs_matchcase);
        REQUIRE(script == L"while(window.find('x', true, !false, false, false, false, false));");
    }
}

TEST_CASE("BuildFindScript escapes special characters in pattern", "[t4][smoke]") {
    SECTION("single quote in pattern is escaped") {
        auto script = BuildFindScript(L"don't", 0);
        REQUIRE(script.find(L"don\\'t") != std::wstring::npos);
    }
    SECTION("backslash in pattern is escaped (default form)") {
        auto script = BuildFindScript(L"C:\\path", 0);
        REQUIRE(script.find(L"C:\\\\path") != std::wstring::npos);
    }
    SECTION("both characters interleaved are both escaped") {
        auto script = BuildFindScript(L"it's C:\\", 0);
        REQUIRE(script.find(L"it\\'s C:\\\\") != std::wstring::npos);
    }
}

TEST_CASE("BuildPrintScript returns literal window.print()", "[t4][smoke]") {
    REQUIRE(BuildPrintScript() == L"window.print();");
}