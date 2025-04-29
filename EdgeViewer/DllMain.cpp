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
#include <system_error>

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
	else if (reason == DLL_PROCESS_DETACH && atoi(gs_Ini()["Chromium"]["CleanupOnExit"].c_str()))
	{
		auto userDirFinal = expandPath(to_utf16(gs_Ini()["Chromium"]["UserDir"]));
		fs::remove_all(fs::path(userDirFinal) / L"EBWebView");
		removeTempFiles();
	}

	return TRUE;
}
//------------------------------------------------------------------------
bool ZoomHotkeyHandled(ICoreWebView2Controller* ctrl, UINT key)
{
	if ((key == VK_OEM_PLUS || key == VK_OEM_MINUS) && (GetKeyState(VK_CONTROL) & 0x8000)) 
	{
		double zoom;
		double delta = key == VK_OEM_PLUS ? ZOOMDELTA : -ZOOMDELTA;
		ctrl->get_ZoomFactor(&zoom);
		gs_ZoomFactor = zoom + delta;
		ctrl->put_ZoomFactor(gs_ZoomFactor);
		return true;
	}

	return false;
}
//------------------------------------------------------------------------
void SetColorProfile(ViewPtr webview)
{
	wil::com_ptr<ICoreWebView2_13> webView2_13;
	webView2_13 = webview.try_query<ICoreWebView2_13>();
	wil::com_ptr<ICoreWebView2Profile> profile;
	webView2_13->get_Profile(&profile);
	profile->put_PreferredColorScheme(gs_isDarkMode ? COREWEBVIEW2_PREFERRED_COLOR_SCHEME_DARK : COREWEBVIEW2_PREFERRED_COLOR_SCHEME_LIGHT);
}
//------------------------------------------------------------------------
HRESULT CreateWebView2Environment(HWND hWnd, const std::wstring& fileToLoad)
{
	auto userDirFinal = expandPath(to_utf16(gs_Ini()["Chromium"]["UserDir"]));
	auto switches = gs_Ini()["Chromium"]["Switches"];
	auto execFolder = gs_Ini()["Chromium"][BROWSER_FOLDER_KEY];

	wchar_t* pBrowserExecFolder = nullptr;
	std::wstring execFolderFinal;

	if (!execFolder.empty())
	{
		execFolderFinal = expandPath(to_utf16(execFolder));
		pBrowserExecFolder = &execFolderFinal[0];
	}

	// switches are plain ASCII, so this wstring conversion is acceptable
	auto options = Make<CoreWebView2EnvironmentOptions>();
	options->put_AdditionalBrowserArguments(std::wstring(std::begin(switches), std::end(switches)).c_str());
	
	return CreateCoreWebView2EnvironmentWithOptions(pBrowserExecFolder, &userDirFinal[0], options.Get(),
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

						if (atoi(gs_Ini()["Chromium"]["KeepZoom"].c_str()))
							controller->put_ZoomFactor(gs_ZoomFactor);


						// disable browser hotkeys (they conflict with the lister interface)
						wil::com_ptr<ICoreWebView2Settings> settings;
						webview->get_Settings(&settings);
						auto settings23 = settings.try_query<ICoreWebView2Settings3>();
						settings23->put_AreBrowserAcceleratorKeysEnabled(FALSE);

						SetColorProfile(webview);

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

									if (ZoomHotkeyHandled(controller, key))	// don't pass the hotkey if it was already handled here
										;
									else
									{
										PostMessage(hWnd, WM_WEBVIEW_KEYDOWN, key, 0);
									}
								}

								return S_OK;
							}).Get(), &token);


						webview->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
							[=](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args)
							{
								wil::unique_cotaskmem_string message;
								args->get_WebMessageAsJson(&message);
								PostMessage(hWnd, WM_WEBVIEW_JS_KEYDOWN, std::stoi(message.get()), 0);
								return S_OK;
							}).Get(), &token);


						webview->AddScriptToExecuteOnDocumentCreated(
							L"window.addEventListener('keydown', event => { window.chrome.webview.postMessage(event.keyCode); });",
							nullptr);

						if (atoi(gs_Ini()["Chromium"]["OfflineMode"].c_str()))	// block everything that starts with http(s)://
						{
							webview->AddWebResourceRequestedFilter(L"http://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
							webview->AddWebResourceRequestedFilter(L"https://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
						}

						webview->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
							[=](ICoreWebView2* webview, ICoreWebView2WebResourceRequestedEventArgs* args)
							{
								wil::com_ptr<ICoreWebView2WebResourceResponse> response;
								wil::com_ptr<ICoreWebView2Environment> environment;
								wil::com_ptr<ICoreWebView2_2> webview2;
								webview->QueryInterface(IID_PPV_ARGS(&webview2));
								webview2->get_Environment(&environment);
								environment->CreateWebResourceResponse(nullptr, 403 /*NoContent*/, L"Blocked", L"", &response);
								args->put_Response(response.get());
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
	if (!gsProcRegistry().CanLoad(FileToLoad))
		return NULL;

	gs_isDarkMode = ShowFlags & lcp_darkmode;
	HWND hWnd = CreateWindowExA(0, EDGE_LISTER_CLASS, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
								0, 0, 0, 0, ParentWin, NULL, gs_pluginInstance, NULL);

	if (!SUCCEEDED(CreateWebView2Environment(hWnd, FileToLoad)))
	{
		if (atoi(gs_Ini()["Chromium"]["ShowErrorBoxes"].c_str()))
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

	gs_isDarkMode = ShowFlags & lcp_darkmode;
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
	std::vector<std::string> secs = { "HTML", "Markdown", "AsciiDoc", "URL", "MHTML" };

	const auto& extIni = gs_Ini().get("Extensions");
	auto exts = "EXT=\"" + extIni.get(secs[0]);
	
	for(auto v = secs.begin() + 1; v != secs.end(); ++v)
		exts += "," + extIni.get(*v);
	
	if (atoi(extIni.get("Dirs").c_str()))
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
