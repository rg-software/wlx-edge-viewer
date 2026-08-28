# Tasks: Automatic charset detection for HTML (provisional auto re-decode)

## 1. Detector asset + glue script

- [x] 1.1 Vendor a universal statistical charset detector (jschardet) under `Resources/assets/charset/jschardet.min.js`
- [x] 1.2 Write `Resources/assets/charset/autodetect.js` glue: runs on DOMContentLoaded for HTML views, reads `window.__evRawFileBytesB64`, `atob`s, runs the detector, checks `document.characterSet`; only post `CMD_AUTO_ENCODING|<tag>` when (a) bytes present, (b) no BOM / no `<meta charset>`/`http-equiv` in the first 1024 bytes, (c) detector is high-confidence (≥0.90), (d) guess ≠ `document.characterSet`
- [x] 1.3 Add `[HTML] ForceDetectEncoding` (default `1`): host reads it at bootstrap time, sets `window.__evForceDetect`; glue skips the declaration early-return when set (still honors confidence + agreement gates). Init the default `1` in edgeviewer.ini. Also maps jschardet's `x-mac-cyrillic` → windows-1251 (the wrongmeta fixture otherwise detects as Mac Cyrillic and would be skipped). Validated in jsdom: force=1+wrongmeta posts, force=0+wrongmeta silent, undec+force=0 posts.

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
- [x] 6.3 Manual TC: `encoding-windows1251.html` auto-corrects; menu shows "Auto-detect (Windows-1251)" checked; UTF-8 file no flicker; manual pick ties
- [x] 6.4 `BuildMakeSetup.bat` package verified

## 7. Verify — Linux (Double Commander)

- [x] 7.1 Repeat on Qt Web Engine build + CMake build green

## 8. MHT auto-detection (loader-owned)

- [x] 8.1 `mhtml/loader.html`: detect over the transfer-decoded text part (first `text/html`, else largest `text/*`), never the raw MIME envelope; quoted-printable + base64 un-encoding; pure-ASCII / no-part guards
- [x] 8.2 Agreement gate: the declared charset is authoritative only if it decodes the payload without fatal error; on disagreement post one `CMD_AUTO_ENCODING|<tag>`, on agreement/equality post `CMD_AUTO_ENCODING_REPORT|<tag>` (label-only, no re-render)
- [x] 8.3 Host gates (`ApplyAutoDetectedEncoding`/`ReportAutoDetectedEncoding`) relaxed from `encodingOverrideHtml` to `encodingOverrideSupported` in `WebView2Backend` + `QtWebEngineBackend`; `SetEncodingOverrideSupported` override added to `IWebView`/`WebView2Backend`
- [x] 8.4 Re-post latch hygiene: `SetEncodingOverrideHtml(false)` clears the host `autoAlreadyApplied` latch so a stale HTML auto-apply on a reused backend cannot block MHT; the page-side one-shot `__evMhtAutoDetectDone` is reset when the menu "Auto-detect" re-pick sends `__evEncodingApply(null)`
- [x] 8.5 Unit-verified via node harness over the real loader: `Examples/encoding-wrong-charset.mht` (QP, wrongly declared utf-8 → windows-1251, posts `CMD_AUTO_ENCODING`), `fileformatinfo.mht`/`large-page.mht`, synthetic base64/8bit/empty/`no-text-part` cases, and the Auto-detect re-pick path (19 assertions green)
- [x] 8.6 Builds green (x64 + Win32 Release) and 60 tests / 261 assertions pass
- [x] 8.7 Manual TC: `encoding-wrong-charset.mht` auto-corrects to Windows-1251, Encoding submenu shows "Auto-detect (Windows-1251)", and UTF-16 pick followed by "Auto-detect" re-corrects (user-verified)
- [ ] 8.8 Manual verification on Linux (Double Commander, Qt Web Engine): MHT auto-correct + Auto-detect re-pick on a Qt build

## 9. Unappliable manual pick reverts to auto

- [x] 9.1 HTML (`WebView2Backend` + `QtWebEngineBackend`): when the host transcode of a user-picked code page fails (e.g. UTF-16LE on a byte-oriented file), clear `userPicked` and re-arm the `autoAlreadyApplied` latch before re-navigating to the real URL, so the fresh document's `CMD_AUTO_ENCODING_REPORT` restores the detected render and the "Auto: <tag>" menu hint instead of a bare, stuck "Auto-detect"
- [x] 9.2 MHT (`mhtml/loader.html` + both backends): on a page-side `__evEncodingApply` re-decode throw, keep the previous render, post the new `CMD_ENCODING_APPLY_FAILED` JS→host command, and re-run MHT detection; the host's new `IWebView::OnEncodingApplyFailed` clears the checked entry, drops `userPicked`, and re-arms the auto latch so the "Auto: <tag>" hint is restored
- [x] 9.3 Synced `charset-autodetect` + `encoding-override` delta specs to main (new capability `charset-autodetect`; `encoding-override` "Decode failure handling" + "Cross-platform parity" updated to cover the revert-to-auto behavior and the `CMD_ENCODING_APPLY_FAILED` round-trip)
- [x] 9.4 Verify (Windows x64 + Win32 Release) and manual: RAD.HTML → pick UTF-16LE → view returns to auto with detected page and "Auto: <tag>" checked; same on an MHT with an undecodable pick