#pragma once

#include <windows.h>
#include <string>
#include <format>
#include <stringapiset.h>

//------------------------------------------------------------------------
// Simple thread-safe append logger. Writes UTF-8 lines (with BOM on the
// first line) to %LOCALAPPDATA%\EdgeViewer\edgeviewer.log. TC is a GUI
// process with no stdout, so a file is the only reliable channel.
//
// Usage:
//   LogInit();                   // DllMain DLL_PROCESS_ATTACH
//   LogLine("hello {}", 42);     // anywhere
//   LogShutdown();               // DllMain DLL_PROCESS_DETACH (flushes)
//
// Lines are prefixed with [HH:MM:SS.mmm pid/tid] for ordering.
namespace Log
{
	void Init();
	void Shutdown();

	void Write(const std::wstring& line);   // thread-safe append

	// Args are taken by value so std::make_wformat_args sees them as
	// rvalues (it stores by-reference). std::forward is unnecessary.
	template <typename... Args>
	void Line(const std::wstring& fmt, Args... args)
	{
		try
		{
			std::wstring body = std::vformat(fmt, std::make_wformat_args(args...));
			Write(std::move(body));
		}
		catch (...)
		{
			// never throw out of a log call
		}
	}

	inline std::wstring HResultHex(HRESULT hr)
	{
		return std::format(L"0x{:08X}", static_cast<unsigned long>(hr));
	}

	// Resolved log file path (empty if Init() could not determine it).
	std::wstring CurrentPath();
}
//------------------------------------------------------------------------