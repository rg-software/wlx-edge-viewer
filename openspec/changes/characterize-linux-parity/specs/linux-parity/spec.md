# linux-parity

Living checklist of Linux feature parity vs Windows. Each row is one feature
unit: a Windows behavior, its current Linux status, where it lives, and the
test approach. Rows are added as functionality is questioned; follow-up
OpenSpec changes are spawned from rows whose status moves toward "fixed".

## Status taxonomy

| Code | Meaning |
|------|---------|
| `works-verified` | Verified working on Linux (commit / spike / harness confirms). |
| `should-work` | Code path is shared with Windows or implemented but not actually exercised in a real DC run yet. Default for "no Linux-specific reason it should fail." |
| `planned` | Future-work item from `Readme.md` or the port proposal. Implementation has not started. |
| `broken-known` | Implemented but known regression / gap on Linux. Spawn a fix-up change. |
| `not-planned` | Out of scope (Windows-only by design, macOS, etc.). |
| `n/a` | The behavior is Windows-specific and has no Linux analog. |

## Test approach taxonomy

| Tier | Meaning | Where |
|------|---------|-------|
| `auto-shared` | Pure logic already covered by `EdgeViewer.Tests` (Catch2) on Windows. Verifies the algorithm, not the platform binding. | `EdgeViewer.Tests/` |
| `auto-qt` | Linux-side test against `QtWebEngineBackend` mock or headless `QWebEngineView`. Requires a future Qt test harness (does not exist today). | new harness TBD |
| `manual-dc` | Manual verification with Double Commander (real files, real DC window). Spec gives explicit DC steps and what to look for. | `Examples/` |
| `manual-visual` | Subjective rendering / interactivity check (e.g. dark-mode CSS). Manual screenshot diff. | `Examples/` |

The Linux build has no automated test suite today. Until `auto-qt` exists,
most rows land in `manual-dc`. A row can have both an `auto-shared` tier
(covers the algorithm) and a `manual-dc` tier (covers the platform binding).

## How to add a row

1. Pick the Windows feature: name, location (`EdgeViewer/...` or `Resources/...`), Windows capability (`openspec/specs/<cap>/spec.md`) if any.
2. Pick the Linux status from the table above. Default to `should-work` if the code path looks shared and there's no Linux-specific reason it should fail; only escalate to `works-verified` after an actual run confirms it.
3. Pick a test approach. Prefer `auto-shared` if the logic is in pure helpers (`Platform`, `Navigator`, `ProcessorInterface`, `WlxDetect`, `ZoomHotkey`). Otherwise `manual-dc` with concrete steps.
4. Add the row under "Quick-reference index" below AND a matching `### Requirement` block under "ADDED Requirements".
5. If a row becomes `broken-known`, open a new OpenSpec change (e.g. `fix-linux-<feature>`) and link it from the row's notes.

## Quick-reference index

| # | Feature | Status | Test | Notes |
|---|---------|--------|------|-------|
| 1 | Markdown rendering via `BaseFileProcessor` + `markdown/loader.html` (marked.js) | `works-verified` | `auto-shared` + `manual-dc` | Pre-fetch + base64 inlining; `http://`→`ev://` rewrite in `QtWebEngineBackend::NavigateToString`. Verified with `Examples/tutorial #1.md`. |
| 2 | `[Extensions] ForcedHtmlExt` regex temp-copy (XML/XHTML rendered as HTML) | `works-verified` *(implementation differs)* | `manual-dc` | `Platform_Linux.cpp::GetPhysicalPath` still does not implement the regex check — confirmed by code reading. However the user-visible result matches Windows: `.xhtml` / `.xml` files render as HTML, not as XML trees. Root cause is `EvSchemeHandler` (`QtWebEngineBackend.cpp:104-111`) returning `Content-Type: text/html` for any extension it does not explicitly map, which masks the missing temp-copy. **Side-effect:** the Windows-side behavior (rename to `.html`) does not run, but Chromium sniffs the `<!DOCTYPE html>` and renders the same HTML. The fix in `Platform_Linux.cpp` becomes a no-op for normal HTML content; it would still matter for XML files that Chromium would not auto-sniff as HTML. |
| 3 | Dynamic directory thumbnails (GDI+ shell thumbnail generator) | `planned` | `manual-dc` | Future-work item #2 from `Readme.md`. The "real thumbnails" rendered on Linux for image files in `Examples/test-data/images/` come from `<img src=...>` in the dirviewer loader — Chromium renders the full image scaled by CSS, not a downscaled GDI+/shell thumbnail. **Manual confirmation:** `[Directory] GenDirThumbs=1` on a directory with images falls back to static `folder.png`/`file.png` icons as expected, with no crash. The ini key is silently ignored on Linux per the port proposal. |
| 4 | Per-processor sticky zoom (`KeepZoom=1`) | `works-verified` for persistence, **`broken-known` for per-processor isolation** | `manual-dc` | Manual test: zoom level persists across Ctrl+Q sessions (closing and reopening the lister), but the zoom is shared across file types — MD and RST end up at the same zoom factor after switching. Two mechanisms at play on Linux: (a) within-session, Qt Web Engine's per-page zoom survives `NavigateToString` calls because the same `QWebEngineView` is reused for every file the current processor handles; (b) across-sessions, Qt Web Engine's per-origin zoom memory kicks in — all plugin files share the `ev://local.example/` origin, so a single zoom value is preserved. Windows's `gs_ZoomFactor` map gives per-processor isolation by storing one zoom per processor pointer; on Linux that map is never written because `QtWebEngineBackend` has no `ZoomFactorChanged` hook. Spawn `fix-linux-per-processor-zoom` if per-processor isolation is desired. |
| 5 | Right-click inside the rendered view (native shell context menu) | `planned` | `manual-dc` | Future-work item #3 from `Readme.md`. |
| 6 | HTML charset override for files without BOM / `<meta charset>` | `n/a` | n/a | Removed on both platforms per port proposal §Removed. |
| 7 | Detect string from `[Extensions]` (`ListGetDetectString`) | `works-verified` | `auto-shared` | Existing `[t4]` test exercises `BuildDetectString`; shared symbol exported on both platforms. |
| 8 | ESC key closes the lister | `works-verified` | `manual-dc` | Fixed by `openspec/changes/fix-linux-esc-closes-lister/` via an in-page JS bridge. The bridge: (1) `AllocateContainerId()` allocates a per-instance token before backend construction; (2) the backend's constructor embeds the token into a JS snippet registered via `AddScriptToExecuteOnDocumentCreated` that listens for `keydown` and on `Escape` issues `fetch('ev://_close/<id>')` (Image.src trick — Chromium rejects custom-scheme `fetch()`); (3) `EvSchemeHandler::requestStarted` recognizes the `_close` host, parses the id, looks up the registered container `QWidget*`, and posts a **synthetic `Q` keypress** to the container's parent widget (DC's viewer panel). DC's hotkey handler processes the `Q` identically to a physical press, invoking `cm_ExitViewer` → lister close. **Why synthetic Q instead of calling ListCloseWindow**: `hide()`/`close()` on the container from within the scheme handler either exits DC entirely (synchronous) or has no effect (deferred). `QCloseEvent` to the parent is ignored. The synthetic `Q` follows the same path DC uses for its built-in `Q` shortcut, which works reliably. **Why JS bridge instead of `QObject::eventFilter`**: Chromium intercepts keyboard input below Qt's event system — an event filter on `QWebEngineView` sees many events but never `QEvent::KeyPress`. |

| 9 | RST rendering (`rst/loader.html` + `render_rst.js`) | `works-verified` | `manual-dc` | `Examples/ReStructuredText.rst`. Shared `BaseFileProcessor` pre-fetch path. |
| 10 | AsciiDoc rendering (`asciidoctor/loader.html` + `asciidoctor.min.js`) | `works-verified` | `manual-dc` | `Examples/asciidoc.adoc`. |
| 11 | MHTML rendering (`mhtml/loader.html` + `mhtml2html.min.js`) | `works-verified` | `manual-dc` | `Examples/decoo.mhtml`. JS bundle parses the MHTML and renders inline bodies; inline images worked in this run. |
| 12 | EML rendering (`eml/loader.html` + `eml.js` + `postal-mime.min.js`) | `works-verified` | `manual-dc` | `Examples/multipart-sample.eml`. Headers, multipart/alternative HTML body, and attachments list all rendered. |
| 13 | HTML rendering via `Navigate("http://local.example/...")` (not pre-fetch) | `works-verified` | `manual-dc` | `Examples/Ôàéë-hoedown.html` (Cyrillic-transliteration filename). Exercises the `Navigate` path + the `http://`→`ev://` rewrite. |
| 14 | Image rendering (SVG/PNG/JPG/ICO/WEBP) via `imgview/loader.html` | `works-verified` for render and `F`-key FitToScreen toggle | `manual-dc` | `Examples/Genaille_division_rods.svg` renders correctly. The `F`-key toggle is fixed by commit `b7787f5`: `QtWebEngineBackend` injects a `window.chrome.webview.postMessage` shim (routes `CMD_ZOOM\|<scale>` through `ev://_cmd/<id>/<msg>`), and `EvSchemeHandler`'s `_cmd` branch parses the message and calls `QWebEnginePage::setZoomFactor(scale)`. The container registry (`g_containers`) now also stores the `QWebEnginePage*`. |
| 15 | PDF / Other rendering via `Navigate("http://local.example/...")` + Chromium PDF viewer | `works-verified` for PDF; additional binary formats now served | `manual-dc` | Commit `7e7b7ba` adds `.pdf` → `application/pdf` (plus `.zip`, `.docx`, `.xlsx`, `.odt`, `.epub`) to the `EvSchemeHandler` MIME map (`QtWebEngineBackend.cpp:249-262`), so Chromium's built-in PDF viewer activates instead of dumping bytes as `text/html`. |
| 16 | URL file → local file delegation (`UrlProcessor` → `LoadAndOpen` → `HtmlProcessor`) | `works-verified` for delegation | `manual-dc` | `Examples/test-data/sample-local.url` (target: `file:///.../sample.xhtml`) successfully chains through `UrlProcessor::OpenIn` → `gsProcRegistry().LoadAndOpen()` → `HtmlProcessor::OpenIn` and renders the file. The rendered result happens to be HTML (see Row 2 note on `EvSchemeHandler` default `text/html`). Commit `4ae5f08` strips trailing `\r` from `.url` URL lines (Windows-style CRLF). |
| 17 | URL file → external network navigation | `broken-known` | `manual-dc` | `Examples/google.url` (`URL=https://www.google.com`) opens a blank page in the lister. `QtWebEngineBackend::Navigate` passes real URLs through to `setUrl` unchanged (`QtWebEngineBackend.cpp:209-228`), but Chromium does not fetch. Observed stderr warning: *"Please register the custom scheme 'ev' via QWebEngineUrlScheme::registerScheme() before installing the custom scheme handler."* — this is a Qt Web Engine ordering diagnostic that fires even when the registration order is correct in our `std::call_once` block (`QtWebEngineBackend.cpp:145-156`). The blank page is a separate issue. Likely candidates: Chromium sandbox blocking network (no `QT_WEBENGINE_DISABLE_SANDBOX` set in the DC process env), DNS / cert chain, or Chromium's default new-tab page being shown in absence of a successful response. Spawn `fix-linux-external-url-network`. |
| 18 | Directory view static icons (no `GenDirThumbs`) | `works-verified` | `manual-dc` | `Examples/test-data/images/` shows the 3 PNGs as a grid via `<img src>` (Chromium scales the full image). Sort order: folders first, then alphabetical; matches Windows behavior for non-thumbnail directories. Dynamic thumbnails gated by `[Directory] GenDirThumbs=1` remain `planned` (Row 3). |
| 19 | Page zoom via Ctrl+wheel (engine-level) | `works-verified` | `manual-dc` | Qt Web Engine's built-in Ctrl+wheel zoom works natively. **Note:** this is engine-level zoom via `QWebEnginePage::setZoomFactor`; the `[WebView] KeepZoom=1` ini key (Row 4, `planned`) is unrelated and still does not persist per-processor zoom. |
| 20 | Search via DC's `ListSearchTextW` → `window.find(...)` | `works-verified` *(with quirks)* | `manual-dc` | `Navigator::Search` calls `IWebView::ExecuteScript` with `BuildFindScript(pattern, params)`; `QtWebEngineBackend::ExecuteScript` calls `page->runJavaScript(...)`. Chromium's `window.find` works (highlighting moves) but has UI quirks: no persistent find bar, F3-style "next match" needs a manual re-trigger, no status indicator. Worth a separate change to add a JS-side find bar. |
| 21 | Dark mode CSS injection (`[WebView]/[Type] CSSDark`) | `works-verified` *(for all processors: pre-fetch path + HTML processor)* | `manual-visual` + `auto-shared` (instrumented) | Manual test confirmed: switching KDE to dark theme correctly swaps `github.dark.css` for Markdown (`BaseFileProcessor::OpenIn` reads `[Markdown] CSSDark` at load time). The HTML processor's CSS injection is fixed by `openspec/changes/fix-linux-html-css-injection/` (see Row 24). **Root cause for the `gs_IsDarkMode`-always-false part** (now fixed by `fix-linux-dark-mode-fallback`): DC's Qt6 WLX caller does not propagate `lcp_darkmode` in `ShowFlags`. The fix added a `QGuiApplication::styleHints()->colorScheme()` fallback in `DllMain.cpp::ComputeDarkMode`. |
| 22 | Print (`ListPrintW` → `window.print()`) | `works-verified` | `manual-dc` | Fixed by commit `cc82f19`: new `IWebView::Print` virtual (default keeps the Windows `ExecuteScript("window.print()")` path), overridden in `QtWebEngineBackend::Print` (`QtWebEngineBackend.cpp:489-511`) with `QWebEnginePage::printToPdf` to a temp PDF, opened in the system viewer on `pdfPrintingFinished`. Chromium suppresses `window.print()` via `runJavaScript` (no user-gesture context), which is why the direct path silently dropped the preview. |
| 23 | Symlink resolution in `Platform_Linux.cpp::GetPhysicalPath` | `works-verified` | `manual-dc` | Manual test passed: F3 on `Examples/test-data/symlink-to-tutorial.md` (absolute symlink to `Examples/tutorial #1.md`) renders identically to F3 on the real path. `std::filesystem::weakly_canonical` (`Platform_Linux.cpp:94-97`) resolves the symlink before the scheme handler maps it. |
| 24 | HTML processor CSS injection | `works-verified` | `manual-dc` | Fixed by `openspec/changes/fix-linux-html-css-injection/`: `QtWebEngineBackend` constructor now mirrors Windows's `WebViewFactory::AddApplyStyleScript` (registers an `AddScriptToExecuteOnDocumentCreated` script that injects `[HTML] CSS` / `[HTML] CSSDark` into `ev://local.example/...` pages). **End-to-end manual verification**: with `[HTML] CSS=style.css` / `[HTML] CSSDark=style-dark.css` configured in `edgeviewer.ini`, F3 on `Examples/Ôàéë-hoedown.html` in dark mode renders with the dark stylesheet applied. The shipped `[HTML] CSSDark=none.css` is intentionally a no-op comment (same as Windows); users opt into plugin-side dark styling on HTML by setting `style-dark.css` (or any other stylesheet) in their `edgeviewer.ini`. Engine-level color scheme (Windows's `SetColorProfile` → WebView2's `PreferredColorScheme`) is intentionally NOT replicated on Linux — Qt Web Engine has no public equivalent, and emulating it via `QWebEnginePage::setBackgroundColor` is out of scope here. |

## ADDED Requirements

### Requirement: Markdown rendering

The plugin SHALL render Markdown files using the shared `BaseFileProcessor`
pre-fetch path (base64 inline `__FILE_CONTENT__`) and the
`Resources/assets/markdown/loader.html` template, and Qt Web Engine SHALL
serve the inline content via the `ev://` URI scheme on Linux.

- Windows ref: `EdgeViewer/Processors/MdProcessor.cpp`, `Resources/assets/markdown/loader.html`, `openspec/specs/markdown/spec.md`.
- Linux ref: shared processor + `EdgeViewer/WebView/QtWebEngineBackend.cpp:175-206` (`NavigateToString`).
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
- Linux ref: `EdgeViewer/Platform_Linux.cpp:88-100` does **not** copy. Instead, the `EvSchemeHandler` (`EdgeViewer/WebView/QtWebEngineBackend.cpp:104-111`) returns `Content-Type: text/html` as the default for any extension it does not explicitly map, so Chromium parses the body as HTML regardless of the original extension. Verified by F3 on `Examples/test-data/sample.xhtml` and `sample.xml` (both rendered as styled HTML pages).
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
- Status: `planned`.
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
- Linux ref: `EdgeViewer/WebView/QtWebEngineBackend.cpp:209-228` — `Navigate` rewrites only `http://local.example` / `http://assets.example` to `ev://`; external URLs pass through to `QWebEngineView::setUrl`. Verified by code reading; blank-page symptom suggests Chromium cannot fetch the URL (sandbox, DNS, or TLS chain).
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

## Notes on the Linux build

- There is **no automated Linux test harness today** (`Readme.md` §Tests confirms Windows-only Catch2). Any tier-`auto-qt` row requires scaffolding a Qt-side test runner first; until then, mark it `manual-dc` and document the DC steps explicitly.
- The Linux `CMakeLists.txt` builds only one configuration (`x64`, Release). Per-config parity tests (Debug vs Release, sandbox vs `--single-process`) are out of scope until a Debug build exists.
- `DllMain`/`DLL_PROCESS_DETACH` does not exist on Linux. `RemoveTempFiles()` is reachable but never auto-called. `EdgeViewer.Tests/tier3_paths.cpp` exercises the function on Windows; on Linux, `gs_tempFiles` accumulates across the plugin's lifetime in DC. Decide whether `ListCloseWindow` should clear it per lister.
- `gs_IsDarkMode` is set from `lcp_darkmode` with a `QGuiApplication::styleHints()->colorScheme()` fallback (see Row 21; fixed by `fix-linux-dark-mode-fallback`). CSS selection is verified, but **engine-level `prefers-color-scheme` propagation is still Windows-only**: the `WebView2Backend` calls `put_PreferredColorScheme`; the `QtWebEngineBackend` does not (Qt Web Engine has no public equivalent). Pages that rely on the `prefers-color-scheme` media query rather than the plugin's injected CSS will not follow the system theme on Linux.
