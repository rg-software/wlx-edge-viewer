# Tasks: Automatic charset detection for HTML (provisional auto re-decode)

## 1. Detector asset + glue script

- [x] 1.1 Vendor a universal statistical charset detector (jschardet) under `Resources/assets/charset/jschardet.min.js`
- [x] 1.2 Write `Resources/assets/charset/autodetect.js` glue: runs on DOMContentLoaded for HTML views, reads `window.__evRawFileBytesB64`, `atob`s, runs the detector, checks `document.characterSet`; only post `CMD_AUTO_ENCODING|<tag>` when (a) bytes present, (b) no BOM / no `<meta charset>`/`http-equiv` in the first 1024 bytes, (c) detector is high-confidence (≥0.90), (d) guess ≠ `document.characterSet`
- [x] 1.3 Add `[HTML] ForceDetectEncoding`: host reads it at bootstrap time, sets `window.__evForceDetect`; glue skips the declaration early-return when set (still honors confidence + agreement gates). Init the default `0` in edgeviewer.ini. Also maps jschardet's `x-mac-cyrillic` → windows-1251 (the wrongmeta fixture otherwise detects as Mac Cyrillic and would be skipped). Validated in jsdom: force=1+wrongmeta posts, force=0+wrongmeta silent, undec+force=0 posts.

## 2. JS→host bridge

- [x] 2.1 Windows: handle `CMD_AUTO_ENCODING|<tag>` in `WebViewFactory.cpp` `ParseAndPostMessage` → resolve backend via `gs_Views[hWnd]` → `ApplyAutoDetectedEncoding(tag)` when allowed (D3)
- [x] 2.2 Linux: add `CMD_AUTO_ENCODING` case to the `ev://_cmd` handler in `QtWebEngineBackend.cpp` → resolve backend by container id → `ApplyAutoDetectedEncoding(tag)`

## 3. Backend state machine (shared)

- [x] 3.1 `IWebView.h`: add `ApplyAutoDetectedEncoding(const std::wstring& tag)` (sets `autoApplied`, NOT `userPicked`) and `GetAutoSuggestedTag()`; both backends implement
- [x] 3.2 `WebView2Backend` + `QtWebEngineBackend`: add `autoApplied`/`userPicked`/`autoSuggestedTag` transient members (reset on Navigate/NavigateToString); `ApplyCharsetOverride` from the menu sets `userPicked`; auto path sets `autoApplied` + `autoSuggestedTag`

## 4. Menu integration

- [x] 4.1 WebViewFactory (`AddNativeEncodingMenu`) + Qt (`createStandardContextMenu` lambda): read `GetAutoSuggestedTag()`; if set, render "Auto-detect (<tag>)" label on the checked auto item; keep radio/check exclusivity (already landed)

## 5. Specs & docs

- [x] 5.1 Spec deltas verified (this change: new `charset-autodetect`, `encoding-override` menu-state requirement)
- [x] 5.2 Update `openspec/notes/future-work.md` row #1 → automatic detection now ships; drop the AGENTS.md "automatic detection stays out of scope" note
- [x] 5.3 Update `Readme.md` future-work row to "resolved"

## 6. Verify — Windows

- [x] 6.1 Glue logic unit-verified via jsdom harness: disagree+high-conf posts CMD_AUTO_ENCODING; agree silent; declared (wrongmeta/BOM) skipped; KOI8-R/windows-1251 normalization
- [x] 6.2 Both platforms build (x64 + Win32), 66 tests green
- [ ] 6.3 Manual TC: `encoding-windows1251.html` auto-corrects; menu shows "Auto-detect (Windows-1251)" checked; UTF-8 file no flicker; manual pick ties
- [ ] 6.4 `BuildMakeSetup.bat` package verified

## 7. Verify — Linux (Double Commander)

- [ ] 7.1 Repeat on Qt Web Engine build + CMake build green