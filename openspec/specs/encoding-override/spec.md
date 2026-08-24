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

Selecting an encoding for an HTML file view SHALL cause the page (via the document-created bootstrap already registered by both backends for CSS injection) to fetch the file bytes from its own origin (`local.example` virtual host), decode them with the chosen `TextDecoder` label, and replace the current document with the result. Relative subresource references SHALL continue to resolve against the same origin.

#### Scenario: Legacy HTML re-rendered

- **GIVEN** a windows-1251 HTML file with no `<meta charset>` rendering as mojibake
- **WHEN** user picks "Windows-1251" from the Encoding submenu
- **THEN** the page re-displays with correctly decoded content and its local images/stylesheets still load

#### Scenario: Reset to auto

- **GIVEN** an HTML view currently displaying under a forced encoding
- **WHEN** user picks "Auto-detect" (or reloads the file via F5/lister refresh)
- **THEN** the page re-renders with engine default sniffing

### Requirement: Transient scope of the override

The selected encoding SHALL apply only to the current rendered view. It MUST NOT persist across navigations, `ListLoadNextW` refreshes, window close/reopen, plugin restarts, or sessions; no ini key or on-disk state SHALL be introduced. Automatic detection remains the default path on every load.

#### Scenario: Override cleared by next load

- **GIVEN** an HTML view showing under a forced encoding
- **WHEN** the same file is reopened in a fresh lister window (or another file is loaded into the view)
- **THEN** rendering uses automatic detection again

### Requirement: Decode failure handling

If the chosen label is rejected by `TextDecoder` or decoding throws, the view SHALL keep the previously rendered content (or show a short inline error notice) instead of a blank page.

#### Scenario: Invalid decode falls back safely

- **WHEN** decoding the fetched bytes with the chosen label throws
- **THEN** the view does not go blank and remains usable (previous content or error notice)

### Requirement: Cross-platform parity

The encoding menu and re-decode behavior SHALL work identically on Windows (WebView2 backend, Win32 and x64 builds) and Linux (Qt Web Engine backend, x64 build), driven by a single shared encoding list (`EdgeViewer/EncodingList.h`). No JS-to-host commands are added: the host-owned menus dispatch picks into the page via `ExecuteScript` (WebView2) / `runJavaScript` (Qt Web Engine) on the `window.__evEncodingApply` executor.

#### Scenario: Same flow on both backends

- **WHEN** the manual-encoding scenario set is exercised on Windows and on Linux
- **THEN** all scenarios above pass with no platform-specific steps
