## MODIFIED Requirements

### Requirement: Mapped folder resources bypass request interception

Resources served from a folder mapped via the platform's virtual-host mechanism (WebView2 `SetVirtualHostNameToFolderMapping` on Windows; the `ev://` scheme handler on Linux) — i.e., any URL under `http://assets.example/` or `http://local.example/` (`ev://assets.example/` / `ev://local.example/` on Linux) — MUST be served directly by the engine and SHALL NOT trigger any plugin-side request interception. The `WebResourceRequested` interceptor (formerly installed in `EdgeViewer/WebView2.cpp` for the `html.example` encoding-override passthrough and the `[Chromium] OfflineMode` gate) was removed with the cross-platform port, so no interceptor exists on either platform. External (non-mapped) URLs pass through to the engine unchanged. This SHALL be identical on 32-bit and 64-bit builds.

#### Scenario: Request to a mapped host is not intercepted

- **WHEN** the renderer requests `http://assets.example/markdown/marked.js` or `http://local.example/Users/test/readme.md`
- **THEN** the engine serves the file from the mapped folder and no plugin-side request handler is invoked for that request

#### Scenario: Request to a non-mapped host is intercepted

- **WHEN** the renderer requests `http://html.example/index.html` (a host with no folder mapping)
- **THEN** no `WebResourceRequested` handler exists to intercept it (the interceptor was removed with the encoding-override path); the request passes through to the engine unchanged

#### Scenario: External request is blocked when OfflineMode is set

- **WHEN** `[Chromium] OfflineMode=1` is set and the renderer requests an external (non-mapped) URL
- **THEN** the key is ignored (OfflineMode was removed with the `[Chromium]` → `[WebView]` rename); no `403 Blocked` response is produced and the request passes through to the engine

#### Scenario: No request interceptor exists

- **WHEN** any document requests a URL
- **THEN** no `WebResourceRequested` / `QWebEngineUrlRequestInterceptor` handler is registered (the encoding-override and OfflineMode plumbing was removed on both platforms)
