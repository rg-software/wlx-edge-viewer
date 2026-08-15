#include "pch.h"
#include "Globals.h"
#include "Navigator.h"
#include "Processors/ProcessorInterface.h"
#include "Processors/DirProcessor.h"
#include "Processors/HtmlProcessor.h"
#include "TestHelpers/TempDir.h"
#include <fstream>

TEST_CASE("to_utf8 + to_utf16 round-trip", "[t1][smoke]") {
    REQUIRE(to_utf16(to_utf8(L"hello world")) == L"hello world");
    REQUIRE(to_utf16(to_utf8(L"")) == L"");
    REQUIRE(to_utf16(to_utf8(L"emoji \xD83D\xDE00")) == L"emoji \xD83D\xDE00");
}

TEST_CASE("to_utf8 known conversions", "[t1][smoke]") {
    REQUIRE(to_utf8(L"hello") == "hello");
    REQUIRE(to_utf8(L"").empty());
}

TEST_CASE("to_utf16 known conversions", "[t1][smoke]") {
    REQUIRE(to_utf16("").empty());
    REQUIRE(to_utf16(std::string("hello")) == L"hello");
}

TEST_CASE("to_int parser", "[t1][smoke]") {
    REQUIRE(to_int(std::string("42")) == 42);
    REQUIRE(to_int(std::string("0")) == 0);
    REQUIRE(to_int(std::string("-1")) == -1);
    REQUIRE(to_int(std::string("123456")) == 123456);
    REQUIRE(to_int(std::string("")) == 0);
    REQUIRE(to_int(std::string("abc")) == 0);
    REQUIRE(to_int(std::string(" 42")) == 42);
}

TEST_CASE("ProcessorInterface::replacePlaceholders", "[t1][smoke]") {
    DirProcessor proc;
    using WSP = ProcessorInterface::WStrPair;

    SECTION("single placeholder replacement") {
        REQUIRE(proc.replacePlaceholders(L"hello __NAME__", {{L"__NAME__", L"World"}}) == L"hello World");
    }
    SECTION("multiple placeholders") {
        REQUIRE(proc.replacePlaceholders(L"__A__ and __B__", {WSP{L"__A__", L"first"}, WSP{L"__B__", L"second"}}) == L"first and second");
    }
    SECTION("no placeholder present is unchanged") {
        REQUIRE(proc.replacePlaceholders(L"plain text", {WSP{L"__X__", L"value"}}) == L"plain text");
    }
    SECTION("placeholder appearing multiple times") {
        REQUIRE(proc.replacePlaceholders(L"__X__ __X__ __X__", {WSP{L"__X__", L"v"}}) == L"v v v");
    }
    SECTION("regex-meaningful characters in value are literal") {
        REQUIRE(proc.replacePlaceholders(L"__P__", {WSP{L"__P__", L"$path\\name"}}) == L"$path\\name");
    }
}

TEST_CASE("DirProcessor::stripTwodots", "[t1][smoke]") {
    DirProcessor proc;
    REQUIRE(proc.stripTwodots("C:\\dir\\..\\") == "C:\\dir\\");
    REQUIRE(proc.stripTwodots("C:\\dir\\file.txt") == "C:\\dir\\file.txt");
    REQUIRE(proc.stripTwodots("C:\\dir\\..") == "C:\\dir\\..");
    REQUIRE(proc.stripTwodots(fs::path(L"")).empty());
}

TEST_CASE("DirProcessor::extensionsToMaskRegex", "[t1][smoke]") {
    DirProcessor proc;

    SECTION("single extension matches only that extension") {
        auto re = proc.extensionsToMaskRegex("png");
        REQUIRE(std::regex_match(std::wstring(L"file.png"), re));
        REQUIRE_FALSE(std::regex_match(std::wstring(L"file.txt"), re));
    }
    SECTION("pipe-separated extensions match all variants") {
        auto re = proc.extensionsToMaskRegex("jpg|png");
        REQUIRE(std::regex_match(std::wstring(L"file.jpg"), re));
        REQUIRE(std::regex_match(std::wstring(L"file.png"), re));
    }
    SECTION("case-insensitive matching") {
        auto re = proc.extensionsToMaskRegex("png");
        REQUIRE(std::regex_match(std::wstring(L"file.PNG"), re));
        REQUIRE(std::regex_match(std::wstring(L"file.Png"), re));
    }
}

TEST_CASE("jsEscape free function", "[t1][smoke]") {
    REQUIRE(jsEscape(L"hello world") == L"hello world");
    REQUIRE(jsEscape(L"") == L"");
    REQUIRE(jsEscape(L"C:\\path") == L"C:\\\\path");
    REQUIRE(jsEscape(L"don't") == L"don\\'t");
    REQUIRE(jsEscape(L"it's C:\\") == L"it\\'s C:\\\\");
}