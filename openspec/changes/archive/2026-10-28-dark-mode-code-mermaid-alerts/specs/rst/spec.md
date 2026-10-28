## MODIFIED Requirements

### Requirement: RST Rendering via the Loader Template

The RST processor MUST render by filling a loader HTML template (shipped at Resources/assets/rst/loader.html) with placeholder values and navigating the WebView to the filled string. The following placeholders MUST be substituted:

- `__BASE_URL__` MUST be replaced with the URL of the file's parent directory (so relative links/images resolve against the source folder).
- `__CSS_NAME__` MUST be replaced with the CSS filename read from `[RST]` CSS or `[RST]` CSSDark in edgeviewer.ini, depending on the dark-mode flag (see the CSS Theme requirement below).
- `__HLJS_CSS__` MUST be replaced with the highlight.js theme filename for the active mode: `github-dark.min.css` when dark mode is active, `github.min.css` otherwise.
- `__RST_FILENAME__` MUST be replaced with a URL for the opened file relative to its parent directory.

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

#### Scenario: Dark mode uses the dark highlight.js theme

- **WHEN** Total Commander opens a `.rst` file with the Lister dark mode flag set to on
- **THEN** the loader references `highlight_js/github-dark.min.css` via `__HLJS_CSS__`, so code blocks render on the dark body instead of the light theme's white background

#### Scenario: Light mode keeps the light highlight.js theme

- **WHEN** Total Commander opens a `.rst` file with the Lister dark mode flag set to off
- **THEN** the loader references `highlight_js/github.min.css` via `__HLJS_CSS__`, unchanged from before
