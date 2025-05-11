#include "Globals.h"
#include "Navigator.h"
#include <wrl.h>
#include <webview2environmentoptions.h>
#include <mutex>
#include <regex>

using namespace Microsoft::WRL;
//------------------------------------------------------------------------
std::mutex gs_ViewCreateLock;
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
	profile->put_PreferredColorScheme(gs_IsDarkMode ? COREWEBVIEW2_PREFERRED_COLOR_SCHEME_DARK : COREWEBVIEW2_PREFERRED_COLOR_SCHEME_LIGHT);
}
//------------------------------------------------------------------------
void AddAccleratorKeyHandler(ICoreWebView2Controller* controller, HWND hWnd)
{
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
}
//------------------------------------------------------------------------
void DisableBrowserHotkeys(ViewPtr webview)
{
	wil::com_ptr<ICoreWebView2Settings> settings;
	webview->get_Settings(&settings);
	auto settings23 = settings.try_query<ICoreWebView2Settings3>();
	settings23->put_AreBrowserAcceleratorKeysEnabled(FALSE);
}
//------------------------------------------------------------------------
void AddApplyStyleScript(ViewPtr webview)
{
	// apply script to HTML documents loaded via ExecuteScript()
	// (checking using window.location.href)
	const auto& htmlIni = GlobalSettings().get("HTML");
	const auto cssFile = gs_IsDarkMode ? htmlIni.get("CSSDark") : htmlIni.get("CSS");
	const auto cssUrl = L"http://assets.example/html/" + to_utf16(cssFile);

	webview->AddScriptToExecuteOnDocumentCreated(std::format(LR"(
							window.addEventListener('DOMContentLoaded', () => {{
							if (window.location.href.toLowerCase().startsWith('http://local.example')) {{							
							const link = document.createElement('link');
							link.rel = 'stylesheet';
							link.href = '{}';
							(document.head || document.documentElement).appendChild(link);}}
							}});)", cssUrl).c_str(), nullptr);
}
//------------------------------------------------------------------------
void AddOfflineModeHandling(ViewPtr webview)
{
	EventRegistrationToken token;

	webview->AddWebResourceRequestedFilter(L"http://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
	webview->AddWebResourceRequestedFilter(L"https://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

	webview->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
		[=](ICoreWebView2* webview, ICoreWebView2WebResourceRequestedEventArgs* args)
		{
			wil::com_ptr<ICoreWebView2WebResourceRequest> request;
			wil::unique_cotaskmem_string uri;

			args->get_Request(&request);
			request->get_Uri(&uri);

			std::wstring ws_uri = uri.get();

			// starts with http(s), must block
			// (just for the future; currently we only filter such URIs)
			if (ws_uri.starts_with(L"http://") || ws_uri.starts_with(L"https://"))
			{
				wil::com_ptr<ICoreWebView2WebResourceResponse> response;
				wil::com_ptr<ICoreWebView2Environment> environment;
				wil::com_ptr<ICoreWebView2_2> webview2;
				webview->QueryInterface(IID_PPV_ARGS(&webview2));
				webview2->get_Environment(&environment);
				environment->CreateWebResourceResponse(nullptr, 403 /*NoContent*/, L"Blocked", L"", &response);
				args->put_Response(response.get());
			}

			return S_OK;

		}).Get(), &token);
}
//------------------------------------------------------------------------
HRESULT CreateWebView2Environment(HWND hWnd, const std::wstring& fileToLoad)
{
	auto userDirFinal = ExpandEnv(to_utf16(GlobalSettings()["Chromium"]["UserDir"]));
	auto switches = GlobalSettings()["Chromium"]["Switches"];
	auto execFolder = GlobalSettings()["Chromium"][BROWSER_FOLDER_KEY];

	wchar_t* pBrowserExecFolder = nullptr;
	std::wstring execFolderFinal;

	if (!execFolder.empty())
	{
		execFolderFinal = ExpandEnv(to_utf16(execFolder));
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

							if (to_int(GlobalSettings()["Chromium"]["KeepZoom"]))
								controller->put_ZoomFactor(gs_ZoomFactor);

							DisableBrowserHotkeys(webview); // they conflict with the lister interface
							SetColorProfile(webview);
							AddAccleratorKeyHandler(controller, hWnd);

							EventRegistrationToken token;
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

							AddApplyStyleScript(webview);

							if (to_int(GlobalSettings()["Chromium"]["OfflineMode"]))
								AddOfflineModeHandling(webview);

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
