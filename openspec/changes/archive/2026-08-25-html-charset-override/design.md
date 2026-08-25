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

### D2: Charset forcing = host-side transcode + fresh full render

On selection, the backend rebuilds from **pristine cached bytes** and renders a fresh embedded-string document. The deciding mechanism is a **host-side transcode of the bytes into proper Unicode** with the chosen code page, then `NavigateToString`/`setHtml` of the decoded text:

- **Windows** — `CharsetOverride::TranscodeBytes` maps the EncodingList label to a Windows code page (`MultiByteToWideChar`) and produces a real Unicode `std::wstring`.
- **Linux** — the Qt backend performs the equivalent decode with `QStringDecoder` (Qt Web Engine builds ship ICU-backed converters), then hands the decoded string to `setHtml`.

Why NOT a `<meta charset>` splice (the originally-planned D2, empirically **dead**): `NavigateToString` (WebView2) and `setHtml` (Qt) always re-encode their string argument to UTF-8 before the engine parses it. The byte stream the HTML parser sees is therefore always UTF-8, so a spliced `<meta charset>` can never change the code page the engine decodes with — the override render was byte-identical to the original (confirmed in the Windows repro log: splice + re-render produced no visual change while MHT's real `TextDecoder` path worked). Transcoding host-side is the only way to force a legacy code page through an embedded-string load. The MHT loader's existing `TextDecoder` re-decode (page-side) is the same mechanism and was never affected.

- The transcode happens on a **fresh document**, so no live-DOM mutation anywhere.
- Unknown/undecodable labels degrade gracefully: the backend falls back to the plain Latin-1 render of the pristine bytes (identical to the initial sniffed view — mojibake perhaps, never blank). "Auto-detect" takes this same fallback.
- Overriding beats a *wrong* declared charset: the file's own `<meta charset="utf-8">` is irrelevant because the stream is already correctly decoded before the engine sniffs it.

Rejected alternatives:
- *Content-Type interception* (old `OverrideEncoding`): requires per-platform request plumbing explicitly barred from ad-hoc re-introduction;
- *`<meta charset>` splice* (tried): cannot work through string loaders that re-encode to UTF-8 (D2 note above);
- *JS `TextDecoder` + return-string* (tried): decode worked but every subsequent rendering strategy failed (§2).

Known limitation (documented, acceptable): Windows `MultiByteToWideChar` has no code page for a few `EncodingList` labels (e.g. `iso-8859-1` maps to 28591 which Windows treats as Windows-1252 per its ANSI fallback); the transcoder maps every label to the closest Windows code page, and where none exists (currently none) the fallback path applies. Genuine UTF-16 files always carry BOMs in practice, which sniffing already handles.

### D3: `<base href>` splice keeps relative references working

Embedded-string loads lose the file-URL context: WebView2's `NavigateToString` renders against `about:blank`, and Linux's `setHtml` base URI pointed at the root, not the file's directory — both break relative `<img>`/`<link>` refs of archived pages. Every embedded render (the initial one and each post-override one) therefore prepends `<base href="{origin}/{dir-of-file}/">`:

- Windows: `http://local.example/<urlDir>/` → refs resolve through the existing virtual-host mapping to disk;
- Linux: `ev://local.example/<urlDir>/` → same through the scheme handler.

This also lets Windows adopt the embedded-string load path outright (dropping its dependence on the >2 MB temp-file workaround), since ref resolution no longer depends on the navigation URL.

### D4: `<base href>` insertion + transcoder helper

One shared primitive (still kept for the default render and the empty-tag/fallback path) inserts the `<base href>` at the head of the byte stream. Insertion point:

1. after a leading `<!DOCTYPE ...>` (case-insensitive) if present;
2. else after a leading `<?xml ... ?>` declaration;
3. else at byte offset 0.

The `<base>` is always applied to the pristine cache — never to previously spliced output — so repeated selections are idempotent. A UTF BOM, if present, stays ahead of the insertion point. (A charset `<meta>` was originally spliced alongside, but the string-loaders re-encode to UTF-8 so it is inert — see D2; `CharsetOverride::SpliceCharsetAndBase` still exists and emits a meta only when called with a non-empty tag, which the override path no longer does.) The transcode path adds its own `<base href>` inline when building the decoded document.

### D5: Shared helper + interface surface

`EdgeViewer/CharsetOverride.{h,cpp}`: pure functions over `std::vector<uint8_t>` — buildable/tested without Qt or WebView2 (tier-1 style tests in `EdgeViewer.Tests`). It provides `SpliceCharsetAndBase` (insert `<base href>`; keeps a charset-meta slot for legacy callers) and `BytesToLatin1` (pristine-byte → string for the default/fallback render), plus, on Windows, `TranscodeBytes` (EncodingList label + Windows code page → Unicode). The Qt backend performs the decode with Qt-internal converters (no shared dependency). Interface addition: `IWebView::ApplyCharsetOverride(const std::wstring& tag)` (empty tag = auto-detect); default no-op not appropriate here — both backends implement it, since only they own the byte cache and the menu. `WebView2Backend` additionally overrides `SetRawFileBytes` (currently inherits the no-op) to fill its cache. Menu handlers reduce to one call: `backend.ApplyCharsetOverride(tag)`, which internally dispatches page-side for MHT (see D6).

### D6: Scope guards

MHT is dispatched page-side: its loader already re-decodes internally via `window.__evEncodingApply`, so the backend routes the tag to that executor (never the host transcode). `SetEncodingOverrideHtml` (reported by the processor during `OpenIn`) distinguishes the two so the single Encoding submenu serves both. The Linux `edgeviewer-encoding-bootstrap` userscript and the Windows fetch/document.write HTML bootstrap are deleted — dead code under this design. `[HTML] CSS` injection (DOMContentLoaded script) is unaffected: DocumentCreation scripts re-run on every fresh render, including post-override ones.

## Risks / Trade-offs

- **Code-page coverage:** Windows `MultiByteToWideChar` covers every `EncodingList` label via the closest code page (incl. ISO-8859-1 treated as Windows-1252 per Windows ANSI fallback); any non-mappable label falls back to the unspliced sniffed render (mojibake, never blank).
- **XML/XHTML served as `.xml`:** a transcoded stream renders as HTML as today (no XML MIME from our pipeline) — behavior unchanged.
- **Linux transcode coverage:** requires the Qt build's ICU-backed `QStringDecoder`; identical fallback semantics.

## Migration Plan

1. Land helper + interface + backend implementations (mechanical, test-covered).
2. Switch menu handlers; delete both bootstraps.
3. Spec deltas + Readme update; manual matrix on Win32/x64/DC-Linux using the four `Examples/encoding-*` fixtures.
