#include "Globals.h"
#include "Navigator.h"
#include "Processors/ProcessorRegistry.h"
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
HRESULT CreateWebView2Environment(HWND hWnd, const std::wstring& fileToLoad)
{
	auto userDir = gs_Ini["Chromium"]["UserDir"];
	auto switches = gs_Ini["Chromium"]["Switches"];

	wchar_t userDirFinal[MAX_PATH];
	ExpandEnvironmentStrings(to_utf16(userDir).c_str(), userDirFinal, MAX_PATH); // so we can use any %ENV_VAR%

	// switches are plain ASCII, so this wstring conversion is acceptable
	auto options = Make<CoreWebView2EnvironmentOptions>();
	options->put_AdditionalBrowserArguments(std::wstring(std::begin(switches), std::end(switches)).c_str());
	
	return CreateCoreWebView2EnvironmentWithOptions(nullptr, userDirFinal, options.Get(),
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
		[=](HRESULT result, ICoreWebView2Environment* env)
		{
			RETURN_IF_FAILED(result);

			env->CreateCoreWebView2Controller(hWnd,
				Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
				[=](HRESULT result, ICoreWebView2Controller* controller)
				{
					RETURN_IF_FAILED(result);

					ViewPtr webview;
					controller->get_CoreWebView2(&webview);

					// disable browser hotkeys (they conflict with the lister interface)
					wil::com_ptr<ICoreWebView2Settings> settings;
					webview->get_Settings(&settings);
					auto settings23 = settings.try_query<ICoreWebView2Settings3>();
					settings23->put_AreBrowserAcceleratorKeysEnabled(FALSE);

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

					if (GetFocus() == hWnd)
						controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);

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
HWND __stdcall ListLoadW(HWND ParentWin, const wchar_t* FileToLoad, int ShowFlags)
{
	if (!ProcessorRegistry::CanLoad(FileToLoad))
		return NULL;

	HWND hWnd = CreateWindowExA(0, EDGE_LISTER_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
								0, 0, 0, 0, ParentWin, NULL, gs_pluginInstance, NULL);

	if (!SUCCEEDED(CreateWebView2Environment(hWnd, FileToLoad)))
	{
		MessageBox(hWnd, std::format(L"Cannot create WebView2. Error code: {}", GetLastError()).c_str(), L"Error", MB_ICONERROR);
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
	if (!ProcessorRegistry::CanLoad(FileToLoad))
		return LISTPLUGIN_ERROR;

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
	auto iniPath = fs::path(GetModulePath()) / INI_NAME;
	mINI::INIFile file(to_utf8(iniPath));
	file.read(gs_Ini);
}
//------------------------------------------------------------------------
