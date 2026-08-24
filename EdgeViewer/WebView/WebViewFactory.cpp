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
							if (window.location.href.toLowerCase().startsWith('http://local.example')) {{
							if (!document.getElementById('ev-html-style-link')) {{
							const link = document.createElement('link');
							link.id = 'ev-html-style-link';
							link.rel = 'stylesheet';
							link.href = '{0}';
							(document.head || document.documentElement).appendChild(link);}}
							}}
							}});)", cssUrl).c_str(), nullptr);
}

// Manual encoding selection (issue #66): injects the page-side executor
// used by the native Encoding submenu on HTML file pages (local.example).
// A forced charset re-fetches the file from its own origin, decodes via
// TextDecoder and rewrites the document in place; Auto-detect reloads so
// engine sniffing applies again. Purely transient - nothing is persisted
// and no JS->host command is involved. The menu itself lives host-side
// (AddNativeEncodingMenu below) and reaches the page through ExecuteScript,
// which also means the executor survives document.open() rewrites (window
// expandos are not cleared), fixing the "menu disappears after first
// re-encode" problem of the previous DOM-menu approach.
void AddEncodingBootstrapScript(const wil::com_ptr<ICoreWebView2>& webview)
{
	const auto& htmlIni = GlobalSettings().get("HTML");
	const auto cssFile = gs_IsDarkMode ? htmlIni.get("CSSDark") : htmlIni.get("CSS");
	const auto cssUrl = L"http://assets.example/html/" + to_utf16(cssFile);

	webview->AddScriptToExecuteOnDocumentCreated(std::format(LR"(
		window.addEventListener('DOMContentLoaded', () => {{
			if (!window.location.href.toLowerCase().startsWith('http://local.example'))
				return;
			window.__evEncodingApply = async (tag) => {{
				if (!tag) {{ window.location.reload(); return; }}
				try {{
					const r = await fetch(window.location.href);
					if (!r.ok) throw new Error('HTTP ' + r.status);
					const html = new TextDecoder(tag).decode(await r.arrayBuffer());
					document.open();
					document.write(html);
					document.close();
					if (!document.getElementById('ev-html-style-link')) {{
						const link = document.createElement('link');
						link.id = 'ev-html-style-link';
						link.rel = 'stylesheet';
						link.href = '{0}';
						(document.head || document.documentElement).appendChild(link);
					}}
				}} catch (e) {{
					let t = document.getElementById('ev-encoding-toast');
					if (!t) {{
						t = document.createElement('div');
						t.id = 'ev-encoding-toast';
						t.style.cssText = 'position:fixed;left:50%;bottom:28px;transform:translateX(-50%);background:rgba(30,30,30,.92);color:#fff;font:12px system-ui,sans-serif;padding:8px 14px;border-radius:6px;z-index:2147483647';
						(document.body || document.documentElement).appendChild(t);
					}}
					t.textContent = 'Cannot re-decode with this encoding';
					t.style.display = 'block';
					setTimeout(() => {{ t.style.display = 'none'; }}, 2600);
				}}
			}};
		}});)", cssUrl).c_str(), nullptr);
}

// Manual encoding selection (issue #66): extends the engine's BUILT-IN
// right-click menu with an "Encoding" submenu instead of replacing it
// with a DOM overlay (user-requested pivot; the old DOM menu also died
// after the first forced rewrite). Registered only for processors whose
// views can re-decode their bytes (supportsEncodingOverride -> HTML/MHT).
// Picks travel back into the page via ExecuteScript on __evEncodingApply,
// so the mechanism stays free of JS->host commands and persistence.
//
// SDK note: put_Handled(true) makes WebView2 show the modified default
// menu itself; per-item picks arrive through add_CustomItemSelected.
// (This SDK revision exposes add_ContextMenuRequested on _11 and item
// creation on ICoreWebView2Environment9.)
void AddNativeEncodingMenu(const wil::com_ptr<ICoreWebView2>& webview)
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
		[env9, webview](ICoreWebView2*, ICoreWebView2ContextMenuRequestedEventArgs* args) -> HRESULT
		{
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
			UINT32 index = 0;
			for (const auto& entry : EncodingList::kItems)
			{
				wil::com_ptr<ICoreWebView2ContextMenuItem> item;
				env9->CreateContextMenuItem(entry.display, nullptr,
					COREWEBVIEW2_CONTEXT_MENU_ITEM_KIND_COMMAND, &item);

				std::wstring js = entry.tag[0]
					? std::format(L"window.__evEncodingApply && window.__evEncodingApply('{}');", entry.tag)
					: std::wstring(L"window.__evEncodingApply && window.__evEncodingApply(null);");
				EventRegistrationToken selectedToken;
				item->add_CustomItemSelected(Callback<ICoreWebView2CustomItemSelectedEventHandler>(
					[webview, js](ICoreWebView2ContextMenuItem*, IUnknown*) -> HRESULT
					{
						webview->ExecuteScript(js.c_str(), nullptr);
						return S_OK;
					}).Get(), &selectedToken);

				children->InsertValueAtIndex(index++, item.get());
			}
			items->InsertValueAtIndex(count++, submenu.get());

			// Leave args->put_Handled(FALSE) (the default): per SDK docs,
			// Handled=TRUE suppresses the WebView2-drawn menu entirely;
			// with FALSE the runtime displays the MODIFIED default menu,
			// including our appended submenu. Picks of our entries arrive
			// via their add_CustomItemSelected handlers below.
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

	std::wstring folder = PickFolder(hWnd);
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
							if (processor->supportsEncodingOverride())
							{
								AddEncodingBootstrapScript(webview);
								AddNativeEncodingMenu(webview);
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