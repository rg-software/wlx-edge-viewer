## MODIFIED Requirements

### Requirement: Encoding context menu on HTML and MHT views

Right-clicking inside the rendered view of an **HTML** file (`HtmlProcessor`), an **MHT** archive (`MhtProcessor`), or an **EML** message (`EmProcessor`) SHALL extend the engine's built-in context menu with an "Encoding" submenu (the standard browser entries remain). The submenu SHALL NOT appear on other processor views (images, directories, PDF/other fallback, URL files) or on the other text-loader views (Markdown, RST, AsciiDoc), which keep the stock menu.

#### Scenario: Menu opens on an HTML file

- **WHEN** user right-clicks anywhere in the view of a `.html` file
- **THEN** the engine's default context menu is shown including an "Encoding" submenu

#### Scenario: Menu opens on an MHT file

- **WHEN** user right-clicks anywhere in the view of a `.mht`/`.mhtml` file
- **THEN** the engine's default context menu is shown including an "Encoding" submenu

#### Scenario: Menu opens on an EML file

- **WHEN** user right-clicks anywhere in the view of a `.eml` file
- **THEN** the engine's default context menu is shown including an "Encoding" submenu

#### Scenario: No menu on excluded types

- **WHEN** user right-clicks in a Markdown, RST, AsciiDoc, image, directory, or PDF/other view
- **THEN** no encoding menu appears (existing behavior of that view is unchanged)

## ADDED Requirements

### Requirement: EML body re-decode with forced charset

Selecting an encoding for an EML view SHALL cause `Resources/assets/eml/loader.html` to decode the rendered body part's transfer-decoded bytes with the chosen `TextDecoder` label and re-render **only the body**, keeping the RFC 2047 header block and the attachment footer unchanged. For an HTML body the re-render SHALL rebuild the body's HTML (preserving inline cid-referenced images) from the re-decoded text; for a plain-text body it SHALL render the re-decoded text with line breaks preserved. Selecting "Auto-detect" SHALL restore the stock render (the loader's normal declared-charset/UTF-8 decoding, or the sniffed auto result). A failed page-side re-decode SHALL keep the previous render, post `CMD_ENCODING_APPLY_FAILED` to the host, and re-run detection — the same unappliable-pick semantics as MHT.

#### Scenario: Mojibake fixed by manual selection

- **GIVEN** a Windows-1251 EML whose body part omits or misstates the charset so it renders as mojibake
- **WHEN** user picks "Windows-1251" from the Encoding submenu
- **THEN** the view re-renders the body with correctly decoded Cyrillic text while the headers and attachments are unchanged

#### Scenario: Reset to auto

- **GIVEN** an EML view currently displaying under a forced encoding
- **WHEN** user picks "Auto-detect"
- **THEN** the view re-renders exactly as it did on initial load

#### Scenario: Unappliable EML pick reverts to auto-detect

- **GIVEN** an EML view whose loader previously rendered correctly (auto or declared charset)
- **WHEN** the user picks a code page that the loader's page-side body re-decode cannot apply (its `__evEncodingApply` render throws)
- **THEN** the view keeps showing the previous render (no partial body), the failed entry is not shown as checked, and the Auto-detect entry goes back to being checked with its "Auto: <tag>" label restored