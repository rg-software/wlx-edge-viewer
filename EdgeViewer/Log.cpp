#include "Log.h"
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

//------------------------------------------------------------------------
namespace Log
{
namespace
{
std::mutex g_mutex;
std::filesystem::path g_path;
bool g_initialized = false;
bool g_bomWritten = false;

//------------------------------------------------------------------------
std::filesystem::path ResolveLogPath()
{
	wchar_t localAppData[MAX_PATH] = {};
	if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData)))
		return {};

	std::filesystem::path dir = std::filesystem::path(localAppData) / L"EdgeViewer";
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	return dir / L"edgeviewer.log";
}

//------------------------------------------------------------------------
std::wstring TimestampPrefix()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	DWORD pid = GetCurrentProcessId();
	DWORD tid = GetCurrentThreadId();

	wchar_t buf[80];
	swprintf_s(buf, L"[%02u:%02u:%02u.%03u pid=%lu tid=%lu] ",
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, pid, tid);
	return buf;
}
} // anonymous namespace

//------------------------------------------------------------------------
void Init()
{
	std::scoped_lock lock(g_mutex);
	if (g_initialized)
		return;

	g_path = ResolveLogPath();
	g_initialized = true;

	// Truncate on each DLL load so logs don't grow forever.
	if (!g_path.empty())
	{
		std::ofstream ofs(g_path, std::ios::binary | std::ios::trunc);
		// UTF-8 BOM so Notepad / editors auto-detect.
		const char bom[] = { (char)0xEF, (char)0xBB, (char)0xBF };
		ofs.write(bom, 3);
		ofs.close();
		g_bomWritten = true;
	}
}

//------------------------------------------------------------------------
void Shutdown()
{
	std::scoped_lock lock(g_mutex);
	g_initialized = false;
	g_bomWritten = false;
	g_path.clear();
}

//------------------------------------------------------------------------
void Write(const std::wstring& line)
{
	if (!g_initialized)
		Init();

	std::scoped_lock lock(g_mutex);
	if (g_path.empty())
		return;

	std::wstring full = TimestampPrefix() + line + L"\r\n";

	int utf8Len = WideCharToMultiByte(CP_UTF8, 0, full.c_str(), (int)full.size(), nullptr, 0, nullptr, nullptr);
	if (utf8Len <= 0)
		return;

	std::string utf8;
	utf8.resize(utf8Len);
	WideCharToMultiByte(CP_UTF8, 0, full.c_str(), (int)full.size(), utf8.data(), utf8Len, nullptr, nullptr);

	// Append (open each time; the mutex serializes calls and avoids the
	// need for a long-lived file handle that could conflict with log
	// readers / tail tools).
	std::ofstream ofs(g_path, std::ios::binary | std::ios::app);
	if (ofs)
		ofs.write(utf8.data(), utf8.size());
}

//------------------------------------------------------------------------
std::wstring CurrentPath()
{
	std::scoped_lock lock(g_mutex);
	return g_path.wstring();
}
} // namespace Log
//------------------------------------------------------------------------