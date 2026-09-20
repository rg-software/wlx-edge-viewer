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
std::wstring GetPhysicalPathForLink(const fs::path& path);
std::wstring GetPhysicalPath(const fs::path& path);
// True when the (already-normalized) path lives on a network share rather
// than a local volume. On Windows this distinguishes UNC paths
// (\\server\share\...) which the WebView2 virtual-host mapping cannot serve
// (the renderer has no credentials for the share), so processors then route
// content through the host-side evh:// scheme. On Linux every mounted share
// is a local path and the ev:// handler reads host-side, so it is always
// false there.
bool IsNetworkPath(const fs::path& path);
std::wstring GenTempFile(const fs::path& path, const std::wstring& ext);
void RemoveTempFiles();
std::wstring PickFolder(const void* parentWindow, const std::wstring& defaultFolder = L"");	// empty string = user cancelled
//------------------------------------------------------------------------
