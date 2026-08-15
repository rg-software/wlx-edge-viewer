## Purpose

Render HTML, XHTML, XML and .htm files inside the Total Commander Lister pane using a real WebView navigation to a virtual-hosted URL (rather than the templated-string navigation used by the Markdown, AsciiDoc, and RST renderers), with an optional encoding-override mode that detects the file's charset from its BOM or `<meta>` tag and rewrites the HTTP response's Content-Type header accordingly.

## ADDED Requirements

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

### Requirement: HTML Rendering Without Encoding Override (DetectEncoding=0)

When the `[HTML] DetectEncoding` key in edgeviewer.ini is set to 0 (the shipped default), the HTML processor MUST render by navigating the WebView to a URL of the form `http://local.example/<urlPath>` where `<urlPath>` is the URL-encoded path of the opened file relative to the file's root directory. The `local.example` virtual host MUST be mapped to the file's root directory so the WebView fetches the real file from disk. In this mode the processor MUST NOT attempt to detect the file's charset, MUST NOT rewrite any HTTP response headers, and MUST let the WebView use its default content-type handling. The behavior MUST be identical on Win32 and x64.

#### Scenario: Default render navigates to local.example

- **WHEN** the user opens `C:\Site\index.html` and `[HTML] DetectEncoding` is `0` (default)
- **THEN** the HTML processor MUST navigate the WebView to `http://local.example/index.html`
- **AND** the WebView MUST fetch `index.html` from `C:\Site\` via the `local.example` host mapping
- **AND** the processor MUST NOT modify the response's Content-Type header

#### Scenario: No charset detection occurs in default mode

- **WHEN** a `.html` file with an unprefixed UTF-8 BOM is opened with `DetectEncoding=0`
- **THEN** the processor MUST NOT detect the BOM and MUST NOT rewrite the response charset
- **AND** the WebView applies its own default encoding guessing

#### Scenario: 32-bit and 64-bit parity for the default path

- **WHEN** the same `.html` file is opened on the 32-bit and 64-bit builds with `DetectEncoding=0`
- **THEN** both builds MUST navigate to the same `http://local.example/...` URL with identical host mapping

### Requirement: HTML Rendering With Encoding Override (DetectEncoding=1)

When the `[HTML] DetectEncoding` key in edgeviewer.ini is set to `1`, the HTML processor MUST render by navigating the WebView to a URL of the form `http://html.example/<urlPath>` where `<urlPath>` is the URL-encoded path of the opened file relative to the file's root directory. The `html.example` virtual host MUST be mapped to the file's root directory. Before navigating, the processor MUST detect the file's charset (see the BOM and meta-tag detection requirements below) and MUST remember the pairing of (file path → detected charset) so that when the WebView later requests that file via `html.example`, the WebResourceRequested interceptor can override the response's `Content-Type` header to include the detected charset (e.g. `Content-Type: text/html; charset=UTF-16LE`). This rewriting MUST happen on both the Win32 and x64 builds.

#### Scenario: Encoding-override mode navigates to html.example

- **WHEN** the user opens `C:\Site\index.html` and `[HTML] DetectEncoding` is `1`
- **THEN** the HTML processor MUST navigate the WebView to `http://html.example/index.html`
- **AND** the WebView MUST fetch `index.html` from `C:\Site\` via the `html.example` host mapping

#### Scenario: The response Content-Type is rewritten with the detected charset

- **WHEN** the WebView requests `http://html.example/index.html` after an encoding-override navigation
- **THEN** the WebResourceRequested interceptor MUST look up the detected charset recorded for that file path
- **AND** the interceptor MUST rewrite the response's `Content-Type` header to include that charset (e.g. `text/html; charset=UTF-8`)
- **AND** the WebView MUST decode the response body using the rewritten charset

#### Scenario: A file opened twice in mapping mode reuses the same path→charset pairing logic

- **WHEN** `index.html` is opened again after the Lister is reopened
- **THEN** the processor MUST re-run charset detection for that file (BOM first, then meta tag) before navigation
- **AND** the most recently detected charset MUST be the one applied to the response

#### Scenario: 32-bit and 64-bit parity for the override path

- **WHEN** the same `.html` file is opened on the 32-bit and 64-bit builds with `DetectEncoding=1`
- **THEN** both builds MUST navigate to `http://html.example/...`, detect the same charset, and rewrite the Content-Type identically

### Requirement: HTML BOM-Based Charset Detection

When the HTML processor is asked to detect the charset for an encoding-override render, it MUST examine the file's leading bytes for a Byte Order Mark and MUST recognize UTF-8, UTF-16LE, and UTF-16BE BOMs:

- If the first three bytes are `0xEF 0xBB 0xBF`, the detected charset MUST be UTF-8.
- If the first two bytes are `0xFF 0xFE`, the detected charset MUST be UTF-16LE.
- If the first two bytes are `0xFE 0xFF`, the detected charset MUST be UTF-16BE.

A BOM-detected charset MUST take precedence over any charset declared in a `<meta>` tag inside the document. If none of the three known BOMs is present, the processor MUST fall through to meta-tag detection. BOM detection MUST be identical on Win32 and x64 because it operates only on the file's leading bytes, which are read identically on both architectures.

#### Scenario: UTF-8 BOM is detected as UTF-8

- **WHEN** an HTML file starts with bytes `0xEF 0xBB 0xBF`
- **THEN** the HTML processor MUST detect the charset as UTF-8
- **AND** the rewritten Content-Type MUST specify `charset=UTF-8`

#### Scenario: UTF-16LE BOM is detected as UTF-16LE

- **WHEN** an HTML file starts with bytes `0xFF 0xFE`
- **THEN** the HTML processor MUST detect the charset as UTF-16LE
- **AND** the rewritten Content-Type MUST specify `charset=UTF-16LE`

#### Scenario: UTF-16BE BOM is detected as UTF-16BE

- **WHEN** an HTML file starts with bytes `0xFE 0xFF`
- **THEN** the HTML processor MUST detect the charset as UTF-16BE
- **AND** the rewritten Content-Type MUST specify `charset=UTF-16BE`

#### Scenario: BOM wins over a conflicting meta tag

- **WHEN** an HTML file starts with a UTF-8 BOM
- **AND** its `<meta>` declares `charset=ISO-8859-1`
- **THEN** the HTML processor MUST detect UTF-8 (from the BOM) and MUST NOT honor the conflicting meta charset

#### Scenario: No BOM falls through to meta-tag detection

- **WHEN** an HTML file has no recognized BOM but contains a `<meta charset=...>` tag
- **THEN** the HTML processor MUST NOT report a BOM-detected charset
- **AND** the meta-tag detection path MUST be used instead

#### Scenario: 32-bit and 64-bit parity for BOM detection

- **WHEN** the same BOM-prefixed HTML file is opened on the 32-bit and 64-bit builds
- **THEN** both builds MUST detect the same charset from the same leading bytes

### Requirement: HTML Meta-Tag Charset Detection

When the HTML processor does not find a BOM in an encoding-override render, it MUST scan the file contents for an HTML `<meta>` element declaring the charset. Two `<meta>` forms MUST be recognized:

- A short form: `<meta charset="...">` (or `charset=...` with single, double, or unquoted attribute value) whose value MUST be used as the detected charset.
- A long form: `<meta content="...charset=...">` (typically `http-equiv="Content-Type" content="text/html; charset=..."`) whose trailing `charset=...` MUST be extracted and used as the detected charset.

If neither form is present and no BOM was detected, the processor MUST leave the charset undetected for this file, in which case the WebResourceRequested interceptor MUST NOT rewrite the Content-Type charset and the WebView MUST fall back to its default handling. Meta-tag detection MUST be a regular expression scan over the raw bytes (decoded enough to match ASCII tag syntax), MUST be identical on Win32 and x64, and MUST yield the charset string found in the document verbatim.

#### Scenario: Short-form `<meta charset>` is detected

- **WHEN** an HTML file contains `<meta charset="UTF-8">` and has no BOM
- **THEN** the HTML processor MUST extract `UTF-8` as the detected charset
- **AND** the rewritten Content-Type MUST specify `charset=UTF-8`

#### Scenario: Long-form `<meta content>` charset is detected

- **WHEN** an HTML file contains `<meta http-equiv="Content-Type" content="text/html; charset=EUC-JP">` and has no BOM
- **THEN** the HTML processor MUST extract `EUC-JP` as the detected charset from the `content` attribute
- **AND** the rewritten Content-Type MUST specify `charset=EUC-JP`

#### Scenario: Multiple meta tags use the first match found

- **WHEN** an HTML file contains more than one charset-declaring `<meta>` tag
- **THEN** the regex scan MUST use the charset of the first meta tag matched in document order

#### Scenario: No BOM and no meta charset leaves the charset unset

- **WHEN** an HTML file has no BOM and contains no `<meta charset>` or `<meta content="...charset=...">` tag
- **THEN** the HTML processor MUST NOT detect a charset for the file
- **AND** the WebResourceRequested interceptor MUST NOT rewrite the Content-Type charset
- **AND** the WebView MUST apply its default encoding behavior

#### Scenario: 32-bit and 64-bit parity for meta-tag detection

- **WHEN** the same meta-tagged HTML file is opened on the 32-bit and 64-bit builds
- **THEN** both builds MUST extract the same charset from the same `<meta>` tag

### Requirement: HTML CSS Injection via a DOMContentLoaded Listener

A single DOMContentLoaded listener installed by the Lister's WebView setup MUST inspect the WebView's current `location` after each navigation. If `location` starts with `http://local.example` or `http://html.example`, the listener MUST inject a `<link rel="stylesheet">` element pointing at the stylesheet selected for the current dark-mode state (see the CSS Theme requirement below), so the Lister can apply a consistent page chrome to all HTML-family files. The listener MUST NOT inject a stylesheet for navigations to other hosts (for example navigations driven by the loader templates of the Markdown, AsciiDoc, or RST renderers, which use `assets.example`). The injected `<link>` MUST be a standalone `<link>` element appended to the document's `<head>`. The injection MUST behave identically on Win32 and x64 because it runs as JavaScript inside the WebView.

#### Scenario: A stylesheet is injected for a local.example render

- **WHEN** the WebView navigates to `http://local.example/index.html`
- **AND** the DOMContentLoaded event fires
- **THEN** the listener MUST inject a `<link rel="stylesheet">` for the selected `[HTML]` stylesheet into the rendered document's `<head>`

#### Scenario: A stylesheet is injected for an html.example render

- **WHEN** the WebView navigates to `http://html.example/index.html` (encoding-override mode)
- **AND** the DOMContentLoaded event fires
- **THEN** the listener MUST inject a `<link rel="stylesheet">` for the selected `[HTML]` stylesheet into the rendered document's `<head>`

#### Scenario: No injection for non-HTML-family host navigations

- **WHEN** the WebView navigates to an `assets.example` URL (as Markdown, AsciiDoc, or RST renderers do)
- **THEN** the DOMContentLoaded listener MUST NOT inject an HTML-specific `<link>`, because `assets.example` is not one of the two HTML-family hosts

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

The HTML processor MUST register virtual host-to-filesystem mappings in the WebView before navigating, according to the active mode:

- The `local.example` virtual host MUST map to the root directory of the opened file, and is used in the default `DetectEncoding=0` path.
- The `html.example` virtual host MUST map to the root directory of the opened file, and is used in the encoding-override `DetectEncoding=1` path. In addition, the processor MUST register the WebResourceRequested interceptor for this host so it can rewrite Content-Type headers on intercepted responses.

The shared `assets.example` virtual host MUST continue to map to the plugin's own Resources/assets directory so that any plugin-bundled assets referenced by the injected stylesheet or the host setup load without a network request. These mappings MUST be set up identically on Win32 and x64, because the host mapping logic is platform-independent and operates on absolute paths supplied by Total Commander.

#### Scenario: local.example resolves to the opened file's root in default mode

- **WHEN** a user opens `C:\Site\index.html` with `[HTML] DetectEncoding=0`
- **THEN** the HTML processor MUST map `local.example` to `C:\Site\`
- **AND** the WebView MUST fetch `index.html` from `C:\Site\` via the `local.example` host

#### Scenario: html.example resolves to the opened file's root in override mode

- **WHEN** a user opens `C:\Site\index.html` with `[HTML] DetectEncoding=1`
- **THEN** the HTML processor MUST map `html.example` to `C:\Site\`
- **AND** the WebView MUST fetch `index.html` from `C:\Site\` via the `html.example` host
- **AND** the WebResourceRequested interceptor MUST be active for `html.example` so it can rewrite the Content-Type charset

#### Scenario: assets.example resolves to plugin assets for any HTML render

- **WHEN** an injected stylesheet or any other resource references `https://assets.example/...` during an HTML-family render
- **THEN** the request MUST be served from the plugin's Resources/assets directory
- **AND** the resource MUST load without any network request

#### Scenario: Relative links inside the opened HTML resolve against the file's root

- **WHEN** the opened `index.html` links to `about.html`
- **THEN** the link MUST resolve to a file in the same root directory that `local.example` or `html.example` is mapped to, regardless of which mode is active

#### Scenario: 32-bit and 64-bit parity for virtual host mapping

- **WHEN** the plugin is loaded on either the 32-bit or 64-bit build
- **THEN** both builds MUST register `local.example`, `html.example`, and the shared `assets.example` host mappings with the same behavior