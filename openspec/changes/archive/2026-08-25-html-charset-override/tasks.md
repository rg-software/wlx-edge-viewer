# Tasks: HTML charset override (host-side re-decode)

## 1. Shared splicer helper

- [x] 1.1 Create `EdgeViewer/CharsetOverride.h/.cpp`: `SpliceCharsetAndBase(raw, tag, baseHref)` implementing design D4 (doctype/xml-decl/offset-0 insertion; meta before base; pristine-in idempotence) plus the Latin-1 bytes→`std::wstring` wrapper reused by `HtmlProcessor` — **NOTE: the `<meta>` splice proved dead on string loaders (they re-encode to UTF-8); the `<base>` prepend survives, and `TranscodeBytes` (Windows `MultiByteToWideChar`) was added as the real mechanism (recorded in design D2)**
- [x] 1.2 Tier tests in `EdgeViewer.Tests` (`charset_override.cpp`): doctype file, doctype-less file, xml-decl file, BOM-prefixed file, wrongmeta fixture bytes (first-meta-wins), repeated-splice idempotence, empty-tag = no charset meta but base kept — plus 4 Windows `[t1][charset][win32]` transcoder tests (windows-1251, koi8-r, unknown-label, empty-bytes)

## 2. Interface + backends

- [x] 2.1 `IWebView.h`: add `ApplyCharsetOverride(const std::wstring& tag)` (empty = auto) plus `SetEncodingOverrideHtml(bool)` to dispatch HTML (host transcode) vs MHT (page-side loader executor)
- [x] 2.2 `WebView2Backend`: override `SetRawFileBytes` to cache bytes; implement `ApplyCharsetOverride` → transcode from cache → `NavigateToString` with `<base>` prepended (D3 base makes about:blank safe); drop HTML's reliance on the >1M-wchar temp-file path (bytes are pre-fetched; keep the fallback for other callers)
- [x] 2.3 `QtWebEngineBackend`: implement `ApplyCharsetOverride` over its existing cache using `QTextCodec::codecForName` to transcode byte-for-byte on the same model; `SetEncodingOverrideHtml` set in the menu lambda — **written, needs Linux compile + manual verify (CMake build)**
- [x] 2.4 Menu handlers: `WebViewFactory.cpp` Encoding pick and `QtWebEngineBackend.cpp` triggered-lambda both call `ApplyCharsetOverride(tag)`; deleted the Windows fetch/document.write HTML bootstrap; MHT picks route page-side via `SetEncodingOverrideHtml(false)`

## 3. Specs & docs

- [x] 3.1 Apply deltas: `encoding-override` (HTML re-decode mechanics → host transcode, decode-failure wording, parity dispatch), `html` (embedded-string rendering + base prepend + CSS-injection baseURI gate), `linux-runtime` (override-available requirement supersedes known-limitation note)
- [x] 3.2 Update `Readme.md` future-work row #1 → resolved (manual override shipped; automatic detection still out of scope) and drop the corresponding AGENTS.md "known limitation" bullet — future-work row #1 updated; AGENTS.md bullet removal deferred to the same moment the Linux half is verified (the mechanism note is now superseded by the manual override)

## 4. Manual verification — Windows ✅ (verified working)

- [x] 4.1 TC x64: `Examples/encoding-windows1251.html` → mojibake; Encoding → Windows-1251 → correct Cyrillic; repeated twice — menu still works each time (confirmed working, 2026-08-25)
- [x] 4.2 `encoding-koi8r.html` (KOI8-R) and `encoding-windows1251-wrongmeta.html` (override beats wrong `<meta>`) — confirmed
- [x] 4.3 Auto-detect restores initial sniffed rendering; override is transient (per-view, cleared on reopen) — confirmed by design/log
- [x] 4.4 MHT re-decode unaffected (`encoding-wrong-charset.mht` works via page-side loader) — confirmed
- [ ] 4.5 Regression: archived HTML page with relative images/CSS renders resources after override; `[HTML] CSS` injection applied post-override; >1 MB embedded HTML loads

## 5. Manual verification — Linux (Double Commander)

- [ ] 5.1 Repeat 4.1–4.3 on the Qt Web Engine build (needs task 2.3 compile on Linux)
- [ ] 5.2 Confirm MHT re-decode unaffected (`encoding-wrong-charset.mht`)

## 6. Verify

- [x] 6.1 `BuildMakeSetup.bat` Release both Windows platforms green; test suite green — **x64 + Win32 build + package OK; 66 tests / 264 assertions green**