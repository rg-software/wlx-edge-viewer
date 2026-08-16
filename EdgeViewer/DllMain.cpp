#include "Globals.h"
#include "Navigator.h"
#include "Processors/ProcessorRegistry.h"
#include "EdgeLister.h"
#include "WlxDetect.h"
#include "WebView/WebViewFactory.h"
#include "Log.h"

#include <windows.h>
#include <tchar.h>
#include <cstdint>
#include <string>
#include <format>
#include <map>
#include <fstream>
#include <regex>
#include <memory>

//------------------------------------------------------------------------
// OpenSpec task 5.1: log the caller's thread ID and the lister HWND's
// owning thread ID for each WLX entry point. The result determines
// whether we can retire WM_COPYDATA and call Navigator directly (5.2)
// or keep WM_COPYDATA (5.3). See design.md Decision 7.
static void LogThreadAffinity(const wchar_t* where, intptr_t listerHwnd)
{
	const DWORD callTid = GetCurrentThreadId();
	DWORD ownerTid = 0;
	if (listerHwnd != 0)
		ownerTid = GetWindowThreadProcessId(reinterpret_cast<HWND>(listerHwnd), nullptr);
	Log::Line(L"spike2: {} callerTid={} listerTid={} listerHwnd=0x{:X}",
		where, callTid, ownerTid, static_cast<uintptr_t>(listerHwnd));
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
//------------------------------------------------------------------------
// TOTAL COMMANDER FUNCTIONS
//------------------------------------------------------------------------
HWND __stdcall ListLoadW(HWND ParentWin, const wchar_t* FileToLoad, int ShowFlags)
{
	LogThreadAffinity(L"ListLoadW", 0);

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
//------------------------------------------------------------------------
// Returns the IWebView* registered for a lister HWND, or nullptr if
// the lister has been closed (its entry already erased from gs_Views).
// Decision 7 (Spike 2 — confirmed): TC's WLX callbacks fire on the
// same thread that owns the lister — we can call Navigator directly
// without the WM_COPYDATA indirection.
static IWebView* FindBackend(HWND ListWin)
{
	auto it = gs_Views.find(ListWin);
	return it != gs_Views.end() ? it->second.get() : nullptr;
}

//------------------------------------------------------------------------
int __stdcall ListLoadNextW(HWND ParentWin, HWND ListWin, const wchar_t* FileToLoad, int ShowFlags)
{
	LogThreadAffinity(L"ListLoadNextW", reinterpret_cast<intptr_t>(ListWin));

	if (!gsProcRegistry().FindProcessor(FileToLoad))
		return LISTPLUGIN_ERROR;

	gs_IsDarkMode = ShowFlags & lcp_darkmode;
	if (IWebView* webView = FindBackend(ListWin))
		Navigator(*webView).Open(FileToLoad);
	// Falls through silently if the lister has been closed — TC may
	// re-invoke ListLoadNextW after ListCloseWindow during teardown.
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
	LogThreadAffinity(L"ListCloseWindow", reinterpret_cast<intptr_t>(ListWin));

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
	LogThreadAffinity(L"ListSearchTextW", reinterpret_cast<intptr_t>(ListWin));

	// Search parameters arrive after the search string; pack them
	// the same way the loader expects (parameter, then pattern).
	std::wstring toSend = std::format(L"{} {}", SearchParameter, SearchString);
	if (IWebView* webView = FindBackend(ListWin))
	{
		// Inline the parse: the old WM_COPYDATA path produced
		// `toSend` and handed it to Navigator::Search which then
		// re-parsed. Doing the parse here avoids the round-trip.
		size_t i = toSend.find_first_of(L' ');
		int params = std::stoi(toSend.substr(0, i));
		std::wstring pattern = toSend.substr(i + 1);
		Navigator(*webView).Search(pattern, params);
	}
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
	LogThreadAffinity(L"ListPrintW", reinterpret_cast<intptr_t>(ListWin));

	if (IWebView* webView = FindBackend(ListWin))
		Navigator(*webView).Print();
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
