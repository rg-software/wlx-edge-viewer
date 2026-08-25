## MODIFIED Requirements

### Requirement: HTML charset override unavailable on both platforms

The `[HTML] DetectEncoding` ini key SHALL remain removed and silently ignored on both Windows and Linux, and the plugin SHALL NOT perform automatic charset detection for HTML files. Manual per-view overrides via the Encoding context menu (capability `encoding-override`) are the only plugin-provided charset control; absent a manual selection, the embedded web engine's default sniffing applies to the embedded source bytes.

> The former known-limitation note (non-UTF-8 HTML without BOM/meta mis-rendering with no user recourse) is resolved by the manual override and no longer applies.

#### Scenario: Windows user with DetectEncoding=1 in ini
- **WHEN** a Windows user has `[HTML] DetectEncoding=1` set and opens an HTML file without charset metadata
- **THEN** the file is rendered using the web engine's default charset sniffing, with no automatic plugin-side override

#### Scenario: Linux user opens an HTML file
- **WHEN** a Linux user opens an HTML file containing `<meta charset="windows-1251">`
- **THEN** Qt Web Engine honors the declared charset and renders the file correctly

#### Scenario: Linux user opens HTML without charset declaration
- **WHEN** a Linux user opens an HTML file with no BOM and no `<meta charset>`
- **THEN** Qt Web Engine sniffs the charset from content; the plugin does not intervene automatically
- **AND** the user may correct mojibake via the Encoding context menu (see `encoding-override`)
