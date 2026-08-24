## 1. Shared encoding-menu component

- [x] 1.1 Create `Resources/assets/encoding-menu.js`: curated encoding list (D5), `contextmenu` handling, submenu UI with self-contained styling, close-on-click-outside/Escape, and the `window.EncodingMenu.attach({ apply })` integration contract from design D2
- [x] 1.2 Implement safe apply wrapper: catch `TextDecoder` label/decode errors and invoke integrator-provided error notice instead of blanking (spec: Decode failure handling)

## 2. MHT loader integration

- [x] 2.1 Add `<script src="http://assets.example/encoding-menu.js">` to `Resources/assets/mhtml/loader.html` (note: host is rewritten to `ev://assets.example` by the Linux backend)
- [x] 2.2 Refactor the render pipeline into a callable (bytes in → `convert()` → DOM) that keeps the raw bytes; wire `EncodingMenu.attach` so a forced label re-decodes via the scoped `TextDecoder` override around `mhtml2html.convert()` (D4), and `null` re-renders stock auto behavior

## 3. HTML bootstrap integration

- [x] 3.1 Extend `AddApplyStyleScript` in `EdgeViewer/WebView/WebViewFactory.cpp`: on `http://local.example*` pages also append `http://assets.example/encoding-menu.js` and attach an `apply(label)` doing fetch-from-own-origin → `TextDecoder` → `document.write`; auto-detect = host reload via existing navigation path (D3)
- [x] 3.2 Mirror the same bootstrap in `EdgeViewer/WebView/QtWebEngineBackend.cpp` for `ev://local.example*` pages with `ev://assets.example/encoding-menu.js`
- [x] 3.3 Verify the `[HTML]` CSS link survives a forced rewrite (document-created scripts refire after `document.open/write`); if not, pass the CSS URL into `attach()` so the menu re-appends it (design risk item)

## 4. Pivot: native context-menu extension (user feedback during apply)

First testing rejected the DOM menu (submenu flyout gap; standard browser menu hidden; menu lost after first forced re-encode). Design D1/D2/D5/D6 revised accordingly.

- [x] 4.1 Add `EdgeViewer/EncodingList.h` shared entry table + `ProcessorInterface::supportsEncodingOverride()` (true for `HtmlProcessor`, `MhtProcessor`)
- [x] 4.2 Windows `WebViewFactory.cpp`: `AddEncodingBootstrapScript` (page executor only) + `AddNativeEncodingMenu` (`ICoreWebView2_11::add_ContextMenuRequested` + `Environment9::CreateContextMenuItem` submenu, picks via `add_CustomItemSelected` → `ExecuteScript`), gated on processor flag
- [x] 4.3 Linux `QtWebEngineBackend.cpp`: same executor bootstrap + native extension of the standard context menu (`createStandardContextMenu` + "Encoding" QMenu, gated at popup time by page URL)
- [x] 4.4 MHT loader: drop `encoding-menu.js`, expose `window.__evEncodingApply`; delete `Resources/assets/encoding-menu.js`
- [x] 4.5 Build Release Win32 + x64 clean; test suite green (54 tests / 233 assertions both platforms)

## 5. Manual verification — Windows

- [x] 5.1 Prepare fixtures: windows-1251 HTML without `<meta charset>` (mojibake baseline) and an MHT with wrong/missing declared charset (see `Examples/`)
- [x] 5.2 TC Win32 + x64: right-click shows the STANDARD menu extended with an Encoding submenu on HTML/MHT views only; pick windows-1251/KOI8-R → content re-renders correctly and REPEATEDLY (menu still present after each override); local images/CSS of the archived page still resolve
- [x] 5.3 Auto-detect reset returns initial rendering; override does not survive ListLoadNextW / window reopen / plugin restart
- [x] 5.4 Regression pass: dirviewer shell right-click still works; dark mode CSS still applied on HTML pages after override; `[WebView] OfflineMode=1` does not block the own-origin fetch

## 6. Manual verification — Linux (Double Commander)

- [ ] 6.1 Repeat 5.2–5.3 on the Qt Web Engine backend (`port-to-double-commander-linux` build); confirm identical menu list and behavior per Cross-platform parity scenario

## 7. Verify

- [ ] 7.1 Build Release for both Win32 and x64 via `BuildMakeSetup.bat` (or vcvarsall + msbuild per AGENTS.md); load plugin in TC and confirm no regressions in the standard file-type smoke set
