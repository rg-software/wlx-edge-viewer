## MODIFIED Requirements

### Requirement: Markdown rendering via loader template

The Markdown processor SHALL render by loading the template at `Resources/assets/markdown/loader.html`, replacing the `__BASE_URL__` placeholder with the URL-encoded parent directory of the file, the `__CSS_NAME__` placeholder with the CSS filename from the `[Markdown]` ini section, the `__HLJS_CSS__` placeholder with the highlight.js theme filename for the active mode, the `__ALERT_CSS__` placeholder with the alert-callout stylesheet filename for the active mode, the `__IS_DARK__` placeholder with `true`/`false` for the active mode, and the `__MD_FILENAME__` placeholder with the URL-encoded relative path of the file. The filled template SHALL be loaded into the web view as an HTML string (not navigated to a URL). The marked.js library running inside the loader SHALL parse the Markdown file from the `local.example` virtual host and render it as formatted HTML with syntax highlighting (highlight.js), MathJax, and Mermaid diagram support.

#### Scenario: Markdown with code block

- **WHEN** the user opens a Markdown file containing a fenced code block with a language hint (e.g. ` ```cpp `)
- **THEN** the rendered view shows the code block with syntax highlighting applied by highlight.js

#### Scenario: Markdown with mathematical formula

- **WHEN** the user opens a Markdown file containing a LaTeX formula
- **THEN** the rendered view shows the formula typeset by MathJax

#### Scenario: Markdown with Mermaid diagram

- **WHEN** the user opens a Markdown file containing a Mermaid code block
- **THEN** the rendered view shows the diagram rendered by the Mermaid library

#### Scenario: Dark mode uses the dark highlight.js theme

- **WHEN** the user opens a Markdown file with `lcp_darkmode` set
- **THEN** the loader references `highlight_js/github-dark.min.css` via `__HLJS_CSS__`, so code blocks render on the dark body instead of the light theme's white background

#### Scenario: Light mode keeps the light highlight.js theme

- **WHEN** the user opens a Markdown file with `lcp_darkmode` not set
- **THEN** the loader references `highlight_js/github.min.css` via `__HLJS_CSS__`, unchanged from before

#### Scenario: Dark mode uses the dark Mermaid theme

- **WHEN** the user opens a Markdown file containing a Mermaid code block with `lcp_darkmode` set
- **THEN** the loader initializes Mermaid with the `dark` theme (selected via `window.__IS_DARK__`), including for in-viewer same-folder navigation renders

#### Scenario: Dark mode uses the dark alert stylesheet

- **WHEN** the user opens a Markdown file containing an alert callout (e.g. `> [!NOTE]`) with `lcp_darkmode` set
- **THEN** the loader references `markdown/github.markdown.alert.dark.css` via `__ALERT_CSS__`, so the callout renders with a dark tinted background instead of the light `#f6f8fa` background

#### Scenario: Light mode keeps the light alert stylesheet

- **WHEN** the user opens a Markdown file containing an alert callout with `lcp_darkmode` not set
- **THEN** the loader references `markdown/github.markdown.alert.css` via `__ALERT_CSS__`, unchanged from before
