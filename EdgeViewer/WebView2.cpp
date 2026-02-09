#include "Globals.h"
#include "Navigator.h"
#include <wrl.h>
#include <shlwapi.h>
#include <webview2environmentoptions.h>
#include <mutex>
#include <regex>

using namespace Microsoft::WRL;
//------------------------------------------------------------------------
std::mutex gs_ViewCreateLock;

//------------------------------------------------------------------------
bool ZoomHotkeyHandled(ICoreWebView2Controller* ctrl, UINT key)
{
	if ((key == VK_OEM_PLUS || key == VK_OEM_MINUS || key == '0') && (GetKeyState(VK_CONTROL) & 0x8000))
	{
		static const std::vector<double> zoomSteps = { 0.25, 0.33, 0.5, 0.67, 0.75, 0.8, 0.9, 1.0,
													   1.1, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0, 5.0 };

		double currentZoom;
		ctrl->get_ZoomFactor(&currentZoom);

		if (key == '0')
			ctrl->put_ZoomFactor(1.0);
		else if (key == VK_OEM_PLUS)
		{
			auto it = std::upper_bound(zoomSteps.begin(), zoomSteps.end(), currentZoom + 0.001);
			if (it != zoomSteps.end())
				ctrl->put_ZoomFactor(*it);
		}
		else
		{
			auto it = std::lower_bound(zoomSteps.begin(), zoomSteps.end(), currentZoom - 0.001);
			if (it != zoomSteps.begin())
				ctrl->put_ZoomFactor(*(--it));
		}

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
	// apply script to HTML documents loaded via Navigate()
	// (checking using window.location.href, which is empty in NavigateToString())

	const auto& htmlIni = GlobalSettings().get("HTML");
	const auto cssFile = gs_IsDarkMode ? htmlIni.get("CSSDark") : htmlIni.get("CSS");
	const auto cssUrl = L"http://assets.example/html/" + to_utf16(cssFile);

	webview->AddScriptToExecuteOnDocumentCreated(std::format(LR"(
							window.addEventListener('DOMContentLoaded', () => {{
							if (window.location.href.toLowerCase().startsWith('http://local.example') ||
							    window.location.href.toLowerCase().startsWith('http://html.example')) {{							
							const link = document.createElement('link');
							link.rel = 'stylesheet';
							link.href = '{}';
							(document.head || document.documentElement).appendChild(link);}}
							}});)", cssUrl).c_str(), nullptr);
}
//------------------------------------------------------------------------
void OverrideEncoding(const std::wstring& ws_uri, wil::com_ptr<ICoreWebView2Environment> environment, ICoreWebView2WebResourceRequestedEventArgs* args)
{
	auto path = ws_uri.substr((std::wstring(L"http://html.example/")).length());
	HtmlInfo info = gs_Htmls[path];
	gs_Htmls.erase(path);
	
	std::ifstream file(info.path, std::ios::binary);
	std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	wil::com_ptr<IStream> stream = SHCreateMemStream(reinterpret_cast<const BYTE*>(buffer.data()), (UINT)buffer.size());

	wil::com_ptr<ICoreWebView2WebResourceResponse> response;
	environment->CreateWebResourceResponse(
		stream.get(), 200, L"OK", (L"Content-Type: text/html; charset=" + info.encoding).c_str(), &response);

	args->put_Response(response.get());
}
//------------------------------------------------------------------------
void AddResourceRequestHandling(ViewPtr webview)
{
	EventRegistrationToken token;

	// it works for nonlocal resources only because SetVirtualHostNameToFolderMapping() 
	// bypasses events for the files served from the mapped folders

	webview->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

	webview->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
		[=](ICoreWebView2* webview, ICoreWebView2WebResourceRequestedEventArgs* args)
		{
			wil::com_ptr<ICoreWebView2WebResourceRequest> request;
			wil::unique_cotaskmem_string uri;
			wil::com_ptr<ICoreWebView2Environment> environment;
			wil::com_ptr<ICoreWebView2_2> webview2;
			webview->QueryInterface(IID_PPV_ARGS(&webview2));
			webview2->get_Environment(&environment);

			args->get_Request(&request);
			request->get_Uri(&uri);

			std::wstring ws_uri = uri.get();

			if (ws_uri.starts_with(L"http://html.example/"))
				OverrideEncoding(ws_uri, environment, args);
			else if (to_int(GlobalSettings()["Chromium"]["OfflineMode"]))
			{
				wil::com_ptr<ICoreWebView2WebResourceResponse> response;
				environment->CreateWebResourceResponse(nullptr, 403, L"Blocked", L"", &response);
				args->put_Response(response.get());
			}
			return S_OK;

		}).Get(), &token);
}
//------------------------------------------------------------------------
HRESULT CreateWebView2Environment(HWND hWnd, const std::wstring& fileToLoad, const ProcessorInterface* processor)
{
	auto userDirFinal = ExpandEnv(to_utf16(GlobalSettings()["Chromium"]["UserDir"]));
	auto switches = GlobalSettings()["Chromium"]["Switches"];
	auto execFolder = GlobalSettings()["Chromium"][BROWSER_FOLDER_KEY];

	wchar_t* pBrowserExecFolder = nullptr;
	std::wstring execFolderFinal;

	if (!execFolder.empty())
	{
		execFolderFinal = ExpandEnv(to_utf16(execFolder));
		pBrowserExecFolder = execFolderFinal.data();
	}

	// switches are plain ASCII, so this wstring conversion is acceptable
	auto options = Make<CoreWebView2EnvironmentOptions>();
	options->put_AdditionalBrowserArguments(std::wstring(std::begin(switches), std::end(switches)).c_str());

	return CreateCoreWebView2EnvironmentWithOptions(pBrowserExecFolder, userDirFinal.data(), options.Get(),
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

							if (to_int(GlobalSettings()["Chromium"]["KeepZoom"]) && gs_ZoomFactor.contains(processor))
								controller->put_ZoomFactor(gs_ZoomFactor[processor]);

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
							AddResourceRequestHandling(webview);

							controller->add_ZoomFactorChanged(Callback<ICoreWebView2ZoomFactorChangedEventHandler>(
								[=](ICoreWebView2Controller* sender, IUnknown* args)
								{
									double zoom_factor;
									sender->get_ZoomFactor(&zoom_factor);
									gs_ZoomFactor[processor] = zoom_factor;
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
