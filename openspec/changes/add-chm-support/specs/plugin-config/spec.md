## MODIFIED Requirements

### Requirement: [Extensions] section

The `[Extensions]` section MUST map each type name to a comma-separated, uppercase list of file extensions that the corresponding processor claims. The shipped section defines `HTML=HTM,HTML,XHTML,XML`, `Markdown=MD,MARKDOWN`, `AsciiDoc=ADOC,ASCIIDOC`, `URL=URL`, `MHTML=MHT,MHTML`, `CHM=CHM`, `EML=EML`, `RST=RST`, `Images=PNG,GIF,BMP,JPG,JPEG,ICO,WEBP,SVG`, and `Other=PDF`. The section MUST additionally support the `Dirs` flag (`Dirs=1` makes the plugin claim directory paths via an empty-extension detect term; `Dirs=0` or absent disables directory handling) and the `ForcedHtmlExt` key (a `|`-separated, case-insensitive regex of extensions that are force-rendered as HTML, shipped as `xml|xhtml`). Type matching against these lists SHALL be case-insensitive and SHALL produce identical results on 32-bit and 64-bit builds.

#### Scenario: Adding a new extension to a type

- **WHEN** the user edits `[Extensions] Markdown=MD,MARKDOWN,MDOWN` and reloads the plugin
- **THEN** `.mdown` files are claimed by the Markdown processor

#### Scenario: Disabling directory handling

- **WHEN** the user sets `Dirs=0` (or removes the `Dirs` key) and reloads the plugin
- **THEN** directory paths are no longer claimed by the plugin (the detect string gains no trailing `EXT=""` term)

#### Scenario: Case-insensitive extension match

- **WHEN** the user opens a file named `IMAGE.PNG` (uppercase) and `Images=PNG,...`
- **THEN** the Images processor claims the file, because extension matching is case-insensitive

#### Scenario: CHM extension is claimed by the CHM processor

- **WHEN** the user opens a file named `manual.chm` and `[Extensions]` contains `CHM=CHM`
- **THEN** the CHM processor claims the file, and the same happens on the 32-bit and 64-bit builds

### Requirement: Per-type stylesheet sections

The plugin MUST read a per-type stylesheet section for each rendered type — `[HTML]`, `[Markdown]`, `[AsciiDoc]`, `[RST]`, `[EML]`, `[CHM]`, `[Images]`, and `[Directory]` — each keyed by `CSS` for the light-mode stylesheet and `CSSDark` for the dark-mode stylesheet (selected when the `lcp_darkmode` flag is active). The shipped `edgeviewer.ini` defines `CSS`/`CSSDark` pairs for `[HTML]`, `[Markdown]`, `[RST]`, `[EML]`, `[CHM]`, `[Images]`, and `[Directory]`; `[AsciiDoc]` ships with `CSS` only (no `CSSDark` key, so dark-mode AsciiDoc has no dark-specific stylesheet configured in the ini). Stylesheet files are loaded from each type's `Resources/assets/<type>/` folder via the `assets.example` virtual host. The `[Images]` section additionally defines `FitToScreen` (`1` fits the image to the viewport, `0` shows it at native size; toggle at runtime with F). The `[HTML] DetectEncoding` key was removed and MUST be silently ignored. Stylesheet selection SHALL be identical on 32-bit and 64-bit builds.

#### Scenario: Light vs dark stylesheet for Markdown

- **WHEN** `[Markdown] CSS=github.css` and `[Markdown] CSSDark=github.dark.css`
- **THEN** light-mode Markdown views use `github.css` and dark-mode Markdown views (lcp_darkmode set) use `github.dark.css`

#### Scenario: AsciiDoc has no dark stylesheet key

- **WHEN** `lcp_darkmode` is set and the user opens an AsciiDoc file
- **THEN** no `CSSDark` value is read for AsciiDoc (the shipped section omits the key), so no dark-specific stylesheet is applied from the ini

#### Scenario: HTML encoding override enabled

- **WHEN** `[HTML] DetectEncoding=1` is set and the user opens an HTML file
- **THEN** the key is ignored (the encoding-override mechanism was removed on both platforms); the WebView's built-in charset sniffing is the only path

#### Scenario: Light vs dark stylesheet for CHM

- **WHEN** `[CHM] CSS=chm.css` and `[CHM] CSSDark=chm.dark.css` are set and the user opens a CHM with `lcp_darkmode` toggled
- **THEN** light-mode CHM views use `chm.css` and dark-mode CHM views use `chm.dark.css` for the sidebar and page chrome
