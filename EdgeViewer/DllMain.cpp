#include "Globals.h"
#include "Navigator.h"
#include "Processors/ProcessorRegistry.h"
#include "EdgeLister.h"
#include "WlxDetect.h"

#include <string>
#include <cstring>
#include <cstdint>
#include <format>
#include <map>
#include <memory>

#ifdef _WIN32
#include "WebView/WebViewFactory.h"
#include "Log.h"

#include <windows.h>
#include <tchar.h>
#include <fstream>
#include <regex>
#endif

//------------------------------------------------------------------------
ListDefaultParamStruct gs_Config;
//------------------------------------------------------------------------

#ifdef _WIN32
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
#endif
//------------------------------------------------------------------------
// Returns the IWebView* registered for a lister handle, or nullptr if
// the lister has been closed (its entry already erased from gs_Views).
// Decision 7 (Spike 2 — confirmed): TC's WLX callbacks fire on the
// same thread that owns the lister — we can call Navigator directly
// without the WM_COPYDATA indirection.
static IWebView* FindBackend(void* ListWin)
{
	auto it = gs_Views.find(ListWin);
	return it != gs_Views.end() ? it->second.get() : nullptr;
}
//------------------------------------------------------------------------
#ifdef _WIN32
//------------------------------------------------------------------------
// TOTAL COMMANDER FUNCTIONS
//------------------------------------------------------------------------
HWND __stdcall ListLoadW(HWND ParentWin, const wchar_t* FileToLoad, int ShowFlags)
{
	auto processor = gsProcRegistry().FindProcessor(FileToLoad);

	if (!processor)
		return nullptr;

	gs_IsDarkMode = ShowFlags & lcp_darkmode;
	HWND hWnd = CreateWindowExA(0, EDGE_LISTER_CLASS, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
								0, 0, 0, 0, ParentWin, nullptr, gs_PluginInstance, nullptr);

	if (!hWnd)
		return nullptr;

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

#else // _WIN32

//------------------------------------------------------------------------
// TOTAL COMMANDER FUNCTIONS (Linux)
// Double Commander loads the plugin via dlsym, so every export must be
// extern "C". The lister handles are the GtkWidget* DC passes in — kept
// as void* here to avoid dragging GTK types into this shared file.
//------------------------------------------------------------------------
// DC's WLX interface passes filenames as PWideChar, which on Linux is a
// UTF-16 string (2-byte code units, like Windows), while GCC's wchar_t
// is 32-bit. Reading the buffer as wchar_t merges two code units into
// one invalid codepoint (std::wstring_convert would throw range_error).
// Decode the 16-bit units into a real wstring of Unicode codepoints.
static std::wstring FromDcWide(const wchar_t* s)
{
	std::wstring out;
	if (!s)
		return out;
	const char16_t* u = reinterpret_cast<const char16_t*>(s);
	while (uint32_t c = *u++)
	{
		if (c >= 0xD800 && c <= 0xDBFF)
		{
			const uint32_t lo = *u++;
			if (lo >= 0xDC00 && lo <= 0xDFFF)
				c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
		}
		out += static_cast<wchar_t>(c);
	}
	return out;
}

//------------------------------------------------------------------------
// Shared body of ListLoadW / ListLoad (narrow variant decodes first).
static void* DoListLoad(void* ParentWin, const std::wstring& wfile, int ShowFlags)
{
	auto processor = gsProcRegistry().FindProcessor(fs::path(to_utf8(wfile)));

	if (!processor)
		return nullptr;

	gs_IsDarkMode = ShowFlags & lcp_darkmode;

	void* pluginWin = EdgeLister::Create(ParentWin, wfile, processor);
	if (!pluginWin)
		return nullptr;

	return pluginWin;
}
//------------------------------------------------------------------------
extern "C" void* ListLoadW(void* ParentWin, const wchar_t* FileToLoad, int ShowFlags)
{
	return DoListLoad(ParentWin, FromDcWide(FileToLoad), ShowFlags);
}
//------------------------------------------------------------------------
extern "C" void* ListLoad(void* ParentWin, const char* FileToLoad, int ShowFlags)
{
	return DoListLoad(ParentWin, to_utf16(FileToLoad), ShowFlags);
}
//------------------------------------------------------------------------
extern "C" int ListLoadNextW(void* ParentWin, void* ListWin, const wchar_t* FileToLoad, int ShowFlags)
{
	const auto wfile = FromDcWide(FileToLoad);

	if (!gsProcRegistry().FindProcessor(fs::path(to_utf8(wfile))))
		return LISTPLUGIN_ERROR;

	gs_IsDarkMode = ShowFlags & lcp_darkmode;
	EdgeLister::OpenIn(ListWin, wfile);
	return LISTPLUGIN_OK;
}
//------------------------------------------------------------------------
extern "C" int ListLoadNext(void* ParentWin, void* ListWin, const char* FileToLoad, int ShowFlags)
{
	const auto wfile = to_utf16(FileToLoad);

	if (!gsProcRegistry().FindProcessor(fs::path(to_utf8(wfile))))
		return LISTPLUGIN_ERROR;

	gs_IsDarkMode = ShowFlags & lcp_darkmode;
	EdgeLister::OpenIn(ListWin, wfile);
	return LISTPLUGIN_OK;
}
//------------------------------------------------------------------------
extern "C" void ListCloseWindow(void* ListWin)
{
	if (gs_Views.find(ListWin) != gs_Views.end())
	{
		gs_Views[ListWin]->Close();
		gs_Views.erase(ListWin);
	}
	// No WM_CLOSE on Linux: WebKitBackend::Close already destroys the
	// embedded WebView widget.
}
//------------------------------------------------------------------------
extern "C" void ListGetDetectString(char* DetectString, int maxlen)
{
	// called after ListSetDefaultParams(), so the ini file should be OK
	if (maxlen <= 0)
		return;
	auto str = BuildDetectString(GlobalSettings());
	strncpy(DetectString, str.c_str(), maxlen - 1);
	DetectString[maxlen - 1] = '\0';
}
//------------------------------------------------------------------------
extern "C" int ListSearchTextW(void* ListWin, const wchar_t* SearchString, int SearchParameter)
{
	auto pattern = FromDcWide(SearchString);
	if (IWebView* webView = FindBackend(ListWin))
		Navigator(*webView).Search(pattern, SearchParameter);
	return LISTPLUGIN_OK;
}
//------------------------------------------------------------------------
extern "C" int ListSearchText(void* ListWin, const char* SearchString, int SearchParameter)
{
	auto pattern = to_utf16(SearchString);
	if (IWebView* webView = FindBackend(ListWin))
		Navigator(*webView).Search(pattern, SearchParameter);
	return LISTPLUGIN_OK;
}
//------------------------------------------------------------------------
extern "C" int ListPrintW(void* ListWin, const wchar_t* FileToPrint, const wchar_t* DefPrinter, int PrintFlags, void* Margins)
{
	if (IWebView* webView = FindBackend(ListWin))
		Navigator(*webView).Print();
	return LISTPLUGIN_OK;
}
//------------------------------------------------------------------------
extern "C" int ListPrint(void* ListWin, const char* FileToPrint, const char* DefPrinter, int PrintFlags, void* Margins)
{
	return ListPrintW(ListWin, nullptr, nullptr, PrintFlags, Margins);
}
//------------------------------------------------------------------------
extern "C" void ListSetDefaultParams(ListDefaultParamStruct* dps)
{
}
//------------------------------------------------------------------------
extern "C" int ListSendCommand(void* ListWin, int Command, int Parameter)
{
	return 0;
}
//------------------------------------------------------------------------

#endif // _WIN32
