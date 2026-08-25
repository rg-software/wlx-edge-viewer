# html Specification

## Purpose
Render HTML, XHTML, XML and .htm files inside the Total Commander Lister pane using a real WebView navigation to a virtual-hosted URL (rather than the templated-string navigation used by the Markdown, AsciiDoc, and RST renderers). The encoding-override mode (charset detection + `Content-Type` rewriting) that previously existed under `[HTML] DetectEncoding=1` has been removed on both platforms; the WebView's built-in charset sniffing is the only path.
## Requirements
### Requirement: HTML File Detection

The HTML processor MUST claim files whose extension (case-insensitive) matches an entry listed under the `[Extensions]` HTML key of edgeviewer.ini (shipped as `HTML=HTM,HTML,XHTML,XML`). Extension matching MUST be identical on the Win32 and x64 builds; both bitnesses claim the same set of extensions.

#### Scenario: Detecting a .htm or .html file

- **WHEN** a file with the extension `.htm` or `.html` is opened in the Lister
- **THEN** the HTML processor MUST claim the file and render it

#### Scenario: Detecting an .xhtml or .xml file

- **WHEN** a file with the extension `.xhtml` or `.xml` is opened
- **THEN** the HTML processor MUST claim the file, because `XHTML` and `XML` are entries under `[Extensions]` HTML

#### Scenario: Case-insensitive extension match

- **WHEN** a file is named `INDEX.HTML`, `Index.Html`, or any other casing
- **THEN** the HTML processor MUST claim the file regardless of letter case

#### Scenario: 32-bit and 64-bit parity for detection

- **WHEN** the plugin is loaded in the 32-bit (`EdgeViewer.wlx`) build
- **AND WHEN** the plugin is loaded in the 64-bit (`EdgeViewer.wlx64`) build
- **THEN** both builds MUST recognize `.htm`, `.html`, `.xhtml`, and `.xml` extensions with identical behavior

### Requirement: HTML Rendering

The HTML processor MUST render from an embedded-string load rather than a virtual-host navigation: it MUST read the file's raw bytes once, hand them to the WebView backend (`IWebView::SetRawFileBytes`), and render the bytes via `NavigateToString` (on Linux, `setHtml` with an `ev://local.example/` base URI). The rendered byte stream MUST have `<base href="{origin}/<urlDir>/">` prepended (via the shared `CharsetOverride` helper, where `<urlDir>` is the opened file's directory relative to its mapped root), so relative subresource references resolve through the `local.example` host mapping to disk on both platforms. Top-level navigation to `http(s)://local.example/...` URLs MUST NOT be used for HTML rendering (the Qt Web Engine renderer cannot process custom-scheme-served documents). The processor MUST NOT attempt to detect the file's charset and MUST NOT rewrite any HTTP response headers; absent a manual override (see `encoding-override`), the WebView's built-in encoding sniffing decides the charset over the embedded bytes. The behavior MUST be identical on Win32 and x64.

#### Scenario: Default render is an embedded string with a base href

- **WHEN** the user opens `C:\Site\sub\index.html`
- **THEN** the HTML processor MUST read the file once and render its bytes via an embedded-string load
- **AND** the rendered byte stream MUST have `<base href="http://local.example/sub/">` prepended so relative references resolve through the `local.example` host mapping (fetched from `C:\Site\`)
- **AND** the processor MUST NOT modify any response Content-Type header

#### Scenario: No charset detection occurs

- **WHEN** a `.html` file with an unprefixed UTF-8 BOM is opened
- **THEN** the processor MUST NOT detect the BOM and MUST NOT force a charset
- **AND** the WebView applies its own default encoding sniffing to the embedded bytes

#### Scenario: 32-bit and 64-bit parity for the default path

- **WHEN** the same `.html` file is opened on the 32-bit and 64-bit builds
- **THEN** both builds MUST produce identical embedded rendering with the same base-href resolution

### Requirement: HTML CSS Injection via a DOMContentLoaded Listener

A single DOMContentLoaded listener installed by the Lister's WebView setup MUST decide, after each render, whether the current document is an HTML-family render. Because HTML renders embedded (about:blank), the local.example origin surfaces through the spliced `<base href>` via `document.baseURI` rather than `window.location.href`; the listener SHALL treat the document as an HTML render when either `window.location.href` or `document.baseURI` carries the `http://local.example` origin (rewritten to `ev://local.example` on Linux). When it does, the listener MUST inject a `<link rel="stylesheet">` element pointing at the stylesheet selected for the current dark-mode state (see the CSS Theme requirement below), so the Lister can apply a consistent page chrome to all HTML-family files. The listener MUST NOT inject a stylesheet for renders to other hosts (for example the renderers of the Markdown, AsciiDoc, or RST loaders, which use `assets.example` and carry no local.example base). The injected `<link>` MUST be a standalone element appended to the document's `<head>`. The injection MUST behave identically on Win32 and x64 because it runs as JavaScript inside the WebView.

#### Scenario: A stylesheet is injected for an HTML render

- **WHEN** an HTML file renders embedded with a spliced `<base href="http://local.example/...">`
- **AND** the DOMContentLoaded event fires
- **THEN** the listener MUST inject a `<link rel="stylesheet">` for the selected `[HTML]` stylesheet into the rendered document's `<head>`

#### Scenario: No injection for non-HTML-family renders

- **WHEN** the rendered document does not carry a local.example base (e.g. a Markdown loader shown from `assets.example`)
- **THEN** the DOMContentLoaded listener MUST NOT inject an HTML-specific `<link>`, because `assets.example` is not an HTML-family origin

#### Scenario: 32-bit and 64-bit parity for CSS injection

- **WHEN** the same HTML file is opened on the 32-bit and 64-bit builds
- **THEN** both builds MUST inject the same `<link>` element with the same stylesheet URL

### Requirement: HTML CSS Theme Selection (CSS vs CSSDark)

The HTML processor (and the DOMContentLoaded CSS injector that serves it) MUST read both the `[HTML] CSS` and the `[HTML] CSSDark` keys from edgeviewer.ini. When Total Commander signals dark mode is off, the injected stylesheet MUST be the file named by `[HTML]` CSS. When Total Commander signals dark mode is on, the injected stylesheet MUST be the file named by `[HTML]` CSSDark. In the shipped edgeviewer.ini both keys are `none.css`, which is an effective no-op stylesheet, so the observed default behavior is "no custom page chrome from the Lister"; this default selection MUST still be honored regardless of mode so that users can override either key to install a real stylesheet without touching the other. The selection MUST be identical on Win32 and x64.

#### Scenario: Light mode uses the [HTML] CSS value

- **WHEN** Total Commander opens an HTML-family file with the Lister dark mode flag set to off
- **THEN** the CSS injector MUST apply the stylesheet named by `[HTML]` CSS (shipped: `none.css`)

#### Scenario: Dark mode uses the [HTML] CSSDark value

- **WHEN** Total Commander opens an HTML-family file with the Lister dark mode flag set to on
- **THEN** the CSS injector MUST apply the stylesheet named by `[HTML]` CSSDark (shipped: `none.css`)

#### Scenario: Shipping default is an effective no-op stylesheet

- **WHEN** the shipped edgeviewer.ini is used unchanged
- **THEN** both `[HTML] CSS` and `[HTML] CSSDark` MUST name `none.css`
- **AND** the rendered HTML MUST show no Lister-injected page chrome beyond what the page itself supplies

#### Scenario: A user overrides CSSDark to a real dark stylesheet

- **WHEN** a user edits edgeviewer.ini so that `[HTML] CSSDark=dark.css` while `[HTML] CSS=none.css`
- **AND** the Lister opens an HTML-family file in dark mode
- **THEN** the CSS injector MUST apply `dark.css` instead of `none.css`

#### Scenario: 32-bit and 64-bit parity for CSS selection

- **WHEN** the plugin is loaded on either the 32-bit or 64-bit build
- **THEN** both builds MUST select between `[HTML]` CSS and CSSDark based on the dark-mode flag with identical behavior

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

