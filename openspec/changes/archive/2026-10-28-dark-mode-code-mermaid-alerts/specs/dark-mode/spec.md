## MODIFIED Requirements

### Requirement: CSS selection per processor

Each processor that renders structured content SHALL read style overrides from its own section of `edgeviewer.ini`. When dark mode is active, processors that declare a `CSSDark` key SHALL switch to that override; processors that only declare `CSS` SHALL keep their document styling unchanged. The Markdown, RST, EML, Images, Directory and HTML processors SHALL support the `CSS`/`CSSDark` pair (the HTML processor applies its styles through an injected script rather than through a loader placeholder, but the observable effect is the same: the style sheet swap happens when the dark mode flag is set). The AsciiDoc processor only declares a `CSS` key and has no `CSSDark` entry, so toggling dark mode SHALL NOT change the AsciiDoc document styling (code-block syntax highlighting alone follows the dark flag — see below). Independently of the `CSS`/`CSSDark` document stylesheets, the Markdown, RST, and AsciiDoc loaders SHALL select their highlight.js theme from the dark-mode flag (`highlight_js/github-dark.min.css` in dark mode; `highlight_js/github.min.css` for Markdown/RST and `highlight_js/highlight.default.min.css` for AsciiDoc in light mode), and the Markdown loader SHALL additionally select its alert-callout stylesheet (`markdown/github.markdown.alert.dark.css` in dark mode, `markdown/github.markdown.alert.css` otherwise) and its Mermaid theme (`dark` in dark mode, `default` otherwise).

#### Scenario: Markdown document in dark mode

- **WHEN** a Markdown file is loaded and the global dark mode flag is true
- **THEN** the Markdown processor reads the `CSSDark` value from the `[Markdown]` section of `edgeviewer.ini` and uses it to style the rendered HTML

#### Scenario: AsciiDoc document in dark mode

- **WHEN** an AsciiDoc file is loaded and the global dark mode flag is true
- **THEN** the AsciiDoc processor uses its `CSS` value from the `[AsciiDoc]` section because no `CSSDark` key is defined, so AsciiDoc document styling is identical in light and dark mode (code-block syntax highlighting alone follows the dark flag)

#### Scenario: Code blocks follow the dark highlight.js theme in dark mode

- **WHEN** a Markdown, RST, or AsciiDoc file containing a code block is loaded and the global dark mode flag is true
- **THEN** the loader references `highlight_js/github-dark.min.css`, so code blocks render with dark-theme highlighting instead of the light theme's bright background

#### Scenario: HTML document in dark mode

- **WHEN** an HTML or MHT file is loaded and the global dark mode flag is true
- **THEN** the HTML processor injects a style block using the `CSSDark` value from the `[HTML]` section, applied after the document has been loaded so it overrides the document's own inline styles

#### Scenario: directory listing in dark mode

- **WHEN** a directory is loaded and the global dark mode flag is true
- **THEN** the Directory processor uses the `CSSDark` value from the `[Directory]` section to render the file listing with dark-themed colors
