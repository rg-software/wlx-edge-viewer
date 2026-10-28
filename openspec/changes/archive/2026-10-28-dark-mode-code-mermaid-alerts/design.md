# Design: dark-mode code / Mermaid / alert rendering

## Context

Dark-mode selection lives host-side: Total Commander passes `lcp_darkmode`,
the plugin records `gs_IsDarkMode`, and `BaseFileProcessor::OpenIn` picks
the `[<Type>] CSS`/`CSSDark` stylesheet into the `__CSS_NAME__` placeholder.
But three rendering inputs bypass that flag entirely:

- the highlight.js **theme CSS** (hardcoded light file per loader),
- the Mermaid **theme** (hardcoded `default`),
- the Markdown **alert stylesheet** (hardcoded light file).

The engine-level `PreferredColorScheme` (WebView2) does not fix these: it
only affects engine defaults, not author CSS.

## Decisions

### D1: One dark highlight.js theme, selected host-side

Vendor the official `github-dark.min.css` (highlight.js 11.9.0, same family
as the shipped `github.min.css`) and select it in C++ via a new
`__HLJS_CSS__` placeholder computed from `gs_IsDarkMode`:

- dark: `github-dark.min.css` (all three loaders),
- light: `github.min.css` (Markdown, RST), `highlight.default.min.css`
  (AsciiDoc — keeps its current light appearance pixel-identical).

Alternatives rejected: overriding `.hljs` colors per-theme with inline
`<style>` (would need to replicate the full token palette per theme to
stay readable); `prefers-color-scheme` media queries (WebView2's
`PreferredColorScheme` would match on Windows, but on Linux there is no
engine-level propagation, so the media query would follow the OS theme
instead of the host/TC flag — the `__IS_DARK__` host flag is authoritative
on both platforms).

### D2: Alert dark stylesheet as a separate file, selected host-side

`github.markdown.alert.dark.css` mirrors the light file's structure with
GitHub dark-mode values (tinted `rgba()` backgrounds per alert type,
`#4493f8`/`#3fb950`/`#ab7df8`/`#d29922`/`#f85149` accents, `#c9d1d9` body
text matching `github.dark.css`). Selected via `__ALERT_CSS__` from
`MdProcessor::extraPlaceholders` (Markdown-only concern; base class stays
generic, following the `ImgProcessor` precedent). A separate file — rather
than a `body.dark` override block — keeps the "one `<link>` per theme"
pattern and avoids a flash of light styling.

### D3: Mermaid theme from `window.__IS_DARK__`

The loader sets `window.__IS_DARK__ = __IS_DARK__` next to the existing
`__MD_NAVIGATION__` flag assignment, and both Mermaid init sites (initial
render in `loader.html`, `_mdNavigate` in `markdownExtension.js`) pass
`theme: window.__IS_DARK__ ? 'dark' : 'default'`. Mermaid's `dark` theme is
built in (no new asset). RST needs no Mermaid handling (rst-compiler has
no Mermaid generator).

### D4: AsciiDoc "identical styling" carve-out

The AsciiDoc spec asserted renders MUST look visually identical in both
modes. That held only because the light highlight theme painted code
blocks white-on-dark — the defect itself. The carve-out: the AsciiDoc
**document stylesheet** (`asciidoctor.css`, the `[AsciiDoc] CSS` key)
remains mode-independent and no `CSSDark` key is introduced; only code
**syntax highlighting** follows the dark flag, exactly like Markdown/RST.

## Risks

- **Mermaid `dark` theme contrast** on the dark body: accepted after visual
  check against `Examples/tutorial #1.md` (reporter-confirmed).
- **No Linux test suite**: the placeholders flow through the shared
  `BaseFileProcessor`, and Linux rewrites the same `http://assets.example`
  URLs to `ev://`, so the new files resolve on both platforms; Windows
  tests + manual DC check cover it.
