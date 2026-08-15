#pragma once

#include <filesystem>
#include <string>
#include <atomic>

class TempDir {
public:
    TempDir() {
        static std::atomic<int> counter{0};
        auto base = std::filesystem::temp_directory_path();
        mPath = base / ("EVTEST_" + std::to_string(++counter));
        std::filesystem::create_directory(mPath);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(mPath, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    const std::filesystem::path& path() const { return mPath; }
private:
    std::filesystem::path mPath;
};