## MODIFIED Requirements

### Requirement: HTML Rendering

The HTML processor MUST render from an embedded-string load rather than a virtual-host navigation: it MUST read the file's raw bytes once, hand them to the WebView backend (`IWebView::SetRawFileBytes`), and render the bytes via `NavigateToString` (on Linux, `setHtml` with an `ev://local.example/` base URI). The rendered byte stream MUST have `<base href="{origin}/<urlDir>/">` prepended (where `<urlDir>` is the opened file's directory relative to its root), so relative subresource references resolve through the `local.example` host mapping to disk on both platforms. Top-level navigation to `http(s)://local.example/...` URLs MUST NOT be used for HTML rendering (the Qt Web Engine renderer cannot process custom-scheme-served documents). The processor MUST NOT attempt to detect the file's charset and MUST NOT rewrite any HTTP response headers; absent a manual override (see `encoding-override`), the WebView's built-in encoding sniffing decides the charset over the embedded bytes. The behavior MUST be identical on Win32 and x64.

#### Scenario: Default render navigates to local.example

- **WHEN** the user opens `C:\Site\index.html`
- **THEN** the HTML processor MUST read the file once and render its bytes via an embedded-string load
- **AND** the rendered document MUST resolve relative references against `local.example/<dir-of-file>/` via a spliced `<base href>`, fetching them from `C:\Site\` through the host mapping
- **AND** the processor MUST NOT modify any response Content-Type header

#### Scenario: No charset detection occurs

- **WHEN** a `.html` file with an unprefixed UTF-8 BOM is opened
- **THEN** the processor MUST NOT detect the BOM and MUST NOT force a charset
- **AND** the WebView applies its own default encoding sniffing to the embedded bytes

#### Scenario: 32-bit and 64-bit parity for the default path

- **WHEN** the same `.html` file is opened on the 32-bit and 64-bit builds
- **THEN** both builds MUST produce identical embedded renders with the same base-href resolution
