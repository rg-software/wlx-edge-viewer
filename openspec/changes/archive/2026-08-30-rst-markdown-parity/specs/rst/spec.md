## MODIFIED Requirements

### Requirement: RST Rendering via the Loader Template

The RST processor MUST render by filling a loader HTML template (shipped at Resources/assets/rst/loader.html) with placeholder values and navigating the WebView to the filled string. Three placeholders MUST be substituted:

- `__BASE_URL__` MUST be replaced with the URL of the file's parent directory (so relative links/images resolve against the source folder).
- `__CSS_NAME__` MUST be replaced with the CSS filename read from `[RST]` CSS or `[RST]` CSSDark in edgeviewer.ini, depending on the dark-mode flag (see the CSS Theme requirement below).
- `__RST_FILENAME__` MUST be replaced with a URL for the opened file relative to its parent directory.

The loader MAY also inline the pre-fetched file content as a base64 literal (`__FILE_CONTENT__`) so the conversion does not require a second `fetch()`. The actual reStructuredText-to-HTML conversion MUST be performed client-side inside the WebView by the `rst-compiler` library loaded from Resources/assets/rst/rst-compiler.bundle.min.js. The library MUST produce complete HTML for the full set of standard docutils block constructs — including tables, images and figures, source-code blocks, admonitions and other directives, and definition/literal/block-quote constructs — rather than a partial subset. After the conversion, the loader MUST apply the same post-processing passes the Markdown loader applies:

- **Syntax highlighting**: source code blocks that declare a language (via `.. code:: <lang>` or `.. highlight:: <lang>`) MUST be colored for highlight.js (from Resources/assets/highlight_js/) to highlight. Because `rst-compiler` emits each code block as a bare `<pre class="code">` with no language class, the loader MUST parse the RST source's `.. code:: <lang>` / `.. code-block:: <lang>` directives in document order and apply the matching `language-<lang>` class to the corresponding `<pre class="code">` elements so highlight.js picks the grammar. Code blocks without a declared language render as plain preformatted text.
- **Math**: LaTeX math expressed via the `.. math::` directive and the inline `:math:` role is already typeset to KaTeX HTML by `rst-compiler` itself, so no MathJax pass is required. The loader MUST load KaTeX's stylesheet (from Resources/assets/katex/) so the pre-rendered HTML is styled, and MUST strip the KaTeX CDN `<link>` that `rst-compiler` emits into the document `header` so rendering stays fully offline.
- Because a single unsupported directive (`.. sourcecode::`, `.. parsed-literal::`, `.. mermaid::`, `.. doctest::`, `.. toctree::`, or an unknown directive) makes `rst-compiler` throw, the loader MUST wrap the `compile()` call in try/catch and fall back to rendering what it can (e.g. the undecorated preformatted source) so one bad directive never blanks the whole document.

The rendering MUST be identical on Win32 and x64 because the loader template, placeholders, and library are platform-independent static assets.

#### Scenario: Loading an .rst file renders formatted HTML

- **WHEN** the user opens `chapter.rst` from `D:\Notes\`
- **THEN** the RST processor MUST fill the loader template with the `[RST]` CSS value, with `__BASE_URL__` pointing to `D:\Notes\`, and with `__RST_FILENAME__` pointing to `chapter.rst`
- **AND** rst-compiler MUST convert the RST source to complete HTML in the WebView

#### Scenario: RST table renders as a table

- **WHEN** the user opens an `.rst` file containing a reStructuredText table (grid or simple table)
- **THEN** the rendered view MUST show the content as a real HTML table (rows and columns preserved), not as literal text

#### Scenario: RST image and figure render

- **WHEN** the user opens an `.rst` file containing a `.. image::` or `.. figure::` directive
- **THEN** the rendered view MUST show the referenced image, resolved against the file's parent directory via `__BASE_URL__`

#### Scenario: RST code block is syntax-highlighted

- **WHEN** the user opens an `.rst` file containing a `.. code:: python` (or `.. highlight::`) block
- **THEN** the rendered view MUST show the code block with syntax highlighting applied by highlight.js

#### Scenario: RST math is typeset

- **WHEN** the user opens an `.rst` file containing a `.. math::` directive or inline math role
- **THEN** the rendered view MUST show the formula typeset (KaTeX HTML produced by rst-compiler and styled by the loaded katex.min.css)

#### Scenario: Relative links resolve against the source folder

- **WHEN** `chapter.rst` (in `D:\Notes\`) references `images/diagram.png`
- **THEN** the placeholder substitution MUST set `__BASE_URL__` to `D:\Notes\` so the nested link resolves against the file's parent directory

#### Scenario: 32-bit and 64-bit parity for rendering

- **WHEN** the same `.rst` file is opened on the 32-bit and 64-bit builds
- **THEN** both MUST load the same Resources/assets/rst/ assets and produce visually identical HTML output

### Requirement: RST CSS Theme Selection with Dark Mode Support

Unlike the AsciiDoc processor (which reads only a CSS key), the RST processor MUST read both the `[RST]` CSS key and the `[RST]` CSSDark key from edgeviewer.ini. When Total Commander signals dark mode is off, the RST processor MUST apply the stylesheet named by `[RST]` CSS. When Total Commander signals dark mode is on, the RST processor MUST apply the stylesheet named by `[RST]` CSSDark. In the shipped edgeviewer.ini the two keys MUST name distinct shipped theme files — a light theme (e.g. `rst-github.css`) for `CSS` and a dark theme (e.g. `rst-github-dark.css`) for `CSSDark` — so that light and dark mode produce visually different output by default, mirroring the Markdown theme system. The shipped themes MUST style the full set of constructs rst-compiler emits: headings, paragraphs, tables, images/figures, source code blocks, admonitions, definition lists, block quotes, and footnotes. The selection MUST be identical on Win32 and x64.

#### Scenario: Light mode uses the [RST] CSS value

- **WHEN** Total Commander opens a `.rst` file with the Lister dark mode flag set to off
- **THEN** the RST processor MUST apply the stylesheet named by `[RST]` CSS (shipped: a light theme)

#### Scenario: Dark mode uses the [RST] CSSDark value

- **WHEN** Total Commander opens a `.rst` file with the Lister dark mode flag set to on
- **THEN** the RST processor MUST apply the stylesheet named by `[RST]` CSSDark (shipped: a dark theme)

#### Scenario: Shipping default points both keys at the same file

- **WHEN** the shipped edgeviewer.ini is used unchanged
- **THEN** `[RST] CSS` MUST name a light theme file and `[RST] CSSDark` MUST name a distinct dark theme file
- **AND** the rendered output MUST differ between light and dark mode, with the dark theme readable on a dark background

#### Scenario: A user overrides CSSDark to a different file

- **WHEN** a user edits edgeviewer.ini so that `[RST] CSSDark=my-dark.css` and `[RST] CSS=rst-github.css`
- **AND** the Lister opens a `.rst` file in dark mode
- **THEN** the RST processor MUST apply `my-dark.css`, distinct from the light-mode `rst-github.css`

#### Scenario: 32-bit and 64-bit parity for CSS selection

- **WHEN** the plugin is loaded on either the 32-bit or 64-bit build
- **THEN** both builds MUST select between `[RST]` CSS and CSSDark based on the dark-mode flag with identical behavior

### Requirement: RST Virtual Host Mapping

The RST processor MUST register two virtual host-to-filesystem mappings in the WebView before navigating:

- The `assets.example` virtual host MUST map to the plugin's own Resources/assets directory, so the loader template can load `rst-compiler.bundle.min.js`, the CSS file, and the shared rendering sub-assets (highlight_js/ and katex/) via `https://assets.example/...`.
- The `local.example` virtual host MUST map to the root directory of the currently opened RST file, so relative links and images inside the RST source resolve to real files on disk without leaking the user's absolute paths into the document's URL surface.

These mappings MUST be set up identically on Win32 and x64, because the host mapping logic is platform-independent and operates on absolute paths supplied by Total Commander.

#### Scenario: assets.example resolves to plugin assets

- **WHEN** the loader template references `https://assets.example/rst/rst-compiler.bundle.min.js` or a shared sub-asset such as `https://assets.example/highlight_js/highlight.min.js`
- **THEN** the RST processor MUST serve that request from the corresponding file under Resources/assets/
- **AND** the asset MUST load successfully without any network request

#### Scenario: local.example resolves to the opened file's root

- **WHEN** a user opens `D:\Notes\chapter.rst`
- **THEN** the RST processor MUST map `local.example` to `D:\Notes\`
- **AND** any relative image reference inside the RST (e.g. `.. image:: images/diagram.png`) MUST resolve to a file under `D:\Notes\` via the `local.example` host

#### Scenario: 32-bit and 64-bit parity for virtual host mapping

- **WHEN** the plugin is loaded on either the 32-bit or 64-bit build
- **THEN** both builds MUST register `assets.example` → Resources/assets and `local.example` → the opened file's root directory with the same behavior

### Requirement: RST Section Navigation

The RST processor MUST provide in-viewer navigation for same-folder `.rst` links, matching the Markdown loader's navigation behavior. Clicking a relative `.rst` link whose target is in the same directory as the current file SHALL be intercepted and the target rendered in-viewer via rst-compiler, replacing the current page content in the same view. Links to files in other directories SHALL NOT be intercepted and SHALL fall through to default browser behavior.

#### Scenario: Navigation enabled — same-folder link

- **WHEN** the user clicks an `.rst` link whose target is in the same directory
- **THEN** the rendered view intercepts the click, fetches the target file, renders it via rst-compiler, and replaces the current page content in the same view

#### Scenario: Navigation disabled — cross-folder link not intercepted

- **WHEN** the user clicks an `.rst` link whose target is in a different directory (e.g. `../other/file.rst`)
- **THEN** the link is not intercepted and the browser navigates normally (default behavior)
