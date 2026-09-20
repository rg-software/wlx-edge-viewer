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

TEST_CASE("ForcedHtmlExt: .xml passes through unchanged (no relocation)", "[t3]") {
    TempDir td;
    auto xmlFile = td.path() / "test.xml";
    std::ofstream(xmlFile) << "<root>content</root>";
    
    auto result = GetPhysicalPath(xmlFile);
    
    SECTION("result points at the original file, not a temp copy") {
        REQUIRE(fs::path(result).parent_path() == xmlFile.parent_path());
        REQUIRE(fs::exists(result));
        REQUIRE(fs::equivalent(fs::path(result), xmlFile));
    }
    SECTION("original extension is preserved") {
        REQUIRE(fs::path(result).extension() == ".xml");
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
    
    auto temp = GenTempFile(xmlFile, L".html");
    REQUIRE(fs::exists(temp));  // temp file was created
    REQUIRE(fs::path(temp).extension() == ".html");
    
    RemoveTempFiles();
    
    REQUIRE_FALSE(fs::exists(temp));  // temp file was removed
}

TEST_CASE("IsNetworkPath classifies UNC vs local paths", "[t3]") {
    SECTION("plain UNC share path is a network path") {
        REQUIRE(IsNetworkPath(LR"(\\server\share\folder\file.md)"));
    }
    SECTION("extended-length UNC prefix is detected (pre-strip form)") {
        REQUIRE(IsNetworkPath(LR"(\\?\UNC\server\share\folder\file.md)"));
    }
    SECTION("local drive path is not a network path") {
        REQUIRE_FALSE(IsNetworkPath(LR"(C:\Users\test\readme.md)"));
    }
    SECTION("extended-length local path is not a network path") {
        REQUIRE_FALSE(IsNetworkPath(LR"(\\?\C:\Users\test\readme.md)"));
    }
    SECTION("relative path is not a network path") {
        REQUIRE_FALSE(IsNetworkPath(L"readme.md"));
    }
    SECTION("share root directory is a network path") {
        REQUIRE(IsNetworkPath(LR"(\\server\share)"));
    }
}
