#include "WebViewFactory.h"
#include "Globals.h"
#include "Navigator.h"
#include "ZoomHotkey.h"
#include "Log.h"
#include "WebView/WebView2Backend.h"
#include "WebPolicy.h"
#include "EncodingList.h"
#include "Processors/ProcessorInterface.h"

#include <windows.h>
#include <webview2.h>

#include <format>
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

// [WebView] OfflineMode: when enabled, every request that does not resolve
// to plugin-local content is answered with an empty 403 before any network
// access (same observable behavior as master's [Chromium] OfflineMode).
// Requests to the virtual-host-mapped folders never fire this event
// (SetVirtualHostNameToFolderMapping bypasses it), so local documents and
// assets keep loading; the IsLocalUri host checks are kept anyway so the
// classification stays correct if a runtime ever starts reporting them.
// Classification itself is shared with the Linux backend via WebPolicy.
void InstallOfflineMode(const wil::com_ptr<ICoreWebView2>& webview)
{
	if (!to_int(GlobalSettings()["WebView"]["OfflineMode"]))
		return;

	webview->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
	EventRegistrationToken token;
	webview->add_WebResourceRequested(Callback<ICoreWebView2WebResourceRequestedEventHandler>(
		[](ICoreWebView2* sender, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT
		{
			wil::com_ptr<ICoreWebView2WebResourceRequest> request;
			wil::unique_cotaskmem_string uri;
			args->get_Request(&request);
			request->get_Uri(&uri);

			if (IsLocalUri(uri.get()))
				return S_OK;

			wil::com_ptr<ICoreWebView2_2> webview2;
			sender->QueryInterface(IID_PPV_ARGS(&webview2));
			wil::com_ptr<ICoreWebView2Environment> environment;
			webview2->get_Environment(&environment);

			wil::com_ptr<ICoreWebView2WebResourceResponse> response;
			environment->CreateWebResourceResponse(nullptr, 403, L"Blocked", L"", &response);
			args->put_Response(response.get());
			return S_OK;
		}).Get(), &token);
}

void AddApplyStyleScript(const wil::com_ptr<ICoreWebView2>& webview)
{
	const auto& htmlIni = GlobalSettings().get("HTML");
	const auto cssFile = gs_IsDarkMode ? htmlIni.get("CSSDark") : htmlIni.get("CSS");
	const auto cssUrl = L"http://assets.example/html/" + to_utf16(cssFile);

	webview->AddScriptToExecuteOnDocumentCreated(std::format(LR"(
							window.addEventListener('DOMContentLoaded', () => {{
							// HTML views render embedded (about:blank with a
							// spliced <base href>) or at evh:// on Windows, so
							// the local.example origin surfaces in either
							// window.location.href or document.baseURI. Gate on
							// the host name alone (matches http:// and evh://).
							if ((window.location.href + ' ' + document.baseURI).toLowerCase().indexOf('local.example') === -1) return;
							if (!document.getElementById('ev-html-style-link')) {{
							const link = document.createElement('link');
							link.id = 'ev-html-style-link';
							link.rel = 'stylesheet';
							link.href = '{0}';
							(document.head || document.documentElement).appendChild(link);}}
							}});)", cssUrl).c_str(), nullptr);
}

// Manual encoding selection (issue #66): extends the engine's BUILT-IN
// right-click menu with an "Encoding" submenu instead of replacing it
// with a DOM overlay (user-requested pivot; the old DOM menu also died
// after the first forced rewrite). Registered only for processors whose
// views can re-decode their bytes (supportsEncodingOverride -> HTML/MHT).
// Picks dispatch HOST-SIDE to the backend's ApplyCharsetOverride: the
// backend re-splices <meta charset> into its cached pristine HTML bytes
// and re-renders a fresh embedded document (HTML views), or forwards the
// tag to the MHT loader's own page-side executor - no fetch(), no JS->
// host round-trip (html-charset-override change).
//
// SDK note: put_Handled(true) makes WebView2 show the modified default
// menu itself; per-item picks arrive through add_CustomItemSelected.
// (This SDK revision exposes add_ContextMenuRequested on _11 and item
// creation on ICoreWebView2Environment9.)
// Provisional HTML charset auto-detection (charset-autodetect change):
// injects a bootstrap that sets the asset host and loads autodetect.js /
// jschardet.min.js. The glue itself guards on __evRawFileBytesB64 (HTML-
// only) and on _evAutoDetectDone, so it is a harmless no-op on non-HTML
// views and never re-fires. It never mutates the live DOM; it reports a
// CMD_AUTO_ENCODING message the host turns into a provisional re-decode.
void AddAutoDetectScript(const wil::com_ptr<ICoreWebView2>& webview)
{
	// [HTML] ForceDetectEncoding: when set, the glue skips the "declared
	// encoding is authoritative" gate so a WRONG declared charset gets
	// corrected (the engine-agreement gate still prevents touching genuine
	// files). Threaded into the page via window.__evForceDetect.
	const bool forceDetect = to_int(GlobalSettings()["HTML"]["ForceDetectEncoding"]) != 0;

	webview->AddScriptToExecuteOnDocumentCreated(std::format(LR"(
		window.addEventListener('DOMContentLoaded', () => {{
			if (window.__evAssetBase || !window.__evRawFileBytesB64) return;
			window.__evAssetBase = 'http://assets.example'; // virtual host to plugin assets
			window.__evForceDetect = {0};
			const s = document.createElement('script');
			s.src = window.__evAssetBase + '/charset/autodetect.js';
			(document.head || document.documentElement).appendChild(s);
		}});)", forceDetect ? L"true" : L"false").c_str(), nullptr);
}

void AddNativeEncodingMenu(const wil::com_ptr<ICoreWebView2>& webview, HWND hWnd)
{
	auto wv11 = webview.try_query<ICoreWebView2_11>();
	if (!wv11)
		return;

	wil::com_ptr<ICoreWebView2_2> wv2 = webview.try_query<ICoreWebView2_2>();
	if (!wv2)
		return;

	wil::com_ptr<ICoreWebView2Environment> environment;
	wv2->get_Environment(&environment);
	auto env9 = environment.try_query<ICoreWebView2Environment9>();
	if (!env9)
		return;

	EventRegistrationToken token;
	wv11->add_ContextMenuRequested(Callback<ICoreWebView2ContextMenuRequestedEventHandler>(
		[env9, hWnd](ICoreWebView2*, ICoreWebView2ContextMenuRequestedEventArgs* args) -> HRESULT
		{
			Log::Line(L"ContextMenuRequested: hwnd=0x{:X}", reinterpret_cast<uintptr_t>(hWnd));
			wil::com_ptr<ICoreWebView2ContextMenuItemCollection> items;
			args->get_MenuItems(&items);

			UINT32 count = 0;
			items->get_Count(&count);
			wil::com_ptr<ICoreWebView2ContextMenuItem> separator;
			env9->CreateContextMenuItem(nullptr, nullptr,
				COREWEBVIEW2_CONTEXT_MENU_ITEM_KIND_SEPARATOR, &separator);
			items->InsertValueAtIndex(count++, separator.get());

			wil::com_ptr<ICoreWebView2ContextMenuItem> submenu;
			env9->CreateContextMenuItem(L"Encoding", nullptr,
				COREWEBVIEW2_CONTEXT_MENU_ITEM_KIND_SUBMENU, &submenu);
			wil::com_ptr<ICoreWebView2ContextMenuItemCollection> children;
			submenu->get_Children(&children);

			// Active encoding for the current view ("" = auto-detect),
			// used to check the matching radio item.
			std::wstring activeTag, autoSuggestedTag;
			if (auto it = gs_Views.find(static_cast<void*>(hWnd)); it != gs_Views.end())
			{
				activeTag = it->second->GetActiveEncodingTag();
				autoSuggestedTag = it->second->GetAutoSuggestedTag();
			}

			UINT32 index = 0;
			for (const auto& entry : EncodingList::kItems)
			{
				// When auto-detection provisionally applied an encoding, show
				// it on the (checked) Auto-detect entry: "Auto: windows-1251".
				std::wstring display;
				if (entry.tag[0] == L'\0' && !autoSuggestedTag.empty())
					display = L"Auto: " + autoSuggestedTag;
				else
					display = entry.display;

				wil::com_ptr<ICoreWebView2ContextMenuItem> item;
				env9->CreateContextMenuItem(display.c_str(), nullptr,
					COREWEBVIEW2_CONTEXT_MENU_ITEM_KIND_RADIO, &item);
				item->put_IsChecked(entry.tag == activeTag);

				// Capture the tag (empty = Auto-detect). The pick resolves
				// the lister's backend by HWND and asks it to re-render
				// from its pristine byte cache.
				const std::wstring tag = entry.tag;
				EventRegistrationToken selectedToken;
				item->add_CustomItemSelected(Callback<ICoreWebView2CustomItemSelectedEventHandler>(
					[hWnd, tag](ICoreWebView2ContextMenuItem*, IUnknown*) -> HRESULT
					{
						Log::Line(L"Encoding pick: tag='{}' hwnd=0x{:X}", tag,
						          reinterpret_cast<uintptr_t>(hWnd));
						if (auto it = gs_Views.find(static_cast<void*>(hWnd)); it != gs_Views.end())
						{
							it->second->ApplyCharsetOverride(tag);
							Log::Line(L"Encoding pick: dispatched to backend");
						}
						else
						{
							Log::Line(L"Encoding pick: backend NOT FOUND in gs_Views");
						}
						return S_OK;
					}).Get(), &selectedToken);

				children->InsertValueAtIndex(index++, item.get());
			}
			items->InsertValueAtIndex(count, submenu.get());

			return S_OK;
		}).Get(), &token);
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

void HandleSaveAttachment(ICoreWebView2* webview, HWND hWnd, const std::vector<std::wstring>& tokens);

void ParseAndPostMessage(ICoreWebView2Controller* controller, ICoreWebView2* webview, HWND hWnd, const wil::unique_cotaskmem_string& message)
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
		else if (tokens[0] == L"CMD_SAVE")
			HandleSaveAttachment(webview, hWnd, tokens);
		else if (tokens[0] == L"CMD_AUTO_ENCODING")
		{
			// Provisional HTML charset auto-detection: the page suggests a
			// code page; the host applies it if allowed (never overrides a
			// user's manual pick). Resolve the backend by this lister's HWND.
			if (auto it = gs_Views.find(static_cast<void*>(hWnd)); it != gs_Views.end())
				it->second->ApplyAutoDetectedEncoding(tokens[1]);
		}
		else if (tokens[0] == L"CMD_AUTO_ENCODING_REPORT")
		{
			// Engine already decoded with the detected code page (data as-is,
			// or a genuine declared charset); record the tag so the Encoding
			// submenu shows "Auto: <codepage>" without re-rendering anything.
			if (auto it = gs_Views.find(static_cast<void*>(hWnd)); it != gs_Views.end())
				it->second->ReportAutoDetectedEncoding(tokens[1]);
		}
		else if (tokens[0] == L"CMD_ENCODING_APPLY_FAILED")
		{
			// A loader's page-side re-decode (MHT's window.__evEncodingApply)
			// could not apply the user's chosen code page. Hand the view back
			// to auto-detection (clear the checked entry, re-arm the latches);
			// the loader re-runs detection to restore the "Auto: <tag>" hint.
			if (auto it = gs_Views.find(static_cast<void*>(hWnd)); it != gs_Views.end())
				it->second->OnEncodingApplyFailed();
		}
	}
}

//------------------------------------------------------------------------
// Save an EML attachment on Windows. Message format (already split on
// '|' by ParseAndPostMessage): tokens[1] = sanitized filename,
// tokens[2] = URL-safe base64 attachment bytes. Runs the native folder
// picker, writes the decoded bytes, and reports the result back to the
// loader's `window.__emlSaveResult` callback via ExecuteScript.
void HandleSaveAttachment(ICoreWebView2* webview, HWND hWnd, const std::vector<std::wstring>& tokens)
{
	auto reply = [&](const std::wstring& status, const std::wstring& message)
	{
		std::wstring script = BuildSaveResultScript(status, message);
		webview->ExecuteScript(script.c_str(), nullptr);
	};

	if (tokens.size() < 3)
	{
		reply(L"error", L"Malformed save request.");
		return;
	}

	std::string b64 = to_utf8(tokens[2]);
	std::vector<uint8_t> bytes = DecodeBase64UrlSafe(b64);
	if (bytes.empty())
	{
		reply(L"error", L"Attachment payload is empty or corrupt.");
		return;
	}

	// Default the picker to the currently-viewed EML file's directory
	// (eml-save-default-folder). Empty default keeps today's behavior.
	std::wstring defaultDir;
	if (auto it = gs_Views.find(static_cast<void*>(hWnd)); it != gs_Views.end())
	{
		auto fileDir = it->second->GetCurrentFileDirectory();
		if (!fileDir.empty())
			defaultDir = fileDir.wstring();
	}

	std::wstring folder = PickFolder(hWnd, defaultDir);
	if (folder.empty())
	{
		reply(L"cancel", L"");
		return;
	}

	std::wstring target = folder + SanitizeAttachmentName(tokens[1]);
	if (SaveAttachmentToFolder(folder, tokens[1], bytes))
		reply(L"ok", L"Saved to " + target);
	else
		reply(L"error", L"Could not write the attachment to the chosen folder.");
}

// Configure the WebView2 environment + controller for `hWnd`. The
// function returns the synchronous HRESULT of the async
// `CreateCoreWebView2EnvironmentWithOptions` call; the actual init
// completes in the nested callbacks. On async failure the callbacks
// destroy the HWND so TC doesn't keep an uninitialized lister window.
HRESULT QueueConfigureWebView2(HWND hWnd, const std::wstring& fileToLoad, const ProcessorInterface* processor)
{
	auto userDirFinal = ExpandEnv(to_utf16(GlobalSettings()["WebView"]["UserDir"]));

	// [WebView] Switches: engine command-line flags forwarded verbatim as
	// AdditionalBrowserArguments (restores master's [Chromium] Switches).
	// The switches are plain ASCII, so this wstring conversion is acceptable.
	auto switches = GlobalSettings()["WebView"]["Switches"];
	auto options = Make<CoreWebView2EnvironmentOptions>();
	options->put_AdditionalBrowserArguments(std::wstring(std::begin(switches), std::end(switches)).c_str());

	// Register the private "evh://" custom scheme used to serve ForcedHtmlExt
	// files (.xml/.xhtml) in place as text/html via WebResourceRequested (the
	// local.example virtual host would serve them with their raw MIME and,
	// unlike a custom scheme, never fires WebResourceRequested). Windows-only
	// route; Linux reaches the same goal through the ev:// handler.
	auto schemeReg = Make<CoreWebView2CustomSchemeRegistration>(L"evh");
	schemeReg->put_HasAuthorityComponent(TRUE);
	schemeReg->put_TreatAsSecure(TRUE);
	LPCWSTR allowedOrigins[] = { L"https://*", L"http://*", L"evh://*" };
	schemeReg->SetAllowedOrigins(3, allowedOrigins);
	ICoreWebView2CustomSchemeRegistration* schemeRegs[] = { schemeReg.Get() };
	options->SetCustomSchemeRegistrations(1, schemeRegs);

	// [WebView] BrowserExecutableX86Folder / X64Folder: pin a specific
	// browser executable folder; empty key auto-detects Edge (as on master).
	std::wstring execFolderFinal;
	wchar_t* pBrowserExecFolder = nullptr;
	if (const auto& execFolder = GlobalSettings()["WebView"][BROWSER_FOLDER_KEY]; !execFolder.empty())
	{
		execFolderFinal = ExpandEnv(to_utf16(execFolder));
		pBrowserExecFolder = execFolderFinal.data();
	}

	return CreateCoreWebView2EnvironmentWithOptions(pBrowserExecFolder, userDirFinal.data(), options.Get(),
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[hWnd, fileToLoad, processor](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
			{
				if (FAILED(result))
				{
					DestroyWindow(hWnd);
					return result;
				}

				return env->CreateCoreWebView2Controller(hWnd,
					Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
						[hWnd, fileToLoad, processor](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
						{
							if (FAILED(result))
							{
								DestroyWindow(hWnd);
								return result;
							}

							wil::com_ptr<ICoreWebView2> webview;
							controller->get_CoreWebView2(&webview);

							if (to_int(GlobalSettings()["WebView"]["KeepZoom"]) && gs_ZoomFactor.contains(processor))
								controller->put_ZoomFactor(gs_ZoomFactor[processor]);

							DisableBrowserHotkeys(webview);
							SetColorProfile(webview);
							InstallOfflineMode(webview);
							AddAccleratorKeyHandler(controller, hWnd);

							EventRegistrationToken token;
							webview->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
								[=](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args)
								{
									wil::unique_cotaskmem_string message;
									args->TryGetWebMessageAsString(&message);
									ParseAndPostMessage(controller, webview, hWnd, message);
									return S_OK;
								}).Get(), &token);

							webview->AddScriptToExecuteOnDocumentCreated(
								L"window.addEventListener('keydown', event => { if (event.code === 'KeyQ') window.chrome.webview.postMessage('CMD_KEY|81'); else if (event.code >= 'Digit1' && event.code <= 'Digit8') window.chrome.webview.postMessage('CMD_KEY|' + event.code.charCodeAt(5)); });",
								nullptr);

							AddApplyStyleScript(webview);
							AddAutoDetectScript(webview);
							if (processor->supportsEncodingOverride())
							{
								AddNativeEncodingMenu(webview, hWnd);
							}

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

							// Match the original CreateWebView2Environment: explicitly size the
							// WebView to the HWND's current client rect. Skip if
							// the HWND hasn't been sized yet (e.g. 0x0) — WM_SIZE
							// will set it correctly later.
							RECT initialBounds;
							GetClientRect(hWnd, &initialBounds);
							if (initialBounds.right > initialBounds.left &&
								initialBounds.bottom > initialBounds.top)
							{
								controller->put_Bounds(initialBounds);
							}

							Navigator nav(*backend);
							nav.Open(fileToLoad);

							if (GetFocus() == hWnd)
								controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);

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
		return E_INVALIDARG;

	std::scoped_lock lock(g_viewCreateLock);
	return QueueConfigureWebView2(hWnd, fileToLoad, processor);
}
#endif
//------------------------------------------------------------------------