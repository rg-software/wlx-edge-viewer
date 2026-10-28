## MODIFIED Requirements

### Requirement: AsciiDoc CSS Theme Selection (No Dark Mode Variant)

Unlike the Markdown, RST, EML, images, and directory renderers — all of which read both `[<Type>] CSS` and `[<Type>] CSSDark` keys and switch between then-notion of dark mode — the AsciiDoc processor MUST read ONLY the `[AsciiDoc] CSS` key from edgeviewer.ini (shipped as `asciidoctor.css`). The AsciiDoc processor MUST NOT read any `[AsciiDoc] CSSDark` value, and such a value MUST have no effect even if a user adds one to edgeviewer.ini. As a consequence, the AsciiDoc document stylesheet MUST be the same regardless of whether Total Commander has informed the plugin that dark mode is active. This is an intentional behavioral difference from the other document renderers and is documented here for characterization, not as a defect. Code-block syntax highlighting is exempt from this rule: like Markdown and RST, the AsciiDoc loader selects its highlight.js theme from the dark-mode flag (`github-dark.min.css` in dark mode, `highlight.default.min.css` otherwise), so code blocks stay readable on dark backgrounds.

#### Scenario: AsciiDoc uses the same CSS in light and dark mode

- **WHEN** Total Commander opens a `.adoc` file with the Lister dark mode flag set to off
- **AND WHEN** Total Commander opens the same file with the Lister dark mode flag set to on
- **THEN** both renders MUST use the stylesheet named by `[AsciiDoc] CSS` (shipped: `asciidoctor.css`)
- **AND** the document styling MUST be identical, because no `[AsciiDoc] CSSDark` value is consulted (code-block syntax highlighting alone MAY differ per the scenario below)

#### Scenario: Dark mode uses the dark highlight.js theme for AsciiDoc code blocks

- **WHEN** Total Commander opens an `.adoc` file containing a code block with the Lister dark mode flag set to on
- **THEN** the loader references `highlight_js/github-dark.min.css` via `__HLJS_CSS__`, so code blocks render on the dark background instead of the light theme's bright background

#### Scenario: A manually-added CSSDark key is ignored for AsciiDoc

- **WHEN** a user edits edgeviewer.ini to add `[AsciiDoc] CSSDark=my-dark.css`
- **AND** the Lister opens a `.adoc` file in dark mode
- **THEN** the AsciiDoc processor MUST still apply only `asciidoctor.css` (the `[AsciiDoc] CSS` value) and MUST NOT apply `my-dark.css`

#### Scenario: Contrast with other renderers that DO support dark mode

- **WHEN** a Markdown, RST, EML, image, or directory file is opened
- **THEN** those processors MUST consult their `[<Type>] CSSDark` key in dark mode
- **AND** the AsciiDoc processor is the documented exception because it has no CSSDark key wired up
