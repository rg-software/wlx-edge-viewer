## Why

On Windows, `WebViewFactory.cpp::AddApplyStyleScript` (`WebViewFactory.cpp:39-53`)
registers a `DOMContentLoaded` script that injects the plugin's `[HTML] CSS`
stylesheet into pages loaded via `HtmlProcessor::OpenIn` (which navigates to
`http://local.example/<file>`). This was ported away during the Linux work —
the Linux backend (`QtWebEngineBackend`) has no equivalent, so HTML files
loaded via the plugin on Linux render with only the file's own styling, and
plugin-side `[HTML] CSSDark` overrides never reach the page.

The user-visible result: dark mode toggles styling on Markdown, AsciiDoc,
RST, EML, MHTML, images and directory listings, but `[HTML] CSSDark` is a
no-op on Linux even when configured.

The shipped `[HTML] CSS=none.css` / `[HTML] CSSDark=none.css` is
intentional (matches Windows's shipped default). The plugin ships
`style.css` and `style-dark.css` under `Resources/assets/html/`; users
who want plugin-injected dark styling on HTML files opt in by changing
their `edgeviewer.ini`. This change makes that opt-in work on Linux.

**Engine-level color scheme (Windows's `SetColorProfile` → WebView2's
`PreferredColorScheme`) is intentionally NOT replicated.** That is a
host-side WebView2 API call, not a CSS injection, and Qt Web Engine has
no equivalent in its public API. Mirroring it via
`QWebEnginePage::setBackgroundColor` is a separate decision and out of
scope for this change.

## What Changes

- **Modified** (`QtWebEngineBackend.cpp`): at the end of the constructor,
  after the `QWebEngineView` is created, register the equivalent of
  Windows's `AddApplyStyleScript` via `AddScriptToExecuteOnDocumentCreated`.
  The script URL uses `ev://assets.example/html/<file>` (the `ev://` scheme
  registered for the Linux backend) instead of `http://assets.example/...`.
  The local-file check uses `ev://local.example` instead of
  `http://local.example`, since `QtWebEngineBackend::Navigate` rewrites the
  page URL to `ev://` before passing it to Qt Web Engine.

## Capabilities

### Modified Capabilities

- `dark-mode`: extend the "CSS selection per processor" requirement to
  cover the HTML processor's CSS injection path on Linux. The existing
  Windows scenario (HTML processor injects a style block using the
  `CSSDark` value from the `[HTML]` section) is mirrored for Linux via
  the same mechanism (`AddScriptToExecuteOnDocumentCreated`) — only the
  URL scheme (`ev://` instead of `http://`) differs.

## Impact

- **Code**: ~15 LOC added in `EdgeViewer/WebView/QtWebEngineBackend.cpp`.
  Windows builds untouched (`AddApplyStyleScript` and `SetColorProfile`
  remain in `WebViewFactory.cpp`).
- **Build**: no CMake change; uses already-linked `QWebEngineScript` /
  `QWebEngineScriptCollection`.
- **Readme**: small clarification in the dark-mode section noting that
  HTML dark mode on Linux follows the same `[HTML] CSSDark` opt-in as
  Windows, with no engine-level chrome replication.
- **Systems**:
  - Linux users with `[HTML] CSSDark` set to a non-empty stylesheet: the
    CSS now injects into local HTML pages. Users with carefully-styled
    HTML files will see the plugin CSS appended (matching Windows
    behavior). With the shipped `none.css` default, no visible change.
  - Windows users: no change.
