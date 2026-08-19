## ADDED Requirements

### Requirement: Linux HTML CSS injection

On Linux, the plugin SHALL inject the `[HTML] CSS` (or `[HTML] CSSDark`
when dark mode is active) stylesheet into pages loaded by the HTML
processor via `Navigate("http://local.example/...")` (rewritten to
`ev://local.example/...` before reaching the engine). The injection
SHALL happen via `IWebView::AddScriptToExecuteOnDocumentCreated`
during `QtWebEngineBackend` construction, matching the Windows-side
behavior of `WebViewFactory::AddApplyStyleScript`. The injected CSS
URL SHALL use the `ev://assets.example/html/<file>` scheme so the
Linux URI-scheme handler resolves it.

- The injection SHALL be skipped if the configured CSS filename is empty.
- The injected script SHALL only act on pages whose `window.location.href`
  starts with `ev://local.example` (the local-file virtual host).
- The injected script SHALL append a `<link rel="stylesheet" ...>` to
  the document's `<head>` (or document element if `<head>` is absent),
  matching the Windows-side behavior.
- Engine-level color scheme (Windows's `SetColorProfile` →
  `PreferredColorScheme`) is intentionally NOT replicated on Linux.
  Qt Web Engine exposes no equivalent via its public API, and
  emulating it via `QWebEnginePage::setBackgroundColor` is a host-side
  decision out of scope here.

#### Scenario: HTML file receives the configured stylesheet

- **WHEN** the user opens an HTML file via F3 on Linux with
  `[HTML] CSSDark=style-dark.css` configured and dark mode active
- **THEN** the rendered page includes a `<link>` element pointing at
  `ev://assets.example/html/style-dark.css` and the dark background /
  text colors from that stylesheet apply

#### Scenario: HTML file does not receive stylesheet when CSS is empty

- **WHEN** `[HTML] CSSDark` is empty (the default for the shipped ini
  is `none.css`, but if a user clears the key)
- **THEN** no `<link>` element is injected and the page renders with
  only its own styling
