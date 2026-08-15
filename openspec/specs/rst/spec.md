# rst Specification

## Purpose
Render reStructuredText (.rst) documents as formatted HTML inside the Total Commander Lister pane using the restructured.js library (the Node.js "restructured" library bundled for the browser) shipped under Resources/assets/rst/.
## Requirements
### Requirement: RST File Detection

The RST processor MUST claim files whose extension (case-insensitive) matches an entry listed under the `[Extensions]` RST key of edgeviewer.ini (shipped as `RST=RST`). Extension matching MUST be identical on the Win32 and x64 builds; both bitnesses claim the same set of extensions.

#### Scenario: Detecting a .rst file

- **WHEN** a file with the extension `.rst` is opened in the Lister
- **THEN** the RST processor MUST claim the file and render it
- **AND** the Lister MUST hand the file to the RST renderer instead of any other processor

#### Scenario: Case-insensitive extension match

- **WHEN** a file is named `INDEX.RST`, `Index.Rst`, or any other casing
- **THEN** the RST processor MUST claim the file regardless of letter case

#### Scenario: 32-bit and 64-bit parity for detection

- **WHEN** the plugin is loaded in the 32-bit (`EdgeViewer.wlx`) build
- **AND WHEN** the plugin is loaded in the 64-bit (`EdgeViewer.wlx64`) build
- **THEN** both builds MUST recognize the `.rst` extension with identical behavior

### Requirement: RST Rendering via the Loader Template

The RST processor MUST render by filling a loader HTML template (shipped at Resources/assets/rst/loader.html) with placeholder values and navigating the WebView to the filled string. Three placeholders MUST be substituted:

- `__BASE_URL__` MUST be replaced with the URL of the file's parent directory (so relative links/images resolve against the source folder).
- `__CSS_NAME__` MUST be replaced with the CSS filename read from `[RST]` CSS or `[RST]` CSSDark in edgeviewer.ini, depending on the dark-mode flag (see the CSS Theme requirement below).
- `__RST_FILENAME__` MUST be replaced with a URL for the opened file relative to its parent directory.

The actual reStructuredText-to-HTML conversion MUST be performed client-side inside the WebView by the restructured.js library loaded from Resources/assets/rst/. The rendering MUST be identical on Win32 and x64 because the loader template, placeholders, and library are platform-independent static assets. The shipped renderer is intentionally close in structure to the Markdown renderer and can diverge over time; both templates nevertheless share the same placeholder-substitution pattern for now, and any divergence MUST be tracked as a separate spec change.

#### Scenario: Loading an .rst file renders formatted HTML

- **WHEN** the user opens `chapter.rst` from `D:\Notes\`
- **THEN** the RST processor MUST fill the loader template with the `[RST]` CSS value, with `__BASE_URL__` pointing to `D:\Notes\`, and with `__RST_FILENAME__` pointing to `chapter.rst`
- **AND** restructured.js MUST convert the RST source to HTML in the WebView

#### Scenario: Relative links resolve against the source folder

- **WHEN** `chapter.rst` (in `D:\Notes\`) references `images/diagram.png`
- **THEN** the placeholder substitution MUST set `__BASE_URL__` to `D:\Notes\` so the nested link resolves against the file's parent directory

#### Scenario: 32-bit and 64-bit parity for rendering

- **WHEN** the same `.rst` file is opened on the 32-bit and 64-bit builds
- **THEN** both MUST load the same Resources/assets/rst/ assets and produce visually identical HTML output

### Requirement: RST CSS Theme Selection with Dark Mode Support

Unlike the AsciiDoc processor (which reads only a CSS key), the RST processor MUST read both the `[RST]` CSS key and the `[RST]` CSSDark key from edgeviewer.ini. When Total Commander signals dark mode is off, the RST processor MUST apply the stylesheet named by `[RST]` CSS. When Total Commander signals dark mode is on, the RST processor MUST apply the stylesheet named by `[RST]` CSSDark. In the shipped edgeviewer.ini both keys point at the same file (`rst-style.css`), so the observed visual output is the same in light and dark mode out of the box; however, the wiring for a separate dark stylesheet MUST be honored if a user overrides either key. The selection MUST be identical on Win32 and x64.

#### Scenario: Light mode uses the [RST] CSS value

- **WHEN** Total Commander opens a `.rst` file with the Lister dark mode flag set to off
- **THEN** the RST processor MUST apply the stylesheet named by `[RST]` CSS (shipped: `rst-style.css`)

#### Scenario: Dark mode uses the [RST] CSSDark value

- **WHEN** Total Commander opens a `.rst` file with the Lister dark mode flag set to on
- **THEN** the RST processor MUST apply the stylesheet named by `[RST]` CSSDark (shipped: `rst-style.css`)

#### Scenario: Shipping default points both keys at the same file

- **WHEN** the shipped edgeviewer.ini is used unchanged
- **THEN** `[RST] CSS` and `[RST] CSSDark` MUST both name `rst-style.css`
- **AND** the rendered output MUST be visually identical in light and dark mode, because the same stylesheet is selected in both cases

#### Scenario: A user overrides CSSDark to a different file

- **WHEN** a user edits edgeviewer.ini so that `[RST] CSSDark=my-dark.css` and `[RST] CSS=rst-style.css`
- **AND** the Lister opens a `.rst` file in dark mode
- **THEN** the RST processor MUST apply `my-dark.css`, distinct from the light-mode `rst-style.css`

#### Scenario: 32-bit and 64-bit parity for CSS selection

- **WHEN** the plugin is loaded on either the 32-bit or 64-bit build
- **THEN** both builds MUST select between `[RST]` CSS and CSSDark based on the dark-mode flag with identical behavior

### Requirement: RST Virtual Host Mapping

The RST processor MUST register two virtual host-to-filesystem mappings in the WebView before navigating:

- The `assets.example` virtual host MUST map to the plugin's own Resources/assets directory, so the loader template can load restructured.js, the CSS file, and any bundled sub-assets via `https://assets.example/...`.
- The `local.example` virtual host MUST map to the root directory of the currently opened RST file, so relative links and images inside the RST source resolve to real files on disk without leaking the user's absolute paths into the document's URL surface.

These mappings MUST be set up identically on Win32 and x64, because the host mapping logic is platform-independent and operates on absolute paths supplied by Total Commander.

#### Scenario: assets.example resolves to plugin assets

- **WHEN** the loader template references `https://assets.example/rst/restructured.js`
- **THEN** the RST processor MUST serve that request from Resources/assets/rst/restructured.js
- **AND** restructured.js MUST load successfully without any network request

#### Scenario: local.example resolves to the opened file's root

- **WHEN** a user opens `D:\Notes\chapter.rst`
- **THEN** the RST processor MUST map `local.example` to `D:\Notes\`
- **AND** any relative image reference inside the RST (e.g. `.. image:: images/diagram.png`) MUST resolve to a file under `D:\Notes\` via the `local.example` host

#### Scenario: 32-bit and 64-bit parity for virtual host mapping

- **WHEN** the plugin is loaded on either the 32-bit or 64-bit build
- **THEN** both builds MUST register `assets.example` → Resources/assets and `local.example` → the opened file's root directory with the same behavior

