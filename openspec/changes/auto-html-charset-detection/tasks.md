# Tasks: Automatic charset detection for HTML (provisional auto re-decode)

## 1. Detector asset + glue script

- [ ] 1.1 Vendor a universal statistical charset detector (chardet-style, MIT) under `Resources/assets/charset/charset-detector.js` (single-file modern ESM/UMD, ~30 KB)
- [ ] 1.2 Write `Resources/assets/charset/autodetect.js` glue: runs on DocumentCreation for HTML views, reads `window.__evRawFileBytesB64`, `atob`s, runs the detector, checks `document.characterSet`; only post `CMD_AUTO_ENCODING|<tag>` when (a) bytes present, (b) no BOM / no `<meta charset>`/`http-equiv` in the first 1024 bytes, (c) detector is high-confidence, (d) guess ≠ `document.characterSet`

## 2. JS→host bridge

- [ ] 2.1 Windows: handle `CMD_AUTO_ENCODING|<tag>` in `WebViewFactory.cpp` `ParseAndPostMessage` → resolve backend via `gs_Views[hWnd]` → `ApplyAutoDetectedEncoding(tag)` when allowed (D3)
- [ ] 2.2 Linux: add `CMD_AUTO_ENCODING` case to the `ev://_cmd` handler in `QtWebEngineBackend.cpp` → resolve backend by container id → `ApplyAutoDetectedEncoding(tag)`

## 3. Backend state machine (shared)

- [ ] 3.1 `IWebView.h`: add `ApplyAutoDetectedEncoding(const std::wstring& tag)` (sets `autoApplied`, NOT `userPicked`) and `GetAutoSuggestedTag()`; both backends implement
- [ ] 3.2 `WebView2Backend` + `QtWebEngineBackend`: add `autoApplied`/`userPicked`/`autoSuggestedTag` transient members (reset on Navigate/NavigateToString); `ApplyCharsetOverride` from the menu sets `userPicked`; auto path sets `autoApplied` + `autoSuggestedTag`

## 4. Menu integration

- [ ] 4.1 WebViewFactory (`AddNativeEncodingMenu`) + Qt (`createStandardContextMenu` lambda): read `GetAutoSuggestedTag()`; if set, render "Auto-detect (<tag>)" label on the checked auto item; keep radio/check exclusivity (already landed)

## 5. Specs & docs

- [ ] 5.1 Spec deltas verified (this change: new `charset-autodetect`, `encoding-override` menu-state requirement)
- [ ] 5.2 Update `openspec/notes/future-work.md` row #1 → automatic detection now ships; drop the AGENTS.md "automatic detection stays out of scope" note
- [ ] 5.3 Update `Readme.md` future-work row to "resolved"

## 6. Verify — Windows

- [ ] 6.1 `Examples/encoding-windows1251.html`: opens mojibake → auto-corrects to Cyrillic (no manual pick); Encoding menu shows "Auto-detect (Windows-1251)" checked
- [ ] 6.2 `encoding-koi8r.html`, `encoding-windows1251-wrongmeta.html`: auto respects wrong `<meta>` (suppressed); validate behavior
- [ ] 6.3 Auto-detect menu pick restores sniffed render; ./manual pick stops auto; reopen resets
- [ ] 6.4 UTF-8/UTF-8-BOM files: no flicker, no re-render, menu "Auto-detect" checked
- [ ] 6.5 `BuildMakeSetup.bat` both platforms green; test suite green (66 tests)

## 7. Verify — Linux (Double Commander)

- [ ] 7.1 Repeat 6.1–6.4 on Qt Web Engine build + CMake build green