## Purpose

Render HTML, XHTML, XML and .htm files inside the Total Commander Lister pane using a real WebView navigation to a virtual-hosted URL (rather than the templated-string navigation used by the Markdown, AsciiDoc, and RST renderers). The encoding-override mode (charset detection + `Content-Type` rewriting) that previously existed under `[HTML] DetectEncoding=1` has been removed on both platforms; the WebView's built-in charset sniffing is the only path.

## MODIFIED Requirements

### Requirement: HTML Rendering

The HTML processor MUST render by navigating the WebView to a URL of the form `http://local.example/<urlPath>` (rewritten to `ev://local.example/<urlPath>` on Linux by `QtWebEngineBackend::Navigate`) where `<urlPath>` is the URL-encoded path of the opened file relative to the file's root directory. The `local.example` virtual host MUST be mapped to the file's root directory so the WebView fetches the real file from disk. The processor MUST NOT attempt to detect the file's charset and MUST NOT rewrite any HTTP response headers; the WebView's built-in encoding sniffing (BOM + `<meta charset>`) decides the charset. The behavior MUST be identical on Win32 and x64.

#### Scenario: Default render navigates to local.example

- **WHEN** the user opens `C:\Site\index.html`
- **THEN** the HTML processor MUST navigate the WebView to `http://local.example/index.html`
- **AND** the WebView MUST fetch `index.html` from `C:\Site\` via the `local.example` host mapping
- **AND** the processor MUST NOT modify the response's Content-Type header

#### Scenario: No charset detection occurs

- **WHEN** a `.html` file with an unprefixed UTF-8 BOM is opened
- **THEN** the processor MUST NOT detect the BOM and MUST NOT rewrite the response charset
- **AND** the WebView applies its own default encoding guessing

#### Scenario: 32-bit and 64-bit parity for the default path

- **WHEN** the same `.html` file is opened on the 32-bit and 64-bit builds
- **THEN** both builds MUST navigate to the same `http://local.example/...` URL with identical host mapping

### Requirement: HTML CSS Injection via a DOMContentLoaded Listener

A single DOMContentLoaded listener installed by the Lister's WebView setup MUST inspect the WebView's current `location` after each navigation. If `location` starts with `http://local.example` (Windows) or `ev://local.example` (Linux), the listener MUST inject a `<link rel="stylesheet">` element pointing at the stylesheet selected for the current dark-mode state (see the CSS Theme requirement below), so the Lister can apply a consistent page chrome to all HTML-family files. The listener MUST NOT inject a stylesheet for navigations to other hosts (for example navigations driven by the loader templates of the Markdown, AsciiDoc, or RST renderers, which use `assets.example`). The injected `<link>` MUST be a standalone `<link>` element appended to the document's `<head>`. The injection MUST behave identically on Win32 and x64 because it runs as JavaScript inside the WebView.

#### Scenario: A stylesheet is injected for a local.example render

- **WHEN** the WebView navigates to `http://local.example/index.html`
- **AND** the DOMContentLoaded event fires
- **THEN** the listener MUST inject a `<link rel="stylesheet">` for the selected `[HTML]` stylesheet into the rendered document's `<head>`

#### Scenario: A stylesheet is injected for an html.example render

- **WHEN** the WebView navigates to `http://html.example/index.html`
- **THEN** no injection occurs for that navigation, because `html.example` is no longer registered (the encoding-override path was removed); only `local.example` renders receive the HTML stylesheet

#### Scenario: No injection for non-HTML-family host navigations

- **WHEN** the WebView navigates to an `assets.example` URL (as Markdown, AsciiDoc, or RST renderers do)
- **THEN** the DOMContentLoaded listener MUST NOT inject an HTML-specific `<link>`, because `assets.example` is not an HTML-family host

#### Scenario: 32-bit and 64-bit parity for CSS injection

- **WHEN** the same HTML file is opened on the 32-bit and 64-bit builds
- **THEN** both builds MUST inject the same `<link>` element with the same stylesheet URL

### Requirement: HTML Virtual Host Mapping

The HTML processor MUST register virtual host-to-filesystem mappings in the WebView before navigating:

- The `local.example` virtual host MUST map to the root directory of the opened file, and is used in the only render path.
- The shared `assets.example` virtual host MUST continue to map to the plugin's own Resources/assets directory so that any plugin-bundled assets referenced by the injected stylesheet or the host setup load without a network request.

The `html.example` virtual host (formerly used by the encoding-override path) MUST NOT be registered. These mappings MUST be set up identically on Win32 and x64; on Linux the host is the `ev` scheme (`ev://local.example`, `ev://assets.example`) with the same folder semantics.

#### Scenario: local.example resolves to the opened file's root

- **WHEN** a user opens `C:\Site\index.html`
- **THEN** the HTML processor MUST map `local.example` to `C:\Site\`
- **AND** the WebView MUST fetch `index.html` from `C:\Site\` via the `local.example` host

#### Scenario: assets.example resolves to plugin assets for any HTML render

- **WHEN** an injected stylesheet or any other resource references `https://assets.example/...` during an HTML-family render
- **THEN** the request MUST be served from the plugin's Resources/assets directory
- **AND** the resource MUST load without any network request

#### Scenario: Relative links inside the opened HTML resolve against the file's root

- **WHEN** the opened `index.html` links to `about.html`
- **THEN** the link MUST resolve to a file in the same root directory that `local.example` is mapped to

#### Scenario: 32-bit and 64-bit parity for virtual host mapping

- **WHEN** the plugin is loaded on either the 32-bit or 64-bit build
- **THEN** both builds MUST register `local.example` and the shared `assets.example` host mappings with the same behavior

## REMOVED Requirements

### Requirement: HTML Rendering With Encoding Override (DetectEncoding=1)

**Reason**: The encoding-override mode was removed on both platforms during the cross-platform port (proposal `port-to-double-commander-linux` §Removed). Both WebView2 and Qt Web Engine already sniff charset from BOM and `<meta charset>`, and the override was off by default and leaked plumbing (`gs_Htmls` map, `WebResourceRequested` interceptor) into shared code paths. Re-introduction is tracked as future-work #1 in `Readme.md`.

**Migration**: None for users (off by default in the shipped ini). `[HTML] DetectEncoding` is silently ignored.

### Requirement: HTML BOM-Based Charset Detection

**Reason**: Dead code removed with the encoding-override path. Engine sniffing handles BOM detection natively.

**Migration**: None.

### Requirement: HTML Meta-Tag Charset Detection

**Reason**: Dead code removed with the encoding-override path. Engine sniffing handles `<meta charset>` natively.

**Migration**: None.
