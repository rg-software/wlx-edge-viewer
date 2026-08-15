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
ListDefaultParamStruct gs_Config;
//------------------------------------------------------------------------
BOOL APIENTRY DllMain(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		gs_PluginInstance = hinst;
		Log::Init();
		Log::Line(L"DLL_PROCESS_ATTACH logFile={}", Log::CurrentPath());
		EdgeLister::RegisterClass(hinst);
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		Log::Line(L"DLL_PROCESS_DETACH");
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
	Log::Line(L"ListLoadW: file={} showFlags=0x{:X} parent=0x{:X}",
		std::wstring(FileToLoad ? FileToLoad : L"<null>"),
		ShowFlags, reinterpret_cast<uintptr_t>(ParentWin));

	auto processor = gsProcRegistry().FindProcessor(FileToLoad);

	if (!processor)
	{
		Log::Line(L"ListLoadW: no processor matched extension for {}", std::wstring(FileToLoad ? FileToLoad : L"<null>"));
		return nullptr;
	}

	gs_IsDarkMode = ShowFlags & lcp_darkmode;
	HWND hWnd = CreateWindowExA(0, EDGE_LISTER_CLASS, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
								0, 0, 0, 0, ParentWin, nullptr, gs_PluginInstance, nullptr);

	if (!hWnd)
	{
		Log::Line(L"ListLoadW: CreateWindowExA FAILED le={}", GetLastError());
		return nullptr;
	}

#ifdef _WIN32
	auto webView = CreateWebView(hWnd, FileToLoad, processor);
	if (!webView)
	{
		Log::Line(L"ListLoadW: CreateWebView returned null for {}", std::wstring(FileToLoad ? FileToLoad : L"<null>"));
		if (to_int(GlobalSettings()["WebView"]["ShowErrorBoxes"]))
		{
			wchar_t msgbuf[512];
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, GetLastError(),
					  	  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msgbuf, (sizeof(msgbuf) / sizeof(wchar_t)), nullptr);

			wchar_t fullMsg[1024];
			swprintf_s(fullMsg,
				L"%s\n\nWebView2 initialization failed. See log for the actual HRESULT:\n%s",
				msgbuf, Log::CurrentPath().c_str());
			MessageBox(hWnd, fullMsg, L"EdgeViewer: cannot create WebView2", MB_ICONERROR);
		}
		DestroyWindow(hWnd);
		hWnd = NULL;
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
