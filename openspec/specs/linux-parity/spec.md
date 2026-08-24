# linux-parity Specification

## Purpose
TBD - created by archiving change characterize-linux-parity. Update Purpose after archive.
## Requirements
### Requirement: Markdown rendering

The plugin SHALL render Markdown files using the shared `BaseFileProcessor`
pre-fetch path (base64 inline `__FILE_CONTENT__`) and the
`Resources/assets/markdown/loader.html` template, and Qt Web Engine SHALL
serve the inline content via the `ev://` URI scheme on Linux.

- Windows ref: `EdgeViewer/Processors/MdProcessor.cpp`, `Resources/assets/markdown/loader.html`, `openspec/specs/markdown/spec.md`.
- Linux ref: shared processor + `EdgeViewer/WebView/QtWebEngineBackend.cpp:393-424` (`NavigateToString`).
- Status: `should-work`.
- Test: `auto-shared` (Catch2 covers `BaseFileProcessor` template substitution and `Base64Encode`) + `manual-dc` (open `Examples/tutorial #1.md` in DC).

#### Scenario: Markdown file renders with code blocks and tables

WHEN the user opens `Examples/tutorial #1.md` in DC's lister pane (F3),
THEN the rendered HTML shows headings, fenced code blocks (highlighted by
`highlight.js`), and GitHub-flavored tables rendered by `marked.js`.

### Requirement: ForcedHtmlExt temp-copy (verified working via different mechanism)

When the file's extension matches the `[Extensions] ForcedHtmlExt` regex
(shipped as `xml|xhtml`), the file SHALL be rendered as HTML rather than as
an XML document tree.

- Windows ref: `EdgeViewer/Platform_Win.cpp:75-80` copies the file to a temp `.html` and the WebView2 loads that, `openspec/specs/temp-file-management/spec.md` §ForcedHtmlExt.
- Linux ref: `EdgeViewer/Platform_Linux.cpp:88-100` does **not** copy. Instead, the `EvSchemeHandler` (`EdgeViewer/WebView/QtWebEngineBackend.cpp:249-262`) returns `Content-Type: text/html` as the default for any extension it does not explicitly map, so Chromium parses the body as HTML regardless of the original extension. Verified by F3 on `Examples/test-data/sample.xhtml` and `sample.xml` (both rendered as styled HTML pages).
- Status: `works-verified` *(implementation differs)* — the requirement is satisfied via the scheme handler's default MIME rather than the temp-copy path.
- Test: `manual-dc` (open `Examples/test-data/sample.xhtml` and `sample.xml`; confirm they render as HTML pages with the inline `<style>` applied, not as XML trees).

#### Scenario: XML file renders as HTML on the matched-extension path

WHEN the user opens `page.xml` and `[Extensions] ForcedHtmlExt=xml|xhtml`,
THEN the plugin renders the content as HTML (headings, paragraphs, lists)
rather than as an XML document tree.

#### Scenario: XHTML file renders as HTML on the matched-extension path

WHEN the user opens `Examples/test-data/sample.xhtml`,
THEN the plugin renders the HTML body with the embedded `<style>` applied
(red `<h1>`, sans-serif paragraph text), not as an XML tree.

### Requirement: Dynamic directory thumbnails

When `[Directory] GenDirThumbs=1` is set, the plugin SHALL generate a
per-entry thumbnail for image files inside a directory listing and for
folders (via the shell thumbnail provider on Windows, via freedesktop
thumbnail spec on Linux).

- Windows ref: `EdgeViewer/Processors/DirProcessor_Win.cpp`, `openspec/specs/directory-view/spec.md`.
- Linux ref: `EdgeViewer/Processors/DirProcessor.cpp:82,95-99` — `#ifdef _WIN32` gates the GDI+/shell call; the static `folder.png` / `file.png` icons are used instead.
- Status: `planned`.
- Test: `manual-dc` (open a directory containing several `.jpg` files; expect real thumbnails on Windows, generic icons on Linux).

#### Scenario: Image files inside a directory show real thumbnails

WHEN `[Directory] GenDirThumbs=1` and the user opens a directory containing
multiple image files,
THEN each entry shows a thumbnail generated from the file's content rather
than a generic icon.

### Requirement: Per-processor sticky zoom

When `[WebView] KeepZoom=1`, the plugin SHALL persist the last-used zoom
factor per processor across `ListLoadNextW` calls so reopening the same
file type restores the previous zoom.

- Windows ref: `EdgeViewer/WebView/WebViewFactory.cpp:147-175`, `openspec/specs/zoom-control/spec.md`.
- Linux ref: `QtWebEngineBackend` has no `ZoomFactorChanged` event hook; `gs_ZoomFactor` is not written on Linux.
- Status: `works-verified` for persistence, `broken-known` for per-processor isolation (Linux persists a single per-origin zoom shared across all file types — see Row 4).
- Test: `manual-dc` (open file A, Ctrl+wheel zoom to ~150%; open file B with `[WebView] KeepZoom=1`, expect the lister to restore A's zoom on returning to A).

#### Scenario: Sticky zoom survives a ListLoadNext round-trip

WHEN `[WebView] KeepZoom=1` and the user zooms file A to 150% then opens
file B (same processor),
THEN re-opening file A restores the zoom to 150%.

### Requirement: Right-click context menu

A right-click inside the rendered view SHALL show a plugin-specific context
menu (e.g. copy, print, find) wired to the WLX accelerator-key surface.

- Windows ref: `EdgeViewer/EdgeLister_Win.cpp` (`showPopupMenu`, ISHELL/IContextMenu), `openspec/specs/popup-context-menu/spec.md`.
- Linux ref: `EdgeViewer/EdgeLister_Linux.cpp` has no equivalent.
- Status: `planned`.
- Test: `manual-dc` (right-click inside the lister; expect a plugin menu on Windows, nothing on Linux).

#### Scenario: Right-click opens the plugin context menu

WHEN the user right-clicks inside the rendered lister pane,
THEN a context menu with plugin-specific entries appears.

### Requirement: HTML charset override (cross-platform)

When the HTML file has no BOM and no `<meta charset>` declaration, the
plugin SHALL NOT inject a charset override (override mechanism removed on
both platforms per port proposal §Removed). Engine sniffing decides the
encoding.

- Windows ref: `EdgeViewer/Processors/HtmlProcessor.cpp` `OverrideEncoding` path (removed).
- Linux ref: N/A.
- Status: `n/a`.
- Test: none. Re-introduction requires a cross-platform redesign triggered by a user-reported sample.

#### Scenario: Non-UTF-8 HTML without BOM or meta is rendered by engine sniffing

WHEN the user opens an HTML file with no BOM, no `<meta charset>`, and
non-UTF-8 content (e.g. Windows-1251),
THEN the engine's sniffing fallback determines the encoding (the plugin
does not inject `Content-Type`).

### Requirement: Detect string from `[Extensions]`

`ListGetDetectString` SHALL produce the detect string from the
`[Extensions]` section in `edgeviewer.ini`, identical on 32/64-bit and on
both Windows and Linux.

- Windows ref: `EdgeViewer/WlxDetect.cpp`, `EdgeViewer.Tests/tier4_extractions.cpp`.
- Linux ref: shared.
- Status: `works-verified`.
- Test: `auto-shared` (existing `[t4]` test exercises `BuildDetectString`).

#### Scenario: Detect string lists every configured type section

WHEN `edgeviewer.ini` has `[Extensions] HTML=HTM,... Other=PDF Dirs=1`,
THEN `ListGetDetectString` returns
`EXT="HTM,...|EXT="PDF,"|...` covering every configured section plus the
empty-extension directory detect term.

### Requirement: ESC closes the lister

When the user presses the ESC key while the lister pane has focus, the
plugin SHALL close the lister (destroy the embedded WebView, erase the
`gs_Views` entry) and return focus to the file panel.

- Windows ref: `EdgeViewer/EdgeLister_Win.cpp` (`pluginWndProc` falls through `WM_CLOSE` to `DefWindowProc`; ESC handling is at the Total Commander panel level, which posts `WM_CLOSE` to the lister HWND).
- Linux ref: JS bridge in `EdgeViewer/WebView/QtWebEngineBackend.cpp` captures `Escape` keydown, dispatches `fetch('ev://_close/<id>')` via Image.src; `EvSchemeHandler` posts a synthetic `Q` keypress to DC's viewer panel, which triggers DC's `cm_ExitViewer` close path.
- Status: `works-verified`.
- Test: `manual-dc` (open any lister in DC's right panel with F3; press ESC; expect the lister to close and focus to return to the file panel). On Windows the same manual step is the green-path reference.

#### Scenario: ESC closes an open lister

WHEN the user opens a file (e.g. `Examples/tutorial #1.md`) in the lister
pane and presses ESC,
THEN the lister pane is dismissed and the file panel regains focus.

#### Scenario: ESC closes an image lister that has toggled fullscreen via F

WHEN the user has pressed `F` to enter the image-viewer's fullscreen
class and then presses ESC,
THEN the lister closes (the loader's fullscreen toggle uses `F`, so
ESC is not consumed by the loader; the JS bridge dispatches the close
request via `ev://_close/<id>`, and the scheme handler posts a synthetic
`Q` keypress to DC's viewer panel, which closes the lister through
DC's normal `cm_ExitViewer` path).

### Requirement: PDF rendering via Chromium's built-in viewer

When the user opens a `.pdf` file (matched by `[Extensions] Other=PDF`),
the plugin SHALL render the PDF using Chromium's built-in PDF viewer
inside the lister pane.

- Windows ref: `EdgeViewer/Processors/OtherProcessor.cpp` `Navigate("http://local.example/<rel>")`; Chromium (WebView2) auto-activates the PDF viewer for `Content-Type: application/pdf` responses.
- Linux ref: `EdgeViewer/WebView/QtWebEngineBackend.cpp:427-446` (Navigate rewrites `http://local.example` → `ev://`) + `EvSchemeHandler` MIME map (`QtWebEngineBackend.cpp:249-262`) — now maps `.pdf` → `application/pdf` (plus `.zip`, `.docx`, `.xlsx`, `.odt`, `.epub`). Chromium activates its built-in PDF viewer when the response carries the correct `Content-Type`.
- Status: `works-verified`.
- Test: `manual-dc` (open `Examples/test-data/sample.pdf`; expect Chromium's PDF viewer page chrome and a rendered first page).

#### Scenario: PDF opens in Chromium's PDF viewer

WHEN the user opens `Examples/test-data/sample.pdf` in DC,
THEN the lister pane shows the PDF rendered as a paginated document with
the Chromium PDF viewer's toolbar (zoom, page navigation, download).

### Requirement: URL file navigates to external URLs

When the user opens a `.url` file whose `URL=` line points to an external
network resource (`http://` or `https://`), the plugin SHALL navigate to
that URL and render the response.

- Windows ref: `EdgeViewer/Processors/UrlProcessor.cpp` — `Navigate(to_utf16(url))` passes the URL straight through to WebView2.
- Linux ref: `EdgeViewer/WebView/QtWebEngineBackend.cpp:427-446` — `Navigate` rewrites only `http://local.example` / `http://assets.example` to `ev://`; external URLs pass through to `QWebEngineView::setUrl`. Verified by code reading; blank-page symptom suggests Chromium cannot fetch the URL (sandbox, DNS, or TLS chain).
- Status: `broken-known`.
- Test: `manual-dc` (open `Examples/google.url`; expect Google to load. Current behavior: blank page. To diagnose, capture `dc.stderr` while opening the URL — Chromium emits sandbox / DNS / TLS errors there).

#### Scenario: External URL renders in the lister

WHEN the user opens `Examples/google.url` (whose `URL=` line points to
`https://www.google.com`),
THEN the lister pane renders the Google homepage, not a blank page.

### Requirement: Image-viewer fullscreen toggle

When the user presses the `F` key while focused inside the image-viewer
loader, the loader SHALL toggle the `full-screen` CSS class on the
`<img>` element and report the new zoom factor to the host via
`IWebView::ExecuteScript` or equivalent (so the engine's zoom level is
synchronized with the visual class).

- Windows ref: `Resources/assets/imgview/loader.html:39-79` — the `keydown` handler calls `window.chrome.webview.postMessage("CMD_ZOOM|<scale>")` and `WebView2Backend.cpp` handles `ParseAndPostMessage` to call `controller->put_ZoomFactor`.
- Linux ref: `Resources/assets/imgview/loader.html:39-79` — same JS. Qt Web Engine lacks `window.chrome.webview`, so `QtWebEngineBackend`'s constructor injects a shim (only when `containerId != 0`) that defines `window.chrome.webview.postMessage` to route messages through `ev://_cmd/<id>/<msg>` via the `Image.src` trick (`QtWebEngineBackend.cpp:341-351`). `EvSchemeHandler`'s `_cmd` branch parses `CMD_ZOOM|<scale>` and calls `QWebEnginePage::setZoomFactor(scale)` (`QtWebEngineBackend.cpp:179-215`). The container registry (`g_containers`) now stores both the container `QWidget*` and the `QWebEnginePage*`.
- Status: `works-verified`.
- Test: `manual-dc` (open `Examples/Genaille_division_rods.svg`, press `F`; expect the `<img>` class to flip between `full-screen` and `real-size` AND the host zoom to synchronize via `setZoomFactor`).

#### Scenario: F toggles image fullscreen class

WHEN the user opens `Examples/Genaille_division_rods.svg` and presses `F`,
THEN the `<img class="...">` flips between `full-screen` (object-fit:
contain, fills viewport) and `real-size` (intrinsic dimensions), and the
host zoom level synchronizes with the visual class (the `CMD_ZOOM`
message reaches `setZoomFactor` via the `ev://_cmd` bridge).

