## 1. EML loader: clickable attachment footer (JS/CSS, `Resources/assets/eml/`)

- [x] 1.1 In `eml.js` `attachmentList()`, render each non-inline attachment as a clickable button/link (filename + size + MIME) that calls a new `saveAttachment(att)` helper; keep the existing plain-footer fallback if attachments are absent.
- [x] 1.2 Register a loader-side callback `window.__emlSaveResult(status, message)` that updates a status element (e.g. "saved to <path>" / error text) based on the `status` value (`ok` | `cancel` | `error`).
- [x] 1.3 Add footer/save affordance styles to `style.css` and `style-dark.css` (clickable entry hover/active, saved/error status text) so the footer is visible in both light and dark mode.
- [x] 1.4 Ensure inline cid-referenced images are excluded from the savable list (filter `!att.related` only), matching the EML spec "Inline images are not saved as attachments".

## 2. JS→host save request direction

- [x] 2.1 Add `saveAttachment()` logic in `eml.js` that posts `window.chrome.webview.postMessage("CMD_SAVE|<sanitized-filename>|<base64>")` with the attachment's bytes (reuse `bytesToBase64`, eml.js:22) and sanitize the filename before embedding (`|` and control chars). Use URL-safe base64 on Linux so the message stays within the transport ceiling.
- [x] 2.2 Confirm the pre-fetch base64 path (`loader.html` inlined `__FILE_CONTENT__`) still feeds `eml.js` the same attachment objects used for save; no fallback-to-fetch change needed.

## 3. Host→JS reply reuses ExecuteScript

- [x] 3.1 Add a small shared helper (in C++ or injected per-backend) that calls `webView.ExecuteScript(L"window.__emlSaveResult && window.__emlSaveResult('ok'|'cancel'|'error', '<json-escaped-message>');")`. No change to the `IWebView` interface (Decision 1).
- [x] 3.2 Guard the reply so it no-ops safely if the callback is absent (the `&&` short-circuit), keeping rollback backward-compatible.

## 4. Platform folder picker + shared write

- [x] 4.1 Add `std::wstring PickFolder(const void* parentWindow);` declaration to `EdgeViewer/Platform.h` (empty string = user cancelled).
- [x] 4.2 Implement `PickFolder()` in `Platform_Win.cpp` using an `SHBrowseForFolder`-style folder dialog parented to the active lister HWND.
- [x] 4.3 Implement `PickFolder()` in `Platform_Linux.cpp` using `QFileDialog::getExistingDirectory` in the backend's widget context.
- [x] 4.4 Add a shared save helper that takes `(bytes, filename, folder)`, sanitizes the filename, and writes with `std::ofstream`, returning ok/error; reproducible on both OSes.

## 5. Windows command dispatch + size guard

- [x] 5.1 Extend `ParseAndPostMessage` (`EdgeViewer/WebView/WebViewFactory.cpp:91`) with a `CMD_SAVE` branch that decodes base64 → bytes, calls `PickFolder()`, and on a non-empty folder invokes the shared write, then delivers the result via the `ExecuteScript` helper.
- [x] 5.2 Windows-side payload handling: no hard cap by default (design Open Question); empty/invalid payloads are declined with an error result via `ExecuteScript`.

## 6. Linux command dispatch + size guard

- [x] 6.1 Extend the `ev://_cmd` handler (`QtWebEngineBackend.cpp:173-215`) to parse `CMD_SAVE|<filename>|<url-safe-base64>` and reject messages over the 1 MB raw-byte guard with an explicit error result (never silently truncate).
- [x] 6.2 For messages within the ceiling, decode, call `PickFolder()`, run the shared write, and deliver the result to the view (reuse the same JS callback).

## 7. Verify: build Release for both Win32 and x64 and load in Total Commander

- [x] 7.1 Build Release for Win32 and x64: `msbuild EdgeViewer.vcxproj /p:Configuration=Release /p:Platform=x86` and `... /p:Platform=x64` from the MSVS Developer Command Prompt (per AGENTS.md), confirming both DLLs build clean.
- [x] 7.1b Fix opened during verification: WebView2 `NavigateToString` 2 MB cap caused `about:blank` once the EML sample exceeded ~1.5 MB base64-inlined. `WebView2Backend::NavigateToString` now falls back to a `%TEMP%` file served via `lister.example` virtual host; `WebPolicy` classifies `lister.example` as local. Both builds compile clean and both test suites pass (233 assertions / 54 cases). See design Decision 6.
- [ ] 7.2 Load `winbuild\Release\EdgeViewer.wlx` / `EdgeViewer.wlx64` in Total Commander, open an `.eml` sample with an attachment (`Examples/`), click the attachment, choose a folder, and confirm the file is written and the view reports "saved".
- [ ] 7.3 Repeat the cancel path (folder dialog dismissed) and a failure path (e.g. unwritable folder) and confirm the view shows no error / a save-failed message respectively.
- [ ] 7.4 Confirm existing zoom (`Ctrl+wheel`) and right-click shell menu still behave (host-js-bridge coexistence).
- [ ] 7.5 On Linux (Double Commander + Qt Web Engine), verify attachments up to 1 MB save through the `ev://_cmd` path and larger attachments are declined with the explicit message (no silent truncation).

---
Rule note: per `openspec/config.yaml` tasks must end with a verify step building Release for both Win32 and x64; the rule's Windows focus is preserved here and Linux verification is documented as a manual Double-Commander step (no Linux test suite per AGENTS.md).