# Rendering pipeline notes (pre-fetch + scheme rewrite)

Technical background on how loader-based processors get file content and
assets into the web engine. This was originally a `Readme.md` section; it
moved here when the README became user-facing documentation only (2026-08).
User-visible behavior is specified in `openspec/specs/` (`markdown`, `rst`,
`asciidoc`, `mhtml`, `eml`, `html`, `virtual-host-mapping`).

## Pre-fetched file content

Every loader-based processor (Markdown, AsciiDoc, RST, MHTML, EML) is a thin
subclass of `BaseFileProcessor` (`EdgeViewer/Processors/BaseFileProcessor.h`).
The base class:

1. Reads the loader template (`Resources/assets/<dir>/loader.html`)
2. Reads the actual file content
3. **Base64-encodes the file content** into a placeholder named `__FILE_CONTENT__`
4. Substitutes the other placeholders (`__FILENAME__`, `__CSS_NAME__`, `__BASE_URL__`)
5. Calls `NavigateToString` with the inlined HTML

The loaders read the content via bracket notation — never dot notation:
base64 padding `=` would break JS assignment syntax, and a
`window["__FILE_CONTENT__"]` lookup also breaks because the placeholder
regex-replacement rewrites the token inside the brackets. The literal string
inlined at `"__FILE_CONTENT__"` is the only reliable delivery form.

This eliminates the JS-side `fetch()` round-trip for the file content. Each
loader's render-time helper scripts (`marked.js`, `asciidoctor.min.js`,
`restructured.bundle.min.js`, `mhtml2html.min.js`, `postal-mime.min.js`,
highlight.js, MathJax, Mermaid) are still loaded normally from the platform's
virtual host (`http://assets.example/...` on Windows; `ev://assets.example/...`
on Linux — see below). Only the **content** is pre-fetched, not the renderer.

The pre-fetch pattern falls back to `fetch()` when the inlined constant is
absent, so old plugin builds work against new loaders and vice versa.

`imgview/loader.html` is intentionally **excluded**: it uses `<img src>`
directly (the engine fetches the image itself), so there is no JS `fetch()` to
replace. Pre-fetching binary images would require data-URL or Blob-URL
inlining, both of which have known issues with large images. Deferred — see
[future-work.md](future-work.md) closing note.

## Linux scheme rewrite (Qt Web Engine only)

Qt Web Engine (Chromium) does not allow registering `http` as a custom URI
scheme — Chromium reserves it for real web traffic. The Linux backend therefore
registers a custom scheme (`ev://`) and `QtWebEngineBackend::NavigateToString` /
`::Navigate` rewrite `http://` → `ev://` in loader HTML before handing it to
the engine. Loader templates can keep using `http://` references; the rewrite
is invisible to them (OpenSpec Decision 3 Fallback A of the port change,
spike-confirmed).

The host→folder map is process-wide; the `EvSchemeHandler` serves files with a
MIME type guessed by extension (`.css`, `.js`, `.png`, `.svg`, `.json`, plus
`.pdf`, `.zip`, `.docx`, `.xlsx`, `.odt`, `.epub` for native viewer activation);
everything else falls back to `text/html` — which is what makes forced-HTML
rendering work without Windows' temp-copy path.

References: `EdgeViewer/Processors/BaseFileProcessor.h`,
`EdgeViewer/WebView/QtWebEngineBackend.cpp`; archived port change
(`openspec/changes/archive/2026-08-24-port-to-double-commander-linux/tasks.md`
task 2.5b); `linux-parity` spec rows 1–9.
