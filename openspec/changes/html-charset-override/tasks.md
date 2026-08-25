# Tasks: HTML charset override (host-side re-decode)

## 1. Shared splicer helper

- [ ] 1.1 Create `EdgeViewer/CharsetOverride.h/.cpp`: `SpliceCharsetAndBase(raw, tag, baseHref)` implementing design D4 (doctype/xml-decl/offset-0 insertion; meta before base; pristine-in idempotence) plus the Latin-1 bytes→`std::wstring` wrapper reused by `HtmlProcessor`
- [ ] 1.2 Tier tests in `EdgeViewer.Tests` (`charset_override.cpp`): doctype file, doctype-less file, xml-decl file, BOM-prefixed file, wrongmeta fixture bytes (first-meta-wins), repeated-splice idempotence, empty-tag = no charset meta but base kept

## 2. Interface + backends

- [ ] 2.1 `IWebView.h`: add `ApplyCharsetOverride(const std::wstring& tag)` (empty = auto)
- [ ] 2.2 `WebView2Backend`: override `SetRawFileBytes` to cache bytes; implement `ApplyCharsetOverride` → splice from cache → `NavigateToString` (D3 base href makes the about:blank context safe); drop HTML's reliance on the >1M-wchar temp-file path (bytes are pre-fetched; keep the fallback for other callers)
- [ ] 2.3 `QtWebEngineBackend`: implement `ApplyCharsetOverride` identically over its existing cache (`ev://local.example/` base retained alongside the spliced `<base>`)
- [ ] 2.4 Menu handlers: `WebViewFactory.cpp` Encoding pick and `QtWebEngineBackend.cpp` triggered-lambda call `ApplyCharsetOverride(tag)`; delete both page-side HTML bootstraps (`AddEncodingBootstrapScript` Windows branch for local.example pages, Linux userscript already removed in c68637b)

## 3. Specs & docs

- [ ] 3.1 Apply deltas: `encoding-override` (HTML re-decode mechanics, decode-failure wording, parity dispatch), `html` (embedded-string rendering + base splice), `linux-runtime` (override-available requirement supersedes known-limitation note)
- [ ] 3.2 Update `Readme.md` future-work row #1 → resolved (manual override shipped; automatic detection still out of scope) and drop the corresponding AGENTS.md "known limitation" bullet once verified

## 4. Manual verification — Windows

- [ ] 4.1 TC x64 (+ Win32 smoke): open `Examples/encoding-windows1251.html` → mojibake; Encoding → Windows-1251 → correct Cyrillic; repeat twice more — menu still works each time
- [ ] 4.2 Same for `encoding-koi8r.html` (KOI8-R) and `encoding-windows1251-wrongmeta.html` (override beats wrong `<meta>`)
- [ ] 4.3 Auto-detect restores initial sniffed rendering; override does not survive ListLoadNextW / reopen
- [ ] 4.4 Regression: archived page with relative images/CSS renders resources after override; `[HTML] CSS` injection still applied post-override; >1 MB embedded HTML loads

## 5. Manual verification — Linux (Double Commander)

- [ ] 5.1 Repeat 4.1–4.3 on the Qt Web Engine build
- [ ] 5.2 Confirm MHT re-decode unaffected (`encoding-wrong-charset.mht`)

## 6. Verify

- [ ] 6.1 `BuildMakeSetup.bat` Release both platforms green; test suite green with new tier tests
