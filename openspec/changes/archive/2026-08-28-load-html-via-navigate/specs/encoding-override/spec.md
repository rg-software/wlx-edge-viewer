## MODIFIED Requirements

### Requirement: HTML re-decode with forced charset

Selecting an encoding for an HTML file view SHALL be performed **host-side** by the owning backend: it SHALL transcode its cached pristine copy of the source bytes into Unicode with the chosen code page (Windows: `MultiByteToWideChar`; Linux: the Qt backend's ICU-backed `QStringDecoder`) and re-render the decoded text through a fresh embedded-string load, with the file's `<base href>` prepended so relative subresource references continue to resolve through the `local.example` host mapping on both platforms. Picking "Auto-detect" SHALL re-render the pristine bytes without a forced decode (fresh engine sniffing over the original bytes) rather than reloading the current document. This embedded re-decode is the **override/auto-detect path only**; the *default* render of an HTML file is a real `local.example` navigation (see `html`) in which the engine decodes the actual bytes and which therefore needs no `BytesToLatin1` or transcode. The override MUST NOT modify live DOM state and MUST NOT require any page-side JavaScript executor. (A `<meta charset>` splice is NOT used for forcing: the embedded-string loaders re-encode their argument to UTF-8, so a spliced declaration cannot influence the engine's decode.)

#### Scenario: Legacy HTML re-rendered

- **GIVEN** a windows-1251 HTML file with no `<meta charset>` rendering as mojibake
- **WHEN** user picks "Windows-1251" from the Encoding submenu
- **THEN** the view re-renders with correctly decoded Cyrillic text via the embedded re-decode path

#### Scenario: Override beats a wrong declared charset

- **GIVEN** a windows-1251 HTML file that declares `<meta charset="utf-8">`
- **WHEN** user picks "Windows-1251"
- **THEN** the view re-renders correctly, because the bytes are decoded host-side before the engine sees them

#### Scenario: Repeated overrides keep the menu functional

- **GIVEN** an HTML view already displaying under a forced encoding
- **WHEN** the user selects another encoding (or the same one again)
- **THEN** the view re-renders from the pristine bytes with the new tag and the context menu remains fully operational

#### Scenario: Reset to auto

- **GIVEN** an HTML view currently displaying under a forced encoding
- **WHEN** user picks "Auto-detect"
- **THEN** the view re-renders from the pristine bytes with engine-default sniffing

### Requirement: HTML auto-detection (provisional)

For an HTML view, the pristine bytes cached via `SetRawFileBytes` SHALL be statistically detected (jschardet) when the file carries no authoritative declared encoding and the user has not disabled detection; when the detected code page disagrees with the charset the engine actually decoded with (`document.characterSet`), the host SHALL provisionally re-render through the embedded re-decode path with the detected code page. When the detected code page agrees with the engine's charset, or the file carries an authoritative declared encoding, the host SHALL NOT re-render and SHALL only surface the resolved charset so the Encoding submenu may show an "Auto: <code page>" entry. This auto-detect behavior SHALL be unaffected by whether the default render was a real `local.example` navigation or an embedded load, because it operates on the pristine bytes independent of the initial load mechanism.

#### Scenario: Disagreement triggers a provisional re-decode

- **WHEN** an HTML file renders via `local.example` navigation with an engine charset (e.g. `windows-1252`) that disagrees with jschardet's high-confidence detection (e.g. `windows-1251`)
- **THEN** the host SHALL re-render through the embedded re-decode path with `windows-1251`

#### Scenario: Agreement reports without re-rendering

- **WHEN** jschardet's detection for the pristine bytes agrees with the engine's `document.characterSet`
- **THEN** the host SHALL NOT re-render
- **AND** the Encoding submenu SHALL surface "Auto: <code page>" for the agreed charset

#### Scenario: Declared encoding is authoritative

- **WHEN** the file carries an authoritative declared encoding (BOM / `<meta charset>` / http-equiv) and auto-detect-force is off
- **THEN** the host SHALL NOT statistically re-decode
- **AND** SHALL surface the engine's resolved charset for the "Auto:" menu entry
