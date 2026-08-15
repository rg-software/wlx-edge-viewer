#include "WebViewFactory.h"
#include "Globals.h"
#include "Navigator.h"
#include "ZoomHotkey.h"
#include "Log.h"

#include "WebView/WebView2Backend.h"

#include <windows.h>
#include <webview2.h>
#include <webview2environmentoptions.h>
#include <wrl.h>
#include <wil/com.h>
#include <mutex>
#include <regex>

using namespace Microsoft::WRL;

namespace
{
std::mutex g_viewCreateLock;

void SetColorProfile(const wil::com_ptr<ICoreWebView2>& webview)
{
	auto wv13 = webview.try_query<ICoreWebView2_13>();
	wil::com_ptr<ICoreWebView2Profile> profile;
	wv13->get_Profile(&profile);
	profile->put_PreferredColorScheme(gs_IsDarkMode ? COREWEBVIEW2_PREFERRED_COLOR_SCHEME_DARK : COREWEBVIEW2_PREFERRED_COLOR_SCHEME_LIGHT);
}

void DisableBrowserHotkeys(const wil::com_ptr<ICoreWebView2>& webview)
{
	wil::com_ptr<ICoreWebView2Settings> settings;
	webview->get_Settings(&settings);
	auto settings23 = settings.try_query<ICoreWebView2Settings3>();
	settings23->put_AreBrowserAcceleratorKeysEnabled(FALSE);
}

void AddApplyStyleScript(const wil::com_ptr<ICoreWebView2>& webview)
{
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

bool ZoomHotkeyHandled(ICoreWebView2Controller* ctrl, UINT key)
{
	bool ctrlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	double currentZoom;
	ctrl->get_ZoomFactor(&currentZoom);
	double newZoom;
	if (::ZoomHotkeyHandled(key, ctrlHeld, currentZoom, newZoom))
	{
		if (newZoom != currentZoom)
			ctrl->put_ZoomFactor(newZoom);
		return true;
	}
	return false;
}

void AddAccleratorKeyHandler(ICoreWebView2Controller* controller, HWND hWnd)
{
	EventRegistrationToken token;
	controller->add_AcceleratorKeyPressed(Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
		[=](ICoreWebView2Controller* sender, ICoreWebView2AcceleratorKeyPressedEventArgs* args)
		{
			COREWEBVIEW2_KEY_EVENT_KIND kind;
			args->get_KeyEventKind(&kind);
			if (kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN)
			{
				UINT key;
				args->get_VirtualKey(&key);
				if (ZoomHotkeyHandled(controller, key))
					;
				else
					PostMessage(hWnd, WM_WEBVIEW_KEYDOWN, key, 0);
			}
			return S_OK;
		}).Get(), &token);
}

void ParseAndPostMessage(ICoreWebView2Controller* controller, HWND hWnd, const wil::unique_cotaskmem_string& message)
{
	std::wstring message_wstr(message.get());
	std::wregex regex(L"\\|");
	std::wsregex_token_iterator first{message_wstr.begin(), message_wstr.end(), regex, -1}, last;
	std::vector<std::wstring> tokens{first, last};

	if (tokens.size() > 1)
	{
		if (tokens[0] == L"CMD_KEY")
			PostMessage(hWnd, WM_WEBVIEW_JS_KEYDOWN, std::stoi(tokens[1]), 0);
		else if (tokens[0] == L"CMD_MENU")
		{
			COPYDATASTRUCT cds{};
			cds.dwData = CMD_MENU;
			cds.cbData = (DWORD)((tokens[1].length() + 1) * sizeof(wchar_t));
			cds.lpData = (PVOID)tokens[1].c_str();
			SendMessage(hWnd, WM_COPYDATA, (WPARAM)hWnd, (LPARAM)&cds);
		}
		else if (tokens[0] == L"CMD_ZOOM")
			controller->put_ZoomFactor(std::stod(tokens[1]));
	}
}

// Configure the WebView2 environment + controller for `hWnd`. The
// function returns the synchronous HRESULT of the async
// `CreateCoreWebView2EnvironmentWithOptions` call; the actual init
// completes in the nested callbacks. On async failure the callbacks
// destroy the HWND so TC doesn't keep an uninitialized lister window.
HRESULT QueueConfigureWebView2(HWND hWnd, const std::wstring& fileToLoad, const ProcessorInterface* processor)
{
	auto userDirFinal = ExpandEnv(to_utf16(GlobalSettings()["WebView"]["UserDir"]));
	Log::Line(L"WebView2 init start: hwnd=0x{:X} userDir={} file={}",
		reinterpret_cast<uintptr_t>(hWnd), userDirFinal, fileToLoad);

	return CreateCoreWebView2EnvironmentWithOptions(nullptr, userDirFinal.data(), nullptr,
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[hWnd, fileToLoad, processor](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
			{
				if (FAILED(result))
				{
					Log::Line(L"WebView2 CreateEnvironment FAILED hr={}", Log::HResultHex(result));
					DestroyWindow(hWnd);
					return result;
				}
				Log::Line(L"WebView2 CreateEnvironment OK");

				return env->CreateCoreWebView2Controller(hWnd,
					Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
						[hWnd, fileToLoad, processor](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
						{
							if (FAILED(result))
							{
								Log::Line(L"WebView2 CreateController FAILED hr={}", Log::HResultHex(result));
								DestroyWindow(hWnd);
								return result;
							}
							Log::Line(L"WebView2 CreateController OK");

							wil::com_ptr<ICoreWebView2> webview;
							controller->get_CoreWebView2(&webview);

							if (to_int(GlobalSettings()["WebView"]["KeepZoom"]) && gs_ZoomFactor.contains(processor))
								controller->put_ZoomFactor(gs_ZoomFactor[processor]);

							DisableBrowserHotkeys(webview);
							SetColorProfile(webview);
							AddAccleratorKeyHandler(controller, hWnd);

							EventRegistrationToken token;
							webview->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
								[=](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args)
								{
									wil::unique_cotaskmem_string message;
									args->TryGetWebMessageAsString(&message);
									ParseAndPostMessage(controller, hWnd, message);
									return S_OK;
								}).Get(), &token);

							webview->AddScriptToExecuteOnDocumentCreated(
								L"window.addEventListener('keydown', event => { if (event.code === 'KeyQ') window.chrome.webview.postMessage('CMD_KEY|81'); else if (event.code >= 'Digit1' && event.code <= 'Digit8') window.chrome.webview.postMessage('CMD_KEY|' + event.code.charCodeAt(5)); });",
								nullptr);

							AddApplyStyleScript(webview);

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

							// Construct the backend AFTER everything else is set up,
							// then move the shared_ptr into gs_Views. The map owns
							// the backend for the lifetime of the lister window
							// — when ListCloseWindow erases the entry, the COM refs
							// drop and the controller releases.
							auto backend = std::make_shared<WebView2Backend>(controller, webview);
							{
								std::scoped_lock lock(g_viewCreateLock);
								gs_Views[hWnd] = backend;
							}

							// Log every navigation event so we can see the render
							// sequence (about:blank -> loader -> JS DOM replace).
							webview->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
								[](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
								{
									wil::unique_cotaskmem_string uri;
									args->get_Uri(&uri);
									Log::Line(L"WebView2 NavigationStarting uri={}", std::wstring(uri.get()));
									return S_OK;
								}).Get(), &token);

							webview->add_ContentLoading(Callback<ICoreWebView2ContentLoadingEventHandler>(
								[](ICoreWebView2* sender, ICoreWebView2ContentLoadingEventArgs* args) -> HRESULT
								{
									Log::Line(L"WebView2 ContentLoading");
									return S_OK;
								}).Get(), &token);

							if (auto webview2 = webview.try_query<ICoreWebView2_2>())
							{
								webview2->add_DOMContentLoaded(Callback<ICoreWebView2DOMContentLoadedEventHandler>(
									[](ICoreWebView2* sender, ICoreWebView2DOMContentLoadedEventArgs* args) -> HRESULT
									{
										Log::Line(L"WebView2 DOMContentLoaded");
										return S_OK;
									}).Get(), &token);
							}

							webview->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
								[](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
								{
									BOOL ok = FALSE;
									args->get_IsSuccess(&ok);
									Log::Line(L"WebView2 NavigationCompleted success={}", ok ? L"true" : L"false");
									return S_OK;
								}).Get(), &token);

							Navigator nav(*backend);
							nav.Open(fileToLoad);

							if (GetFocus() == hWnd)
								controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);

							Log::Line(L"WebView2 init complete: hwnd=0x{:X}", reinterpret_cast<uintptr_t>(hWnd));
							return S_OK;
						}).Get());
			}).Get());
}
} // namespace

//------------------------------------------------------------------------
#ifdef _WIN32
HRESULT CreateWebView(void* parentWindow,
                       const std::wstring& fileToLoad,
                       const ProcessorInterface* processor)
{
	auto hWnd = static_cast<HWND>(parentWindow);
	if (!hWnd)
	{
		Log::Line(L"CreateWebView: null hwnd");
		return E_INVALIDARG;
	}

	std::scoped_lock lock(g_viewCreateLock);
	auto hr = QueueConfigureWebView2(hWnd, fileToLoad, processor);
	if (FAILED(hr))
	{
		Log::Line(L"CreateWebView sync failure hr={}", Log::HResultHex(hr));
		return hr;
	}

	Log::Line(L"CreateWebView sync OK (async init pending)");
	return S_OK;
}
#endif
//------------------------------------------------------------------------