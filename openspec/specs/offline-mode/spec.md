# offline-mode Specification

## Purpose
Defines the observable offline-rendering behavior driven by the `[WebView] OfflineMode` configuration key: when enabled, documents render exclusively from plugin-local content and every request that would reach the network is blocked, with identical observable effects on Windows (WebView2) and Linux (Qt Web Engine).

## Requirements
### Requirement: Offline mode is opt-in

The offline-blocking behavior SHALL be active only when `[WebView] OfflineMode=1`. When the key is `0` or absent (the shipped default), no request filtering SHALL occur and rendering behaves exactly as before: external resources (remote images, CSS, fonts, scripts, remote navigations) load normally.

#### Scenario: Default leaves external content reachable

- **WHEN** `edgeviewer.ini` has no `OfflineMode` key and the user opens an HTML file that references an `https://` image
- **THEN** the image is fetched from the network and displayed

#### Scenario: Opt-in activates blocking

- **WHEN** `[WebView] OfflineMode=1` and the user reopens the same HTML file after reloading the plugin
- **THEN** the external image is not fetched and does not display, while the rest of the document renders

### Requirement: Non-local requests are blocked on both platforms

When offline mode is active, the web engine SHALL block every request whose target does not resolve to plugin-local content, before any network access takes place, on both Windows and Linux with the same observable outcome: the resource fails to load while the containing document continues to render. Local content means: the `assets.example` and `local.example` virtual hosts (serving plugin assets and the rendered file's directory), the `ev://` scheme handled by the Linux backend including its internal bridges, and engine-internal `about:`/`data:`/`blob:` URIs used by inline content. Any other origin — notably remote `http:`/`https:` targets — SHALL be treated as non-local.

#### Scenario: Remote subresource blocked, local document intact

- **WHEN** offline mode is active and the user opens a Markdown file whose rendered page loads its renderer scripts from `assets.example` and embeds a placeholder `<img src="https://example.net/pic.png">`
- **THEN** the renderer scripts load (the document formats correctly) and the remote image fails to load without disturbing the rest of the page

#### Scenario: Identical behavior across platforms

- **WHEN** the same `edgeviewer.ini` with `OfflineMode=1` is used on Windows and on Linux
- **THEN** both platforms block the same set of requests and render the same local-only documents

### Requirement: Internal machinery keeps working under offline mode

Blocking SHALL NOT interfere with the plugin's own operation: loader pages served via `NavigateToString`, stylesheet injection into `local.example` documents, the directory-viewer thumbnail pipeline, and the Linux JS→host bridges (`ev://_close/<id>`, `ev://_cmd/<id>/<message>`) SHALL continue to function while offline mode is active.

#### Scenario: ESC close bridge works while offline (Linux)

- **WHEN** offline mode is active on Linux and the user presses Esc inside a rendered image preview
- **THEN** the lister closes via the usual bridge path (the `ev://_close` request is not blocked)

### Requirement: Remote URL-file targets are unreachable while offline

When offline mode is active and the user opens a `.url` shortcut whose `URL=` target is a remote address, the target SHALL NOT be fetched; the view shows the engine's failure result for the navigation instead of the site. Local `file:///` targets of `.url` files SHALL continue to open through their normal processors.

#### Scenario: Remote shortcut blocked

- **WHEN** offline mode is active and the user opens a `.url` file with `URL=https://www.example.com/`
- **THEN** the site is not loaded; the view shows a load-failure page

#### Scenario: Local shortcut unaffected

- **WHEN** offline mode is active and the user opens a `.url` file with `URL=file:///C:/Docs/page.html`
- **THEN** the referenced local file renders through the HTML processor as usual