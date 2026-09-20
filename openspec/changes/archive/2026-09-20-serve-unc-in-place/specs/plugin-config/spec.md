## MODIFIED Requirements

### Requirement: ForcedHtmlExt forced-HTML rendering

On Windows, files whose extension matches the `ForcedHtmlExt` regex in `[Extensions]` (shipped as `xml|xhtml`, matched case-insensitively against the file's full path) MUST NOT be relocated: they MUST be served from their real filesystem location through the host-side `evh://` custom scheme, whose `WebResourceRequested` handler answers with `Content-Type: text/html`, so that Edge/WebView2 treats the content as an HTML document rather than applying its native XML tree rendering. No temp file is created and nothing needs cleanup. On Linux, the `ev://` scheme handler's default `Content-Type: text/html` achieves the same result. Non-matching files SHALL be rendered from their original path.

#### Scenario: XHTML file forced to HTML

- **WHEN** the user opens `page.xhtml` and `[Extensions] ForcedHtmlExt=xml|xhtml`
- **THEN** the file is rendered in place as HTML from its original path through the `evh://` scheme (no temp copy)

#### Scenario: Non-listed extension is not forced

- **WHEN** the user opens `data.xml` but `ForcedHtmlExt` is empty or removed
- **THEN** the file is rendered from its original path with no special content-type handling