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

The HTML processor MUST render from a real navigation to the `local.example` virtual-host URL rather than an embedded-string load. It MUST navigate to `http://local.example/<urlDir>/<rel>` (on Linux rewritten to `ev://local.example/<urlDir>/<rel>` by the backend), where `<rel>` is the file's path relative to its mapped root, so the WebView engine fetches the file's real bytes through the `local.example` host mapping and applies its own BOM / `<meta charset>` / content charset sniffing over those bytes. A correctly-declared UTF-8 file MUST therefore render its non-ASCII characters correctly on the default path with no byte-mapping and no host-side transcode. The processor MUST still cache the file's raw bytes via `IWebView::SetRawFileBytes` so the encoding override and auto-detect paths (see `encoding-override`) can re-decode from pristine bytes. The processor MUST NOT splice a `<base href>` into the default render (the URL itself supplies the base). Genuine `.html`/`.htm` MUST be served with their natural `text/html` Content-Type by the virtual host and the processor MUST NOT rewrite any response header for them. The behavior MUST be identical on Win32 and x64.

#### Scenario: Default render is a real local.example navigation

- **WHEN** the user opens `C:\Site\sub\index.html`
- **THEN** the HTML processor MUST render by navigating to the `local.example` virtual-host URL (e.g. `http://local.example/sub/index.html`) rather than an embedded-string load
- **AND** the engine MUST fetch the file's real bytes through the `local.example` host mapping and decode them with its built-in charset sniffing
- **AND** the processor MUST NOT modify any response Content-Type header and MUST NOT prepend a `<base href>` into the default render

#### Scenario: No charset detection occurs

- **WHEN** a `.html` file with an unprefixed UTF-8 BOM or `<meta charset>` is opened
- **THEN** the processor MUST NOT detect the BOM and MUST NOT force a charset
- **AND** the WebView applies its own default encoding sniffing to the file's real bytes fetched through the `local.example` host mapping

#### Scenario: Declared UTF-8 non-ASCII HTML renders correctly

- **WHEN** a `.html` file declaring `<meta charset="utf-8">` contains non-ASCII characters (e.g. block-drawing `U+2588`)
- **THEN** the default render MUST display those characters correctly without byte-mapping or host-side transcoding

#### Scenario: Raw bytes are still cached for override and auto-detect

- **WHEN** any `.html` file is opened
- **THEN** the processor MUST cache the file's pristine bytes via `SetRawFileBytes` so the Encoding submenu and auto-detect can re-decode from them

#### Scenario: 32-bit and 64-bit parity for the default path

- **WHEN** the same `.html` file is opened on the 32-bit and 64-bit builds
- **THEN** both builds MUST produce identical real-navigation rendering with the same `local.example` URL resolution

### Requirement: ForcedHtmlExt rendered in place as HTML

A `ForcedHtmlExt` file (`.xml`/`.xhtml` per `[Extensions] ForcedHtmlExt`) is not relocated: it is served from its real filesystem location (mirroring Linux, which never temp-copies). So that its bytes render as HTML rather than raw markup, the file MUST be fetched through a path whose response carries `Content-Type: text/html`: on Linux the `ev://` scheme handler's default MIME (rewrite of `http://local.example`) provides this with no relocation and no temp copy; on Windows, because the `local.example` virtual host serves `.xml` as `application/xml` and does not raise `WebResourceRequested`, the backend MUST serve the forced file through a custom URI scheme whose requests DO hit a `WebResourceRequested` handler that reads the mapped file and answers with `Content-Type: text/html`. Relative subresource references inside the forced document MUST resolve against the source directory (the file is at its real location). Auto-detect and encoding override MUST continue to work over the cached pristine bytes. This requirement supersedes the "no response header rewrite" clause for ForcedHtmlExt files only; genuine `.html`/`.htm` are unaffected.

#### Scenario: ForcedHtmlExt renders as HTML with working relative refs

- **WHEN** the user opens `C:\Site\sub\sample.xml` (declared in `[Extensions] ForcedHtmlExt`)
- **THEN** the file MUST NOT be relocated and MUST be fetched at its real location
- **AND** it MUST be served with `Content-Type: text/html` (via the `ev://` default MIME on Linux, the custom scheme handler on Windows)
- **AND** a relative subresource such as `images/blue.png` next to it MUST resolve against `C:\Site\sub\`
- **AND** auto-detect / encoding override MUST still operate over the pristine bytes

#### Scenario: Genuine HTML is not overridden

- **WHEN** the user opens a genuine `.html` or `.htm` file
- **THEN** it MUST keep navigating to `http://local.example/<rel>` and be served with its natural `text/html` MIME
- **AND** no response header rewrite and no `<base href>` splice MUST occur, even if the document declares its own `<base>`

### Requirement: HTML CSS Injection via a DOMContentLoaded Listener

A single DOMContentLoaded listener installed by the Lister's WebView setup MUST decide, after each render, whether the current document is an HTML-family render. Because the HTML render now navigates to the `local.example` origin, that origin surfaces through `window.location.href` (and continues to surface through `document.baseURI` for embedded re-decode override/auto-detect renders that splice a `<base href>`); the listener SHALL treat the document as an HTML render when either `window.location.href` or `document.baseURI` carries the `http://local.example` origin (rewritten to `ev://local.example` on Linux). When it does, the listener MUST inject a `<link rel="stylesheet">` element pointing at the stylesheet selected for the current dark-mode state (see the CSS Theme requirement below), so the Lister can apply a consistent page chrome to all HTML-family files. The listener MUST NOT inject a stylesheet for renders to other hosts (for example the renderers of the Markdown, AsciiDoc, or RST loaders, which use `assets.example` and carry no local.example origin). The injected `<link>` MUST be a standalone element appended to the document's `<head>`. The injection MUST behave identically on Win32 and x64 because it runs as JavaScript inside the WebView.

#### Scenario: A stylesheet is injected for an HTML render

- **WHEN** an HTML file is rendered via a `local.example` navigation (or an embedded override render with a spliced `<base href>`)
- **AND** the DOMContentLoaded event fires
- **THEN** the listener MUST inject a `<link rel="stylesheet">` for the selected `[HTML]` stylesheet into the rendered document's `<head>`

#### Scenario: No injection for non-HTML-family renders

- **WHEN** the rendered document does not carry a local.example origin (e.g. a Markdown loader shown from `assets.example`)
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

