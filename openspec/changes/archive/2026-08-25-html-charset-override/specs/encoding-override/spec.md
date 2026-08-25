## MODIFIED Requirements

### Requirement: HTML re-decode with forced charset

Selecting an encoding for an HTML file view SHALL be performed **host-side** by the owning backend: it SHALL transcode its cached pristine copy of the source bytes into Unicode with the chosen code page (Windows: `MultiByteToWideChar`; Linux: the Qt backend's ICU-backed `QStringDecoder`) and re-render the decoded text through a fresh embedded-string load, with the file's `<base href>` prepended so relative subresource references continue to resolve through the `local.example` host mapping on both platforms. Selecting "Auto-detect" SHALL re-render the pristine bytes without a forced decode (fresh engine sniffing over the original bytes) rather than reloading the current document. The override MUST NOT modify live DOM state and MUST NOT require any page-side JavaScript executor. (A `<meta charset>` splice is NOT used for forcing: the embedded-string loaders re-encode their argument to UTF-8, so a spliced declaration cannot influence the engine's decode.)

#### Scenario: Legacy HTML re-rendered

- **GIVEN** a windows-1251 HTML file with no `<meta charset>` rendering as mojibake
- **WHEN** user picks "Windows-1251" from the Encoding submenu
- **THEN** the view re-renders with correctly decoded Cyrillic text

#### Scenario: Override beats a wrong declared charset

- **GIVEN** a windows-1251 HTML file that declares `<meta charset="utf-8">`
- **WHEN** user picks "Windows-1251"
- **THEN** the view re-renders correctly, because the bytes are decoded host-side before the engine sees them

#### Scenario: Repeated overrides keep the menu functional

- **GIVEN** an HTML view already displaying under a forced encoding
- **WHEN** the user selects another encoding (or the same one again)
- **THEN** the view re-renders from the pristine bytes with the new code page and the context menu remains fully operational

#### Scenario: Reset to auto

- **GIVEN** an HTML view currently displaying under a forced encoding
- **WHEN** user picks "Auto-detect"
- **THEN** the view re-renders the pristine bytes with engine-default sniffing (the original mojibake)

### Requirement: Decode failure handling

If the chosen label cannot be mapped to a code page (or the bytes fail to decode), the view SHALL fall back to the pristine-bytes render (identical to the initial sniffed view) — the view SHALL remain rendered and usable (possibly mojibake), never blank; no error dialog is required.

#### Scenario: Invalid decode falls back safely

- **WHEN** the backend is asked to re-render with an unmappable/unknown label
- **THEN** the view displays the pristine sniffed render and stays usable

### Requirement: Cross-platform parity

The encoding menu and re-decode behavior SHALL work identically on Windows (WebView2 backend, Win32 and x64 builds) and Linux (Qt Web Engine backend, x64 build), driven by the shared encoding list (`EdgeViewer/EncodingList.h`) and the shared `CharsetOverride` helpers. No JS-to-host commands are added: the host-owned menus dispatch picks directly to their backend (`ApplyCharsetOverride`).

#### Scenario: Same flow on both backends

- **WHEN** the manual-encoding scenario set is exercised on Windows and on Linux
- **THEN** all scenarios above pass with no platform-specific steps
