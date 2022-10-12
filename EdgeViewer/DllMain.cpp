#include "Globals.h"
#include "Navigator.h"
#include "EdgeLister.h"
#include <mini/ini.h>
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
#include <regex>

using namespace Microsoft::WRL;
//------------------------------------------------------------------------
ListDefaultParamStruct gs_Config;
std::mutex gs_ViewCreateLock;
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
	auto switches = gs_Ini["Chromium"]["Switches"];

	// switches are plain ASCII, so this wstring conversion is acceptable
	auto options = Make<CoreWebView2EnvironmentOptions>();
	options->put_AdditionalBrowserArguments(std::wstring(std::begin(switches), std::end(switches)).c_str());
	
	return CreateCoreWebView2EnvironmentWithOptions(nullptr, userDir.c_str(), options.Get(),
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
		[=](HRESULT result, ICoreWebView2Environment* env)
		{
			env->CreateCoreWebView2Controller(hWnd,
				Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
				[=](HRESULT result, ICoreWebView2Controller* controller)
				{
					ViewPtr webview;
					controller->get_CoreWebView2(&webview);

					EventRegistrationToken token;
					controller->add_AcceleratorKeyPressed(Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
						[=](ICoreWebView2Controller* sender, ICoreWebView2AcceleratorKeyPressedEventArgs* args)
						{
							COREWEBVIEW2_KEY_EVENT_KIND kind;
							args->get_KeyEventKind(&kind);
							
							// resend all key down events to the parent (EdgeLister window)
							if (kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN)
							{
								UINT key;
								args->get_VirtualKey(&key);

								PostMessage(hWnd, WM_WEBVIEW_KEYDOWN, key, 0);
							}

							return S_OK;
						}).Get(), &token);

					RECT bounds;
					GetClientRect(hWnd, &bounds);
					controller->put_Bounds(bounds);

					Navigator(webview).Open(fileToLoad);

					std::scoped_lock lock(gs_ViewCreateLock);
					gs_Views[hWnd] = ViewCtrlPtr(controller);

					return S_OK;	// add error checking (controller can be nullptr, e.g.)?
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
// TOTAL COMMANDER FUNCTIONS
//------------------------------------------------------------------------
HWND __stdcall ListLoadW(HWND ParentWin, wchar_t* FileToLoad, int ShowFlags)
{
	HWND hWnd = CreateWindowExA(0, EDGE_LISTER_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
								0, 0, 0, 0, ParentWin, NULL, gs_pluginInstance, NULL);

	auto iniDir = fs::path(gs_Config.DefaultIniName).parent_path();
	
	if (!SUCCEEDED(CreateWebView2Environment(hWnd, iniDir, FileToLoad)))
		MessageBox(hWnd, std::format(L"Cannot create WebView2. Error code: {}", GetLastError()).c_str(), L"Error", MB_ICONERROR);
	
	return hWnd;
}
//------------------------------------------------------------------------
int __stdcall ListLoadNextW(HWND ParentWin, HWND PluginWin, wchar_t* FileToLoad, int ShowFlags)
{
	if (HWND pluginWindow = FindWindowExA(ParentWin, NULL, EDGE_LISTER_CLASS, NULL))
	{
		SendCommand(pluginWindow, ParentWin, CMD_NAVIGATE, FileToLoad);
		return LISTPLUGIN_OK;
	}
	
	return LISTPLUGIN_ERROR;
}
//------------------------------------------------------------------------
void __stdcall ListCloseWindow(HWND ListWin)
{
	gs_Views[ListWin]->Close();
	gs_Views.erase(ListWin);
	PostMessage(ListWin, WM_CLOSE, 0, 0);
}
//------------------------------------------------------------------------
void __stdcall ListGetDetectString(char* DetectString, int maxlen)
{
	// called after ListSetDefaultParams(), so the ini file should be OK
	// convert ext1,ext2,ext3 into EXT="ext1"|EXT="ext2"|EXT="ext3"
	// TODO: double check this
	auto exts = "EXT=\"" + gs_Ini["Extensions"]["HTML"] + "," + gs_Ini["Extensions"]["Markdown"] + "\"";
	auto dstr = std::regex_replace(exts, std::regex(","), "|\"EXT=\"");
	strcpy_s(DetectString, maxlen, dstr.c_str());
}
//------------------------------------------------------------------------
// TODO: implement
//ListSearchTextW
//ListPrintW
//------------------------------------------------------------------------
void __stdcall ListSetDefaultParams(ListDefaultParamStruct* dps)
{
	gs_Config = *dps;

	// copy ini file from the plugin location to the suggested directory
	if (!fs::exists(gs_Config.OurIniPath()))
		fs::copy_file(fs::path(GetModulePath()) / INI_NAME, gs_Config.OurIniPath());

	mINI::INIFile file(to_utf8(gs_Config.OurIniPath()));
	file.read(gs_Ini);
}
//------------------------------------------------------------------------
