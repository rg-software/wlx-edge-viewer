# Tasks: Send link to VirusTotal from the link context menu

## 1. Shared URL builder and launcher

- [x] 1.1 Add `EdgeViewer/UrlLauncher.h` declaring `std::wstring BuildVirusTotalUrl(const std::wstring& link)` and `bool OpenInDefaultBrowser(const std::wstring& url)`, with the `http:`/`https:` predicate used to gate the menu item (`IsScannableWebLink`), placed next to the existing platform-split helpers.
- [x] 1.2 Implement `UrlLauncher_Win.cpp` (Windows): `BuildVirusTotalUrl` encodes the link as URL-safe base64 (the deep-link form the current VirusTotal web UI and its own context-menu extensions use); `OpenInDefaultBrowser` calls `ShellExecuteW(nullptr, L"open", url.c_str(), ...)`.
- [x] 1.3 Implement `UrlLauncher_Linux.cpp` (Linux): same `BuildVirusTotalUrl`; `OpenInDefaultBrowser` calls `QDesktopServices::openUrl(QUrl(...))`, matching the existing print-output launch pattern.
- [x] 1.4 Verify the exact current VirusTotal deep-link form. **Result:** the production VirusTotal-Lookup extension (Chrome + Firefox) builds `https://www.virustotal.com/gui/url/<url-safe-base64-no-padding>/detection` via `btoa(url).replace('+','-').replace('/','_').replace(/=+$/,'')`; the launcher uses that same form (design D4 fallback). No percent-encoded variant is needed.
- [x] 1.5 Register the new files in `EdgeViewer/EdgeViewer.vcxproj` + `.vcxproj.filters` (Windows) and `CMakeLists.txt` (Linux); Windows-only file is `#ifdef _WIN32`-guarded at include time, no new WLX symbol so the Linux version script is untouched.

## 2. Windows context menu (WebView2)

- [x] 2.1 In `EdgeViewer/WebView/WebViewFactory.cpp`, call the context-menu registration unconditionally in `QueueConfigureWebView2` (moved out of the `if (processor->supportsEncodingOverride())` block).
- [x] 2.2 In the `ContextMenuRequested` handler, add the "Send link to VirusTotal" command item before the Encoding submenu, built only when `get_ContextMenuTarget()->get_LinkUri()` returns a URL that passes `IsScannableWebLink` (http/https, non-local per `WebPolicy::IsLocalUri`); wire `add_CustomItemSelected` to build the VirusTotal URL from the captured link and call `OpenInDefaultBrowser`.
- [x] 2.3 Re-gate the Encoding submenu inside the now-unconditional handler on the view's `SupportsEncodingOverride()` (new `IWebView` accessor backed by `m_encodingOverrideSupported`), so Encoding still appears only on HTML/MHT/EML views.

## 3. Linux context menu (Qt Web Engine)

- [x] 3.1 In `EdgeViewer/WebView/QtWebEngineBackend.cpp` `ContextView::contextMenuEvent`, add a "Send link to VirusTotal" action when `lastContextMenuRequest()->linkUrl()` passes `IsScannableWebLink`; connect it to `OpenInDefaultBrowser(BuildVirusTotalUrl(link))`.
- [x] 3.2 Keep the Encoding submenu gating unchanged (`m_impl->encodingOverrideSupported`); the menu builder already runs on every view, so only the new action is added.

## 4. Verify

- [ ] 4.1 Build Windows Release for both Win32 and x64 (`msbuild` in the MSVS Dev prompt per AGENTS.md), plus the Linux CMake build. **Windows done: Release Win32 + x64 build clean (0 errors), EdgeViewer.Tests pass (61 cases / 269 assertions). Linux CMake build NOT run — needs a Linux host (no Qt toolchain here).**
- [x] 4.2 Manual check (Windows): right-click an `http` link in an HTML file and in a Markdown view — "Send link to VirusTotal" appears and opens the default browser; right-click plain text and a directory-viewer link — item absent; Encoding submenu still only on HTML/MHT/EML. **Done by user: feature works.**
- [x] 4.3 Manual check (Linux): same matrix on the Qt Web Engine backend (DC F3/sample files per `openspec/notes/manual-testing-checklist.md`). **Done by user: feature works.**