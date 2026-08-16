#include "pch.h"
#include "Globals.h"
#include "TestHelpers/TempDir.h"
#include <fstream>

TEST_CASE("GetPhysicalPath passes plain paths unchanged", "[t3]") {
    TempDir td;
    auto f = td.path() / "test.md";
    std::ofstream(f) << "hello";
    
    auto result = GetPhysicalPath(f);
    REQUIRE(fs::exists(result));
    REQUIRE(fs::equivalent(result, f));
}

TEST_CASE("GetPhysicalPath strips \\\\?\\ extended-length prefix", "[t3]") {
    TempDir td;
    auto f = td.path() / "test.md";
    std::ofstream(f) << "content";
    
    auto extended = std::wstring(L"\\\\?\\") + f.wstring();
    auto result = GetPhysicalPath(extended);
    REQUIRE_FALSE(result.starts_with(L"\\\\?\\"));
    REQUIRE(fs::equivalent(fs::path(result), f));
}

TEST_CASE("GetPhysicalPathForLink returns original path when input does not exist", "[t3]") {
    auto bogus = L"C:\\this\\path\\does\\not\\exist\\file.txt";
    auto result = GetPhysicalPathForLink(bogus);
    REQUIRE(result == bogus);
}

TEST_CASE("ForcedHtmlExt: .xml file triggers temp-copy with .html extension", "[t3]") {
    TempDir td;
    auto xmlFile = td.path() / "test.xml";
    std::ofstream(xmlFile) << "<root>content</root>";
    
    auto result = GetPhysicalPath(xmlFile);
    
    SECTION("result is in the system temp directory, not the source path") {
        REQUIRE_FALSE(fs::path(result).parent_path() == xmlFile.parent_path());
    }
    SECTION("result ends with .html extension") {
        REQUIRE(fs::path(result).extension() == ".html");
    }
    SECTION("temp copy contains the same contents") {
        std::ifstream src(xmlFile), dst(result);
        std::string srcContent((std::istreambuf_iterator<char>(src)), std::istreambuf_iterator<char>());
        std::string dstContent((std::istreambuf_iterator<char>(dst)), std::istreambuf_iterator<char>());
        REQUIRE(srcContent == dstContent);
    }
}

TEST_CASE("ForcedHtmlExt does not trigger for .txt file", "[t3]") {
    TempDir td;
    auto txtFile = td.path() / "test.txt";
    std::ofstream(txtFile) << "just text";
    
    auto result = GetPhysicalPath(txtFile);
    REQUIRE(fs::path(result).extension() == ".txt");
    REQUIRE(fs::equivalent(fs::path(result), txtFile));
}

TEST_CASE("GenTempFile + RemoveTempFiles lifecycle", "[t3]") {
    TempDir td;
    auto xmlFile = td.path() / "lifecycle.xml";
    std::ofstream(xmlFile) << "<data/>";
    
    auto result = GetPhysicalPath(xmlFile);
    REQUIRE(fs::exists(result));  // temp file was created
    
    RemoveTempFiles();
    
    REQUIRE_FALSE(fs::exists(result));  // temp file was removed
}
