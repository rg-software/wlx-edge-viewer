#pragma once

#include <filesystem>
#include <string>

//------------------------------------------------------------------------
// Platform-agnostic surface for filesystem, environment, and temporary
// file helpers that previously lived in Globals.{h,cpp} as direct
// Win32 calls. Each implementation (Platform_Win.cpp / Platform_Linux.cpp)
// provides the platform-specific backing.
namespace fs = std::filesystem;

std::wstring GetModulePath();
std::wstring ExpandEnv(const std::wstring& path);
std::wstring GetPhysicalPath(const std::wstring& path);
std::wstring GetPhysicalPathForLink(const std::wstring& path);
std::wstring GenTempFile(const std::wstring& path, const std::wstring& ext);
void RemoveTempFiles();
//------------------------------------------------------------------------
