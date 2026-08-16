#include "Globals.h"
#include "Navigator.h"
#include "Processors/ProcessorRegistry.h"
#include "EdgeLister.h"
#include "WlxDetect.h"
#include "WebView/WebViewFactory.h"
#include "Log.h"

#include <windows.h>
#include <tchar.h>
#include <string>
#include <format>
#include <map>
#include <fstream>
#include <regex>
#include <memory>
#include <wrl.h>
#include <wil/com.h>

using namespace Microsoft::WRL;
//------------------------------------------------------------------------
// OpenSpec task 5.1: log the caller's thread ID and the lister HWND's
// owning thread ID for each WLX entry point. The result determines
// whether we can retire WM_COPYDATA and call Navigator directly (5.2)
// or keep WM_COPYDATA (5.3). See design.md Decision 7.
static void LogThreadAffinity(const wchar_t* where, HWND hLister)
{
	const DWORD callTid = GetCurrentThreadId();
	DWORD ownerTid = 0;
	if (hLister)
		ownerTid = GetWindowThreadProcessId(hLister, nullptr);
	Log::Line(L"spike2: {} callerTid={} listerTid={} listerHwnd=0x{:X}",
		where, callTid, ownerTid, reinterpret_cast<uintptr_t>(hLister));
}

//------------------------------------------------------------------------
ListDefaultParamStruct gs_Config;
//------------------------------------------------------------------------
BOOL APIENTRY DllMain(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		gs_PluginInstance = hinst;
		Log::Init();
		EdgeLister::RegisterClass(hinst);
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		Log::Shutdown();
		if (to_int(GlobalSettings()["WebView"]["CleanupOnExit"]))
		{
			auto userDirFinal = ExpandEnv(to_utf16(GlobalSettings()["WebView"]["UserDir"]));
			fs::remove_all(fs::path(userDirFinal) / L"EBWebView");
			RemoveTempFiles();
		}
	}

	return TRUE;
}
//------------------------------------------------------------------------
void SendCommand(HWND hWndReceiver, HWND hWndSender, ULONG command, const std::wstring& data)
{
	COPYDATASTRUCT cds;
	cds.dwData = command;
	cds.cbData = DWORD(sizeof(wchar_t) * (data.length() + 1));	// payload is a single wstring
	cds.lpData = (void*)data.c_str();
	SendMessage(hWndReceiver, WM_COPYDATA, (WPARAM)hWndSender, (LPARAM)(LPVOID)&cds);
}
//------------------------------------------------------------------------
// TOTAL COMMANDER FUNCTIONS
//------------------------------------------------------------------------
HWND __stdcall ListLoadW(HWND ParentWin, const wchar_t* FileToLoad, int ShowFlags)
{
	LogThreadAffinity(L"ListLoadW", nullptr);

	auto processor = gsProcRegistry().FindProcessor(FileToLoad);

	if (!processor)
		return nullptr;

	gs_IsDarkMode = ShowFlags & lcp_darkmode;
	HWND hWnd = CreateWindowExA(0, EDGE_LISTER_CLASS, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
								0, 0, 0, 0, ParentWin, nullptr, gs_PluginInstance, nullptr);

	if (!hWnd)
		return nullptr;

#ifdef _WIN32
	// Queue WebView2 init. The synchronous return is whether the call was
	// queued; the async callback either populates gs_Views[hWnd] on
	// success or DestroyWindow(hWnd) on failure. Either way we return
	// the HWND to TC.
	HRESULT hr = CreateWebView(hWnd, FileToLoad, processor);
	if (FAILED(hr))
	{
		if (to_int(GlobalSettings()["WebView"]["ShowErrorBoxes"]))
		{
			wchar_t fullMsg[1024];
			swprintf_s(fullMsg,
				L"WebView2 setup could not be queued. See log for the actual HRESULT:\n%s",
				Log::CurrentPath().c_str());
			MessageBox(hWnd, fullMsg, L"EdgeViewer: cannot create WebView2", MB_ICONERROR);
		}
		DestroyWindow(hWnd);
		return nullptr;
	}
#endif

	return hWnd;
}
//------------------------------------------------------------------------
HWND __stdcall ListLoad(HWND ParentWin, const char* FileToLoad, int ShowFlags)
{
	return ListLoadW(ParentWin, to_utf16(FileToLoad).c_str(), ShowFlags);
}
//------------------------------------------------------------------------
int __stdcall ListLoadNextW(HWND ParentWin, HWND ListWin, const wchar_t* FileToLoad, int ShowFlags)
{
	LogThreadAffinity(L"ListLoadNextW", ListWin);

	if (!gsProcRegistry().FindProcessor(FileToLoad))
		return LISTPLUGIN_ERROR;

	gs_IsDarkMode = ShowFlags & lcp_darkmode;
	SendCommand(ListWin, ParentWin, CMD_NAVIGATE, FileToLoad);
	return LISTPLUGIN_OK;
}
//------------------------------------------------------------------------
int __stdcall ListLoadNext(HWND ParentWin, HWND ListWin, const char* FileToLoad, int ShowFlags)
{
	return ListLoadNextW(ParentWin, ListWin, to_utf16(FileToLoad).c_str(), ShowFlags);
}
//------------------------------------------------------------------------
void __stdcall ListCloseWindow(HWND ListWin)
{
	LogThreadAffinity(L"ListCloseWindow", ListWin);

	if (gs_Views.find(ListWin) != gs_Views.end())
	{
		gs_Views[ListWin]->Close();
		gs_Views.erase(ListWin);
	}
	PostMessage(ListWin, WM_CLOSE, 0, 0);
}
//------------------------------------------------------------------------
void __stdcall ListGetDetectString(char* DetectString, int maxlen)
{
	// called after ListSetDefaultParams(), so the ini file should be OK
	strcpy_s(DetectString, maxlen, BuildDetectString(GlobalSettings()).c_str());
}
//------------------------------------------------------------------------
int __stdcall ListSearchTextW(HWND ListWin, const wchar_t* SearchString, int SearchParameter)
{
	LogThreadAffinity(L"ListSearchTextW", ListWin);

	// let's save parameters before the string
	std::wstring toSend = std::format(L"{} {}", SearchParameter, SearchString);
	SendCommand(ListWin, GetParent(ListWin), CMD_SEARCH, toSend);
	return LISTPLUGIN_OK;
}
//------------------------------------------------------------------------
int __stdcall ListSearchText(HWND ListWin, const char* SearchString, int SearchParameter)
{
	return ListSearchTextW(ListWin, to_utf16(SearchString).c_str(), SearchParameter);
}
//------------------------------------------------------------------------
int __stdcall ListPrintW(HWND ListWin, const wchar_t* FileToPrint, const wchar_t* DefPrinter, int PrintFlags, RECT* Margins)
{
	LogThreadAffinity(L"ListPrintW", ListWin);

	SendCommand(ListWin, GetParent(ListWin), CMD_PRINT, L"");
	return LISTPLUGIN_OK;
}
//------------------------------------------------------------------------
int __stdcall ListPrint(HWND ListWin, const char* FileToPrint, const char* DefPrinter, int PrintFlags, RECT* Margins)
{
	return ListPrintW(ListWin, to_utf16(FileToPrint).c_str(), to_utf16(DefPrinter).c_str(), PrintFlags, Margins);
}
//------------------------------------------------------------------------
void __stdcall ListSetDefaultParams(ListDefaultParamStruct* dps)
{
}
//------------------------------------------------------------------------
int __stdcall ListSendCommand(HWND ListWin, int Command, int Parameter)
{
	return 0;
}
//------------------------------------------------------------------------
