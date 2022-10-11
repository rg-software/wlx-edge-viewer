// TODO:
// 2) Markdown should be loaded with my previous approach (convert & load from string aka NavigateToString with base url)
// In the future it might be reasonable to use NavigateToString in all cases BUT now it would not have the right base url)
// 3) set focus on Webview only when there is a focus on our child window
// implement missing functions
// keypresses from webview
// markdown, etc.

#include "CommonTypes.h"
#include "Navigator.h"
#include "EdgeLister.h"
#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <wrl.h>
#include <webview2environmentoptions.h>
#include <string>
#include <format>
#include <map>
#include <mutex>
#include <fstream>

using namespace Microsoft::WRL;
//------------------------------------------------------------------------
HINSTANCE gs_pluginInstance;
ListDefaultParamStruct gs_Config;
std::mutex gs_ViewCreateLock;
ViewsMap gs_Views;
//------------------------------------------------------------------------
BOOL APIENTRY DllMain(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		gs_pluginInstance = hinst;
		EdgeLister::RegisterClass(hinst);
	}

	return TRUE;
}
//------------------------------------------------------------------------
HRESULT CreateWebView2Environment(HWND hWnd, const std::wstring& userDir, const std::wstring& fileToLoad)
{
	// TODO: read options from the ini file
	auto options = Make<CoreWebView2EnvironmentOptions>();
	//options->put_AdditionalBrowserArguments(L"--allow-file-access-from-files");

	return CreateCoreWebView2EnvironmentWithOptions(nullptr, userDir.c_str(), options.Get(),
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([=](HRESULT result, ICoreWebView2Environment* env)
		{
			env->CreateCoreWebView2Controller(hWnd,
				Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([=](HRESULT result, ICoreWebView2Controller* controller)
				{
					ViewPtr webview;
					controller->get_CoreWebView2(&webview);

					RECT bounds;
					GetClientRect(hWnd, &bounds);
					controller->put_Bounds(bounds);

					Navigator(webview).Open(fileToLoad);

					std::scoped_lock lock(gs_ViewCreateLock);
					gs_Views[hWnd] = ViewCtrlPtr(controller);

					return S_OK;	// add error checking (controller can be nullptr, for example)?
				}).Get());
			return S_OK;
		}).Get());
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
HWND __stdcall ListLoadW(HWND ParentWin, wchar_t* FileToLoad, int ShowFlags)
{
	HWND hWnd = CreateWindowExA(0, "mdLister", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 
								0, 0, 0, 0, ParentWin, NULL, gs_pluginInstance, NULL);

	auto iniDir = fs::path(gs_Config.DefaultIniName).parent_path();
	
	if (!SUCCEEDED(CreateWebView2Environment(hWnd, iniDir, FileToLoad)))
		MessageBox(hWnd, std::format(L"Cannot create WebView2. Error code: {}", GetLastError()).c_str(), L"Error", MB_ICONERROR);
	
	return hWnd;
}
//------------------------------------------------------------------------
int __stdcall ListLoadNextW(HWND ParentWin, HWND PluginWin, wchar_t* FileToLoad, int ShowFlags)
{
	if (HWND pluginWindow = FindWindowEx(ParentWin, NULL, L"mdLister", NULL))
	{
		SendCommand(pluginWindow, ParentWin, CMD_NAVIGATE, FileToLoad);
		return LISTPLUGIN_OK;
	}
	
	return LISTPLUGIN_ERROR;
}
//------------------------------------------------------------------------
void __stdcall ListCloseWindow(HWND ListWin)
{
	PostMessage(ListWin, WM_CLOSE, 0, 0);
	gs_Views.erase(ListWin);
}
//------------------------------------------------------------------------
void __stdcall ListGetDetectString(char* DetectString, int maxlen)
{
	// TODO: use ini file
	// copy it from the archive if it is not present
	strcpy_s(DetectString, maxlen, "EXT = \"HTM\" | EXT = \"HTML\" | EXT = \"MARKDOWN\"");

	// FROM AUDIOCONVERTER (SAMPLE)
	//gPluginIniPath = join_paths(get_dirname(std::string(dps->DefaultIniName)), std::string("audio-converter.ini"));

	//if (GetFileAttributesA(gPluginIniPath.c_str()) == INVALID_FILE_ATTRIBUTES)
	//{
	//	// copy from our archive
	//	std::string archiveIniPath = join_paths(GetModulePath(), std::string("audio-converter.ini"));
	//	CopyFileA(archiveIniPath.c_str(), gPluginIniPath.c_str(), FALSE);
	//}

	// then read possible extensions (markdown and html)

}
//------------------------------------------------------------------------
// TODO: implement
//ListSearchTextW
//ListPrintW
//------------------------------------------------------------------------
void __stdcall ListSetDefaultParams(ListDefaultParamStruct* dps)
{
	gs_Config = *dps;
}
//------------------------------------------------------------------------
