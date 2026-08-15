# markdown Specification

## Purpose
Lets users view Markdown files (`.md`, `.markdown`) rendered as formatted HTML inside the Total Commander lister, using the shared `Resources/assets/markdown/` loader template and the `marked.js` rendering library.
## Requirements
### Requirement: Markdown file detection

The lister SHALL route files whose extension matches the `Markdown` extension set from the `[Extensions]` section of `edgeviewer.ini` (e.g. `MD,MARKDOWN`) to the Markdown processor (`EdgeViewer/Processors/MdProcessor.cpp`, `Resources/assets/markdown/`). Detection SHALL be case-insensitive and SHALL behave identically on 32-bit and 64-bit builds.

#### Scenario: Opening a .md file

- **WHEN** the user opens a file named `readme.md` in Total Commander
- **THEN** the Markdown processor renders the file instead of any fallback

#### Scenario: Non-matching extension

- **WHEN** the user opens a `.txt` file
- **THEN** the Markdown processor is not selected

### Requirement: Markdown rendering via loader template

The Markdown processor SHALL render by loading the template at `Resources/assets/markdown/loader.html`, replacing the `__BASE_URL__` placeholder with the URL-encoded parent directory of the file, the `__CSS_NAME__` placeholder with the CSS filename from the `[Markdown]` ini section, and the `__MD_FILENAME__` placeholder with the URL-encoded relative path of the file. The filled template SHALL be loaded into the web view as an HTML string (not navigated to a URL). The marked.js library running inside the loader SHALL parse the Markdown file from the `local.example` virtual host and render it as formatted HTML with syntax highlighting (highlight.js), MathJax, and Mermaid diagram support.

#### Scenario: Markdown with code block

- **WHEN** the user opens a Markdown file containing a fenced code block with a language hint (e.g. ` ```cpp `)
- **THEN** the rendered view shows the code block with syntax highlighting applied by highlight.js

#### Scenario: Markdown with mathematical formula

- **WHEN** the user opens a Markdown file containing a LaTeX formula
- **THEN** the rendered view shows the formula typeset by MathJax

#### Scenario: Markdown with Mermaid diagram

- **WHEN** the user opens a Markdown file containing a Mermaid code block
- **THEN** the rendered view shows the diagram rendered by the Mermaid library

### Requirement: Markdown CSS theme selection

The Markdown processor SHALL select the CSS file from the `[Markdown]` section of `edgeviewer.ini`: `CSS` for light mode and `CSSDark` for dark mode (when `lcp_darkmode` is set in `ShowFlags`). The CSS file SHALL be loaded from the `asciidoctor` asset directory under `Resources/assets/markdown/`. The selection SHALL be identical on 32-bit and 64-bit builds.

#### Scenario: Light mode CSS

- **WHEN** the user opens a Markdown file with `lcp_darkmode` not set and `[Markdown] CSS=github.css`
- **THEN** the rendered view uses `github.css` for styling

#### Scenario: Dark mode CSS

- **WHEN** the user opens a Markdown file with `lcp_darkmode` set and `[Markdown] CSSDark=github.dark.css`
- **THEN** the rendered view uses `github.dark.css` for styling

### Requirement: Markdown virtual host mapping

The Markdown processor SHALL map the `assets.example` virtual host to the plugin's `Resources/assets/` directory and the `local.example` virtual host to the root directory of the file's drive before loading the template. This allows the loader HTML to reference the Markdown file via `http://local.example/...` and the rendering libraries via `http://assets.example/markdown/...`.

#### Scenario: Markdown file on C: drive

- **WHEN** the user opens `C:\Users\test\readme.md`
- **THEN** the `local.example` host maps to `C:\` and the loader can fetch the file at `http://local.example/Users/test/readme.md`

