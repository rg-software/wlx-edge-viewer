# Bugfixes

Linux backend corrections (manual encoding override, issue #66, and the
Qt6 build-compile errors that blocked `./BuildMakeSetup.sh`).

## Bug 1 — HTML re-decode fails ("Cannot re-decode with this encoding")

- **Symptom:** On an HTML file, right-click → Encoding → pick a code page
  shows the inline error toast `Cannot re-decode with this encoding`
  instead of re-rendering the file in the chosen charset.
- **Root cause:** The HTML re-decode executor (the document-created
  bootstrap in `QtWebEngineBackend.cpp`) re-fetches the file bytes with
  `fetch(window.location.href)`. The custom `ev://` scheme was
  registered **without** `QWebEngineUrlScheme::FetchApiAllowed`, so
  Chromium refused `fetch()`/`XHR` on it and the promise rejected → the
  `catch` showed the toast. The text loaders only used `fetch()` as an
  untested fallback (they inline base64), so this path was never
  exercised and the gap went unnoticed.
- **Fix:** Added `FetchApiAllowed` to the scheme flags in the
  `QWebEngineUrlScheme::registerScheme` block.
- **Files:** `EdgeViewer/WebView/QtWebEngineBackend.cpp`

## Bug 2 — MHT context menu never appears

- **Symptom:** On an MHT/MHTML file, right-click shows no Encoding
  submenu at all.
- **Root cause:** Menu gating matched
  `url.startsWith("ev://assets.example/mhtml/")`, but every
  loader-based processor is navigated with the fixed base URI
  `ev://assets.example/loader.html` (see `EdgeLister_Linux.cpp`). The
  MHT page URL therefore never matched, so the handler `return`ed and
  the menu was suppressed. Markdown/RST/AsciiDoc/EML share that exact
  same URL, so URL matching could never distinguish MHT from the rest.
- **Fix:** Gate the menu on the processor's `supportsEncodingOverride()`
  capability instead of the page URL. Each processor now reports it via
  `webView.SetEncodingOverrideSupported(...)` in `OpenIn` (HTML and MHT
  pass `true`; every other processor `false`). `IWebView` gained a
  no-op default so the Windows backend is unchanged. Every
  `Navigate`/`NavigateToString` resets the flag to `false`, which
  prevents a stale `true` from an earlier HTML/MHT view leaking onto a
  later image/directory/PDF view that reuses the same backend.
- **Files:** `EdgeViewer/IWebView.h`, `EdgeViewer/WebView/QtWebEngineBackend.cpp`
  (+ `.h`), `EdgeViewer/Processors/BaseFileProcessor.cpp`,
  `EdgeViewer/Processors/HtmlProcessor.cpp`

## Bug 3 — Stock context menu suppressed on non-encoding views

- **Symptom:** Right-clicking an image, directory, or PDF showed no
  context menu whatsoever.
- **Root cause:** The old handler `return`ed early (after the URL gate)
  for any non-encoding view, so it never displayed even the engine's
  standard menu.
- **Fix:** Always build and show the engine's standard context menu;
  the Encoding submenu is appended only when the capability flag is set.
- **Files:** `EdgeViewer/WebView/QtWebEngineBackend.cpp`

## Note on `encoding-koi8r.html` ("renders blank")

A legacy-encoded HTML file with **no declared charset** renders as
mojibake/blank *by design* — automatic charset detection was
deliberately removed (see `AGENTS.md` and future-work item 1). The
intended remedy is the manual Encoding override, which Bugs 1–3 now
make functional: right-click → Encoding → **KOI8-R** re-renders the
file correctly. The same applies to `encoding-windows1251-wrongmeta.html`
(force Windows-1251) and `encoding-wrong-charset.mht` (force a code
page from the now-working MHT submenu).

## Documentation (Readme.md)

- Removed the manual `cmake` one-liner and the classic two-step
  (`mkdir build && cd build && cmake ..`) blocks; `./BuildMakeSetup.sh`
  is the single build entry point.
- Fixed the Setup install path: it pointed at `~/.doublecmd/...`, but
  the CMake install rule and the tested location are
  `~/.local/share/doublecmd/...`.
- Added the missing prerequisites `zip` (consumed by the packaging step
  of the script) and a C++23-capable compiler; kept `cmake >= 3.16` and
  `Qt 6.4+`, both verified against `CMakeLists.txt`.

## Earlier Linux/Qt6 compile fixes (separate commit)

- `EdgeViewer/Platform_Linux.cpp`: `QFileDialog::getExistingDirectory`
  argument order — the `Options` flags were passed in the `dir`
  (`QString`) slot; inserted the missing `QString()` dir argument.
- `EdgeViewer/WebView/QtWebEngineBackend.cpp`:
  - `EvOfflineInterceptor` given an explicit `QObject*` constructor
    (it was constructed with a profile parent argument but had only an
    implicit default ctor).
  - `createStandardContextMenu()` called on `QWebEngineView` (not
    `QWebEnginePage`) and without the `QPoint` argument that the old
    `QWebEnginePage` API required; positioning is handled by
    `menu->exec(mapToGlobal(pos))`.
