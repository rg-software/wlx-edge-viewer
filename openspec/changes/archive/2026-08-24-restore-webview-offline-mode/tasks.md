# Tasks: restore-webview-offline-mode

## 1. Shared policy predicate

- [x] 1.1 Create `EdgeViewer/WebPolicy.h` / `WebPolicy.cpp` with `IsLocalUri` (wide + narrow overloads): allow `about`/`data`/`blob` schemes, `assets.example`/`local.example` authorities, `ev` scheme; everything else is non-local; malformed URIs fail closed; case-insensitive
- [x] 1.2 Add the new files to `EdgeViewer.vcxproj` (both Win32 and x64) and to the Linux `CMakeLists.txt` source list

## 2. Windows backend

- [x] 2.1 In `EdgeViewer/WebView/WebViewFactory.cpp`, add an offline-mode setup helper next to `DisableBrowserHotkeys`: when `[WebView] OfflineMode` parses to 1, register `AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL)` and a handler returning empty `403 Blocked` via `ICoreWebView2Environment::CreateWebResourceResponse` for every `!IsLocalUri(...)` request; call it from the controller-completed callback

## 3. Linux backend

- [x] 3.1 In `EdgeViewer/WebView/QtWebEngineBackend.cpp`, inside the once-only scheme-registration block, install a profile-level `QWebEngineUrlRequestInterceptor` when `[WebView] OfflineMode` parses to 1; its `interceptRequest` blocks requests where `!IsLocalUri(...)`

## 4. Config template

- [x] 4.1 Add a documented `OfflineMode=0` entry under `[WebView]` in `Resources/edgeviewer.ini`

## 5. Tests

- [x] 5.1 Add EdgeViewer.Tests cases for `IsLocalUri`: virtual-host authorities (`http://assets.example/...`, `https://LOCAL.example/...`), `ev://_close/<id>` / `ev://_cmd/...`, `about:`/`data:`/`blob:`, blocked `http(s)://` remote, `file://`, `ws(s)://`, garbage/no-scheme input
- [x] 5.2 Build tests and confirm all pass on Win32 and x64

## 6. Docs & bookkeeping

- [x] 6.1 Update `Readme.md` (remove future-work row about OfflineMode; document the key where ini keys are summarized)
- [x] 6.2 Update `openspec/notes/future-work.md` row 9 → restored by this change (keep table numbering stable)

## 7. Verify

- [x] 7.1 Build Release Win32 + x64 via MSBuild/vcpkg; run both test executables
- [ ] 7.2 Manual TC check (Win32 or x64): default ini unchanged behavior; with `OfflineMode=1` a Markdown file renders fully while an HTML page's remote image fails; `.url` shortcut to a remote site shows failure
