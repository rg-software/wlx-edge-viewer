# asciidoc Specification

## Purpose
Render AsciiDoc (.adoc, .asciidoc) documents as formatted HTML inside the Total Commander Lister pane using the asciidoctor.js library shipped under Resources/assets/asciidoctor/.
## Requirements
### Requirement: AsciiDoc File Detection

The AsciiDoc processor MUST claim files whose extension (case-insensitive) matches an entry listed under the `[Extensions]` AsciiDoc key of edgeviewer.ini (shipped as `AsciiDoc=ADOC,ASCIIDOC`). Extension matching MUST be identical on the Win32 and x64 builds; both bitnesses claim the same set of extensions.

#### Scenario: Detecting a regular .adoc file

- **WHEN** a file with the extension `.adoc` is opened in the Lister
- **THEN** the AsciiDoc processor MUST claim the file and render it
- **AND** the Lister MUST hand the file to the AsciiDoc renderer instead of any other processor

#### Scenario: Detecting a long-form .asciidoc file

- **WHEN** a file with the extension `.asciidoc` is opened
- **THEN** the AsciiDoc processor MUST claim the file, because `ASCIIDOC` is an entry under `[Extensions]` AsciiDoc

#### Scenario: Case-insensitive extension match

- **WHEN** a file is named `EXAMPLE.ADOC`, `Example.AsciiDoc`, or any other casing
- **THEN** the AsciiDoc processor MUST claim the file regardless of letter case

#### Scenario: 32-bit and 64-bit parity for detection

- **WHEN** the plugin is loaded in the 32-bit (`EdgeViewer.wlx`) build
- **AND WHEN** the plugin is loaded in the 64-bit (`EdgeViewer.wlx64`) build
- **THEN** both builds MUST recognize the `.adoc` and `.asciidoc` extensions with identical behavior

### Requirement: AsciiDoc Rendering via the Loader Template

The AsciiDoc processor MUST render by filling a loader HTML template (shipped at Resources/assets/asciidoctor/loader.html) with placeholder values and navigating the WebView to the filled string. Three placeholders MUST be substituted:

- `__BASE_URL__` MUST be replaced with the URL of the file's parent directory (so relative links/images resolve against the source folder).
- `__CSS_NAME__` MUST be replaced with the CSS filename read from `[AsciiDoc]` CSS in edgeviewer.ini (shipped as `asciidoctor.css`).
- `__ADOC_FILENAME__` MUST be replaced with a URL for the opened file relative to its parent directory.

The actual AsciiDoc-to-HTML conversion MUST be performed client-side inside the WebView by the asciidoctor.js library loaded from Resources/assets/asciidoctor/. The rendering MUST be identical on Win32 and x64 because the loader template, placeholders, and library are platform-independent static assets.

#### Scenario: Loading an .adoc file renders formatted HTML

- **WHEN** the user opens `readme.adoc` from `C:\Docs\`
- **THEN** the AsciiDoc processor MUST fill the loader template with `__CSS_NAME__` = `asciidoctor.css`, with `__BASE_URL__` pointing to `C:\Docs\`, and with `__ADOC_FILENAME__` pointing to `readme.adoc`
- **AND** asciidoctor.js MUST convert the AsciiDoc source to HTML in the WebView

#### Scenario: Relative links resolve against the source folder

- **WHEN** `readme.adoc` (in `C:\Docs\`) contains a link to `glossary.adoc`
- **THEN** the placeholder substitution MUST set `__BASE_URL__` to `C:\Docs\` so the nested link resolves against the file's parent directory

#### Scenario: Off-network rendering of FontAwesome icons

- **WHEN** the opened AsciiDoc source uses FontAwesome icon directives (e.g. `icon:github[]`)
- **AND** the host machine has no network access
- **THEN** asciidoctor.js MUST still run and render the document body, but FontAwesome icon glyphs MAY be unavailable or unstyled, because the AsciiDoc renderer ships without a fully offline FontAwesome bundle
- **AND** the loader template MUST document this known gap (the shipped asciidoctor loader comments note the intent to ship fully offline asciidoc, including FontAwesome, has not yet been carried through)

#### Scenario: 32-bit and 64-bit parity for rendering

- **WHEN** the same `.adoc` file is opened on the 32-bit and 64-bit builds
- **THEN** both MUST load the same Resources/assets/asciidoctor/ assets and produce visually identical HTML output

### Requirement: AsciiDoc CSS Theme Selection (No Dark Mode Variant)

Unlike the Markdown, RST, EML, images, and directory renderers — all of which read both `[<Type>] CSS` and `[<Type>] CSSDark` keys and switch between then-notion of dark mode — the AsciiDoc processor MUST read ONLY the `[AsciiDoc] CSS` key from edgeviewer.ini (shipped as `asciidoctor.css`). The AsciiDoc processor MUST NOT read any `[AsciiDoc] CSSDark` value, and such a value MUST have no effect even if a user adds one to edgeviewer.ini. As a consequence, the AsciiDoc rendering MUST use the same stylesheet regardless of whether Total Commander has informed the plugin that dark mode is active. This is an intentional behavioral difference from the other document renderers and is documented here for characterization, not as a defect.

#### Scenario: AsciiDoc uses the same CSS in light and dark mode

- **WHEN** Total Commander opens a `.adoc` file with the Lister dark mode flag set to off
- **AND WHEN** Total Commander opens the same file with the Lister dark mode flag set to on
- **THEN** both renders MUST use the stylesheet named by `[AsciiDoc] CSS` (shipped: `asciidoctor.css`)
- **AND** the two renders MUST look visually identical, because no `[AsciiDoc] CSSDark` value is consulted

#### Scenario: A manually-added CSSDark key is ignored for AsciiDoc

- **WHEN** a user edits edgeviewer.ini to add `[AsciiDoc] CSSDark=my-dark.css`
- **AND** the Lister opens a `.adoc` file in dark mode
- **THEN** the AsciiDoc processor MUST still apply only `asciidoctor.css` (the `[AsciiDoc] CSS` value) and MUST NOT apply `my-dark.css`

#### Scenario: Contrast with other renderers that DO support dark mode

- **WHEN** a Markdown, RST, EML, image, or directory file is opened
- **THEN** those processors MUST consult their `[<Type>] CSSDark` key in dark mode
- **AND** the AsciiDoc processor is the documented exception because it has no CSSDark key wired up

### Requirement: AsciiDoc Virtual Host Mapping

The AsciiDoc processor MUST register two virtual host-to-filesystem mappings in the WebView before navigating:

- The `assets.example` virtual host MUST map to the plugin's own Resources/assets directory, so the loader template can load asciidoctor.js, the CSS file, and any bundled sub-assets via `https://assets.example/...`.
- The `local.example` virtual host MUST map to the root directory of the currently opened AsciiDoc file, so relative links and images inside the AsciiDoc source resolve to real files on disk without leaking the user's absolute paths into the document's URL surface.

These mappings MUST be set up identically on Win32 and x64, because the host mapping logic is platform-independent and operates on absolute paths supplied by Total Commander.

#### Scenario: assets.example resolves to plugin assets

- **WHEN** the loader template references `https://assets.example/asciidoctor.js`
- **THEN** the AsciiDoc processor MUST serve that request from Resources/assets/asciidoctor/asciidoctor.js
- **AND** asciidoctor.js MUST load successfully without any network request

#### Scenario: local.example resolves to the opened file's root

- **WHEN** a user opens `C:\Docs\readme.adoc`
- **THEN** the AsciiDoc processor MUST map `local.example` to `C:\Docs\`
- **AND** any relative image reference inside the AsciiDoc (e.g. `image::diagram.png[]`) MUST resolve to a file under `C:\Docs\` via the `local.example` host

#### Scenario: 32-bit and 64-bit parity for virtual host mapping

- **WHEN** the plugin is loaded on either the 32-bit or 64-bit build
- **THEN** both builds MUST register `assets.example` → Resources/assets and `local.example` → the opened file's root directory with the same behavior

