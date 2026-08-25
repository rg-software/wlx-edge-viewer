## MODIFIED Requirements

### Requirement: HTML re-decode with forced charset

Selecting an encoding for an HTML file view SHALL be performed **host-side** by the owning backend: it SHALL splice `<meta charset="<tag>">` into its cached pristine copy of the source bytes (at the head of the stream, per the insertion rules of the implementation) and re-render the result through a fresh embedded-string load, so the web engine's own HTML parser performs the decoding (first charset declaration in the prescan window wins). A `<base href>` referencing the opened file's directory SHALL be spliced alongside so relative subresource references continue to resolve through the `local.example` host mapping on both platforms. Selecting "Auto-detect" SHALL re-render the pristine bytes without a charset meta (fresh engine sniffing) rather than reloading the current document. The override MUST NOT modify live DOM state and MUST NOT require any page-side JavaScript executor.

#### Scenario: Legacy HTML re-rendered

- **GIVEN** a windows-1251 HTML file with no `<meta charset>` rendering as mojibake
- **WHEN** user picks "Windows-1251" from the Encoding submenu
- **THEN** the view re-renders with correctly decoded Cyrillic text

#### Scenario: Override beats a wrong declared charset

- **GIVEN** a windows-1251 HTML file that declares `<meta charset="utf-8">`
- **WHEN** user picks "Windows-1251"
- **THEN** the view re-renders correctly, because the spliced declaration precedes the file's own

#### Scenario: Repeated overrides keep the menu functional

- **GIVEN** an HTML view already displaying under a forced encoding
- **WHEN** the user selects another encoding (or the same one again)
- **THEN** the view re-renders from the pristine bytes with the new tag and the context menu remains fully operational

#### Scenario: Reset to auto

- **GIVEN** an HTML view currently displaying under a forced encoding
- **WHEN** user picks "Auto-detect"
- **THEN** the view re-renders from the pristine bytes with engine-default sniffing

### Requirement: Decode failure handling

If the chosen label is not a recognized charset name, the spliced document SHALL fall back to the engine's default decoding during the fresh render — the view SHALL remain rendered and usable (possibly mojibake), never blank; no error dialog is required.

#### Scenario: Invalid decode falls back safely

- **WHEN** the backend renders with a label the engine does not recognize
- **THEN** the view displays using default decoding and stays usable

### Requirement: Cross-platform parity

The encoding menu and re-decode behavior SHALL work identically on Windows (WebView2 backend, Win32 and x64 builds) and Linux (Qt Web Engine backend, x64 build), driven by the shared encoding list (`EdgeViewer/EncodingList.h`) and a shared byte-splice helper (`EdgeViewer/CharsetOverride`). No JS-to-host commands are added: the host-owned menus dispatch picks directly to their backend (`ApplyCharsetOverride`).

#### Scenario: Same flow on both backends

- **WHEN** the manual-encoding scenario set is exercised on Windows and on Linux
- **THEN** all scenarios above pass with no platform-specific steps
