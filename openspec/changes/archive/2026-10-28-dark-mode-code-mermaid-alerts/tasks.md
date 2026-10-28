## 1. Assets

- [x] 1.1 Vendor `Resources/assets/highlight_js/github-dark.min.css` (official highlight.js 11.9.0 GitHub Dark theme, same family as the shipped light themes)
- [x] 1.2 Add `Resources/assets/markdown/github.markdown.alert.dark.css` (dark counterpart of the alert stylesheet: tinted backgrounds, dark-mode accents, `#c9d1d9` body text)

## 2. Host placeholder wiring (C++)

- [x] 2.1 `BaseFileProcessor::OpenIn`: compute `__HLJS_CSS__` (`github-dark.min.css` in dark mode; `github.min.css`, or `highlight.default.min.css` for AsciiDoc, in light mode) and `__IS_DARK__` (`true`/`false`) from `gs_IsDarkMode`
- [x] 2.2 `MdProcessor.h`: `extraPlaceholders()` override supplying `__ALERT_CSS__` (dark vs light alert stylesheet)

## 3. Loader templates

- [x] 3.1 `markdown/loader.html`: `__HLJS_CSS__` / `__ALERT_CSS__` link targets, `window.__IS_DARK__` flag, Mermaid `dark` theme on the initial-render init
- [x] 3.2 `markdown/markdownExtension.js`: Mermaid `dark` theme in `_mdNavigate` (same-folder navigation re-render)
- [x] 3.3 `rst/loader.html` and `asciidoctor/loader.html`: `__HLJS_CSS__` link targets

## 4. Verification

- [x] 4.1 Simulate placeholder substitution for light + dark across all three loaders: no leftover tokens; `node --check` on the substituted inline scripts and `markdownExtension.js`
- [x] 4.2 Build `EdgeViewer.Tests` Release x64 and run the suite (all green: 68 cases / 280 assertions)
- [x] 4.3 Manual check in Total Commander dark mode against `Examples/tutorial #1.md` (code blocks, Mermaid, alerts all dark-readable; reporter-confirmed)
- [x] 4.4 Sync delta specs to main specs (`openspec sync`), validate the change, and verify implementation matches (`openspec verify` equivalent)
- [x] 4.5 Archive the change and commit
