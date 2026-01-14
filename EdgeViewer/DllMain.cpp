#include "Globals.h"
#include "Navigator.h"
#include "Processors/ProcessorRegistry.h"
#include "EdgeLister.h"
#include <mini/ini.h>
#include <windows.h>
#include <tchar.h>
#include <string>
#include <format>
#include <map>
#include <fstream>
#include <regex>

using namespace Microsoft::WRL;
//------------------------------------------------------------------------
ListDefaultParamStruct gs_Config;
HRESULT CreateWebView2Environment(HWND hWnd, const std::wstring& fileToLoad);
//------------------------------------------------------------------------
BOOL APIENTRY DllMain(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		gs_PluginInstance = hinst;
		EdgeLister::RegisterClass(hinst);
	}
	else if (reason == DLL_PROCESS_DETACH && to_int(GlobalSettings()["Chromium"]["CleanupOnExit"]))
	{
		auto userDirFinal = ExpandEnv(to_utf16(GlobalSettings()["Chromium"]["UserDir"]));
		fs::remove_all(fs::path(userDirFinal) / L"EBWebView");
		RemoveTempFiles();
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
	if (!gsProcRegistry().CanLoad(FileToLoad))
		return NULL;

	gs_IsDarkMode = ShowFlags & lcp_darkmode;
	HWND hWnd = CreateWindowExA(0, EDGE_LISTER_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
								0, 0, 0, 0, ParentWin, NULL, gs_PluginInstance, NULL);

	if (!SUCCEEDED(CreateWebView2Environment(hWnd, FileToLoad)))
	{
		if (to_int(GlobalSettings()["Chromium"]["ShowErrorBoxes"]))
		{
			wchar_t msgbuf[512];
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(),
					  	  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msgbuf, (sizeof(msgbuf) / sizeof(wchar_t)), NULL);
		
			MessageBox(hWnd, msgbuf, L"Error: cannot create WebView2", MB_ICONERROR);
		}
		DestroyWindow(hWnd);
		hWnd = NULL;
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
	if (!gsProcRegistry().CanLoad(FileToLoad))
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
	// convert ext1,ext2,ext3 into EXT="ext1"|EXT="ext2"|EXT="ext3"
	
	// NOTE(mm): all type sections should be listed here!
	std::vector<std::string> secs = { "HTML", "Markdown", "AsciiDoc", "URL", "MHTML", "RST", "Images", "Other" };

	const auto& extIni = GlobalSettings().get("Extensions");
	auto exts = "EXT=\"" + extIni.get(secs[0]);
	
	for(auto v = secs.begin() + 1; v != secs.end(); ++v)
		exts += "," + extIni.get(*v);
	
	if (to_int(extIni.get("Dirs")))
		exts += ",";	// directories match the empty extension

	exts += "\"";

	auto dstr = std::regex_replace(exts, std::regex(","), "\"|EXT=\"");
	strcpy_s(DetectString, maxlen, dstr.c_str());
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
