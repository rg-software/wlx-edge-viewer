# Design: HTML charset override (host-side re-decode)

## Context

Evidence accumulated while fixing #66 on Linux (session log, fixtures in `Examples/`):

1. **Qt Web Engine cannot render scheme-served top-level documents.** `Navigate("ev://local.example/x.html")` produces a blank page: the renderer subprocess never receives the custom-scheme registration (warning: *"Please register the custom scheme 'ev' via QWebEngineUrlScheme::registerScheme()"*), so its parser rejects scheme-served bytes even though the UI-process handler serves them correctly. Fixed in c68637b by rendering embedded bytes via `setHtml`.
2. **In-page re-decode is unviable on Qt Web Engine.** Three strategies — `document.open/write/close`, DOMParser + head+body swap, body-only swap — all ended identically: no visual change and the host context menu stopped appearing; DevTools traces show execution halting mid-flight with no catchable error (page teardown). The MHT loader's shell-div pattern works because it never touches document structure.
3. **The old Windows mechanism** (removed with the port) forced charsets by faking a response header via `WebResourceRequested`. AGENTS.md forbids silently reviving that interceptor; any re-introduction must be designed cross-platform.

## Goals / Non-Goals

**Goals:**
- Manual per-view charset override for HTML that actually works on both backends.
- Relative subresources of archived pages keep resolving after an override.
- Menu stays functional across repeated overrides.
- No interceptors, no ini keys, no persistence, no new JS→host commands.

**Non-Goals:**
- Automatic charset detection (`[HTML] DetectEncoding` stays removed).
- Overrides for Markdown/RST/AsciiDoc/EML (UTF-8 by convention / per-part headers).
- Changing the MHT loader flow.

## Decisions

### D1: Host-side re-decode; no page-side executor for HTML

All re-decode logic lives in C++. The Encoding menu is already host-native on both platforms, and the backend already holds the pristine source bytes (`SetRawFileBytes`, c68637b) — there is nothing to message back to the host, so the JS round-trip is dropped entirely. This removes the entire class of failures in Context §2.

### D2: Charset forcing = `<meta charset>` splice + fresh full render

On selection, the backend rebuilds from **pristine cached bytes**: splice `<meta charset="<tag>">` at the head of the byte stream and perform a fresh embedded-string render. The engine's HTML parser then decodes the stream itself — standard Chromium encoding-sniffing behavior:

- the *first* `<meta charset>` within the 1024-byte prescan window wins, so the splice beats a later wrong declaration (`encoding-windows1251-wrongmeta.html`);
- decoding happens during normal parse of a fresh document — no live-DOM mutation anywhere;
- rejected/unknown labels degrade gracefully: Chromium falls back to default decoding, the page renders (mojibake perhaps, never blank).

Rejected alternatives:
- *Content-Type interception* (old `OverrideEncoding`): requires per-platform request plumbing explicitly barred from ad-hoc re-introduction;
- *C++ transcoding to UTF-16*: needs a codec library for ~25 code pages; meta-splice gets identical results for free;
- *JS `TextDecoder` + return-string* (tried): decode worked but every subsequent rendering strategy failed (§2).

Known limitation (documented, acceptable): per the HTML spec, a `<meta charset>` declaring UTF-16BE/LE is coerced to UTF-8, so genuine UTF-16 files cannot be forced via meta — they always carry BOMs in practice, which sniffing already handles. `EncodingList` keeps the entries; they simply act as no-ops on HTML.

### D3: `<base href>` splice fixes relative references on both platforms

Embedded-string loads lose the file-URL context: WebView2's `NavigateToString` renders against `about:blank`, and Linux's `setHtml` base URI pointed at the root, not the file's directory — both break relative `<img>`/`<link>` refs of archived pages. The splicer therefore prepends `<base href="{origin}/{dir-of-file}/">` alongside the charset meta:

- Windows: `http://local.example/<urlDir>/` → refs resolve through the existing virtual-host mapping to disk;
- Linux: `ev://local.example/<urlDir>/` → same through the scheme handler.

This also lets Windows adopt the embedded-string load path outright (dropping its dependence on the >2 MB temp-file workaround), since ref resolution no longer depends on the navigation URL.

### D4: Splicer insertion rules

Insertion point, chosen so both tags sit inside the prescan window and before any content (the tree builder files leading metas into `<head>` automatically):

1. after a leading `<!DOCTYPE ...>` (case-insensitive) if present;
2. else after a leading `<?xml ... ?>` declaration;
3. else at byte offset 0.

Tags are ASCII from the fixed `EncodingList`; no escaping needed. The splice is always applied to the pristine cache — never to previously spliced output — so repeated selections are idempotent. A UTF BOM, if present, stays ahead of the insertion point and continues to win (correct: BOM'd files don't need overrides).

### D5: Shared helper + interface surface

`EdgeViewer/CharsetOverride.{h,cpp}`: pure functions over `std::vector<uint8_t>` — buildable/tested without Qt or WebView2 (tier-1 style tests in `EdgeViewer.Tests`). Interface addition: `IWebView::ApplyCharsetOverride(const std::wstring& tag)` (empty tag = auto-detect); default no-op not appropriate here — both backends implement it, since only they own the byte cache and the menu. `WebView2Backend` additionally overrides `SetRawFileBytes` (currently inherits the no-op) to fill its cache. Menu handlers reduce to one call: `backend.ApplyCharsetOverride(tag)`.

### D6: Scope guards

MHT untouched (its loader already re-decodes internally). The Linux `edgeviewer-encoding-bootstrap` userscript and the Windows fetch/document.write HTML bootstrap are deleted — dead code under this design. `[HTML] CSS` injection (DOMContentLoaded script) is unaffected: DocumentCreation scripts re-run on every fresh render, including post-override ones.

## Risks / Trade-offs

- **Prescan window:** pathological files whose first 1024 bytes contain no doctype and huge leading comments still get our tags first (offset-0 rule) — safe by construction.
- **XML/XHTML served as `.xml`:** `<meta>` is meaningless in true XML mode; Chromium treats these as HTML here anyway (no XML MIME from our pipeline) — behavior unchanged vs today.
- **Windows parity testing burden:** Windows gains the embedded path; verification matrix below covers both platforms with the same fixtures.

## Migration Plan

1. Land helper + interface + backend implementations (mechanical, test-covered).
2. Switch menu handlers; delete both bootstraps.
3. Spec deltas + Readme update; manual matrix on Win32/x64/DC-Linux using the four `Examples/encoding-*` fixtures.
