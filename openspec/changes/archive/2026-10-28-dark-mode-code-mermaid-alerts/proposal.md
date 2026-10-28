## Why

In dark mode (`lcp_darkmode` set) the Markdown view renders three elements
with hard light-theme styling, making them unreadable or glaring against the
dark body (`github.dark.css`, `#0d1117`):

1. **Code blocks are bright white** — `markdown/loader.html` hardcodes the
   light `highlight_js/github.min.css` (`.hljs{background:#fff}`), so every
   highlighted block paints white over the dark page.
2. **Mermaid diagrams are near-unreadable** — `mermaid.initialize()` always
   uses the light `default` theme (light labels/edges tuned for a white
   page).
3. **Alert callouts are light** — `github.markdown.alert.css` hardcodes
   `background:#f6f8fa`, so NOTE/TIP/... boxes render near-white with
   washed-out body text.

The same hardcoded-light highlight.js theme ships in the RST loader
(`github.min.css`) and the AsciiDoc loader (`highlight.default.min.css`),
so code blocks glare there too. (Confirmed visually against
`Examples/tutorial #1.md`.)

## What Changes

- Vendor the official highlight.js GitHub Dark theme as
  `Resources/assets/highlight_js/github-dark.min.css` (same 11.9.0 family
  as the shipped light themes; no new dependency).
- Add `Resources/assets/markdown/github.markdown.alert.dark.css`: dark-mode
  counterpart of the alert stylesheet (dark tinted backgrounds, GitHub
  dark-mode accent hues), mirroring the existing `CSS`/`CSSDark` pattern.
- `BaseFileProcessor::OpenIn` substitutes two new placeholders keyed off
  the existing `gs_IsDarkMode` flag: `__HLJS_CSS__` (`github-dark.min.css`
  in dark mode; `github.min.css`, or `highlight.default.min.css` for
  AsciiDoc, in light mode) and `__IS_DARK__` (`true`/`false`).
- `MdProcessor::extraPlaceholders` substitutes `__ALERT_CSS__` (light vs
  dark alert stylesheet), following the `ImgProcessor` precedent.
- `markdown/loader.html` uses `__HLJS_CSS__` / `__ALERT_CSS__` in its
  `<link>` tags, exposes `window.__IS_DARK__`, and initializes Mermaid with
  `theme: 'dark'` when the flag is set (initial render and `_mdNavigate`).
- `rst/loader.html` and `asciidoctor/loader.html` use `__HLJS_CSS__` for
  their highlight.js theme link. No other loader changes.

## Capabilities

### Modified Capabilities

- `markdown`: dark-mode code blocks render with the GitHub Dark
  highlight.js theme; Mermaid diagrams use the Mermaid `dark` theme;
  alert callouts use the dark alert stylesheet. Light-mode output is
  byte-identical to before.
- `rst`: dark-mode code blocks render with the GitHub Dark highlight.js
  theme. Light-mode output is unchanged.
- `asciidoc`: dark-mode code blocks render with the GitHub Dark
  highlight.js theme; the document stylesheet (`asciidoctor.css`) is still
  identical in both modes. This narrows the previous "visually identical
  in light and dark mode" characterization, which is updated.
- `dark-mode`: the "AsciiDoc styling never changes in dark mode" rule is
  narrowed to the document stylesheet; syntax highlighting is now
  dark-aware for Markdown, RST, and AsciiDoc.

## Impact

- **New assets**: `highlight_js/github-dark.min.css`,
  `markdown/github.markdown.alert.dark.css` — shipped automatically by the
  existing whole-directory deployment (`robocopy Resources\` on Windows,
  `install(DIRECTORY .../assets ...)` on Linux); no packaging change.
- **Modified code**: `BaseFileProcessor.cpp` (two placeholder pairs),
  `MdProcessor.h` (`extraPlaceholders` override).
- **Modified assets**: `markdown/loader.html`,
  `markdown/markdownExtension.js`, `rst/loader.html`,
  `asciidoctor/loader.html` (placeholder links + Mermaid theme flag).
- **Specs**: `markdown`, `rst`, `asciidoc`, `dark-mode` delta specs; main
  specs synced on completion.
- **No new dependencies** (vendored CSS only), no `edgeviewer.ini` keys,
  no Linux-floor impact (no new Qt/WebEngine API use).
