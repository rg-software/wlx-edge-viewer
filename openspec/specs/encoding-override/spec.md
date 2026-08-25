# encoding-override Specification

## Purpose
Lets users fix mojibake in legacy-encoded archived documents by choosing a character encoding manually from a right-click menu inside the rendered view, without any persistent configuration or automatic-detection changes.

## Requirements
### Requirement: Encoding context menu on HTML and MHT views

Right-clicking inside the rendered view of an **HTML** file (`HtmlProcessor`) or an **MHT** archive (`MhtProcessor`) SHALL extend the engine's built-in context menu with an "Encoding" submenu (the standard browser entries remain). The submenu SHALL NOT appear on other processor views (images, directories, PDF/other fallback, URL files) or on text-loader views (Markdown, RST, AsciiDoc, EML), which keep the stock menu.

#### Scenario: Menu opens on an HTML file

- **WHEN** user right-clicks anywhere in the view of a `.html` file
- **THEN** the engine's default context menu is shown including an "Encoding" submenu

#### Scenario: Menu opens on an MHT file

- **WHEN** user right-clicks anywhere in the view of a `.mht`/`.mhtml` file
- **THEN** the engine's default context menu is shown including an "Encoding" submenu

#### Scenario: No menu on excluded types

- **WHEN** user right-clicks in a Markdown, RST, AsciiDoc, EML, image, directory, or PDF/other view
- **THEN** no encoding menu appears (existing behavior of that view is unchanged)

### Requirement: Encoding list contents

The context menu SHALL offer "Auto-detect" plus a fixed curated list of common code pages supported by the web engine's `TextDecoder` (e.g., UTF-8, UTF-16LE, windows-1250/1251/1252/1254/1255/1256/1257/1258, KOI8-R, ISO-8859-x family, windows-874, GBK/GB2312, Big5, Shift-JIS, EUC-JP, EUC-KR). The list SHALL be identical on both platforms.

#### Scenario: Common legacy code pages are offered

- **WHEN** user opens the Encoding submenu on an HTML or MHT view
- **THEN** entries include at minimum UTF-8, Auto-detect, windows-1251, KOI8-R, windows-1252, Shift-JIS, and GB2312

### Requirement: MHT re-decode with forced charset

Selecting an encoding for an MHT view SHALL cause `Resources/assets/mhtml/loader.html` to decode the file bytes with the chosen `TextDecoder` label before passing the source string to `mhtml2html.convert()`. Selecting "Auto-detect" SHALL restore the stock mhtml2html behavior (header/meta-driven decoding).

#### Scenario: Mojibake fixed by manual selection

- **GIVEN** a windows-1251 MHT whose part headers omit or misstate the charset so it renders as mojibake
- **WHEN** user picks "Windows-1251" from the Encoding submenu
- **THEN** the view re-renders with correctly decoded Cyrillic text

#### Scenario: Reset to auto

- **GIVEN** an MHT view currently displaying under a forced encoding
- **WHEN** user picks "Auto-detect"
- **THEN** the view re-renders exactly as it did on initial load

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
- **THEN** the view re-renders from the pristine bytes with the new tag and the context menu remains fully operational

#### Scenario: Reset to auto

- **GIVEN** an HTML view currently displaying under a forced encoding
- **WHEN** user picks "Auto-detect"
- **THEN** the view re-renders from the pristine bytes with engine-default sniffing

### Requirement: Transient scope of the override

The selected encoding SHALL apply only to the current rendered view. It MUST NOT persist across navigations, `ListLoadNextW` refreshes, window close/reopen, plugin restarts, or sessions; no ini key or on-disk state SHALL be introduced. Automatic detection remains the default path on every load.

#### Scenario: Override cleared by next load

- **GIVEN** an HTML view showing under a forced encoding
- **WHEN** the same file is reopened in a fresh lister window (or another file is loaded into the view)
- **THEN** rendering uses automatic detection again

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
