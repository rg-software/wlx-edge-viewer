## Why

The RST viewer is a "basic" port: it parses RST to an AST with the `restructured` library and renders it with a small hand-written `render_rst.js` that covers only a handful of node types. Tables, images/figures, the full directive/admonition set, working code-block highlighting, math, Mermaid, and section navigation either render incompletely or are silently dropped. Markdown already has all of these. This change brings RST up to functional parity with Markdown by switching the renderer to a mature, maintained RST→HTML compiler.

## What Changes

- Swap the RST renderer from the `restructured` AST parser + hand-written `render_rst.js` to **`rst-compiler`** (npm, MIT), which converts RST directly to HTML with full docutils directive and role support. The old `restructured.bundle.min.js` and `render_rst.js` are removed, replaced by a vendored browser bundle `rst-compiler.bundle.min.js` committed under `Resources/assets/rst/`.
- Rewrite `Resources/assets/rst/loader.html` to: parse the base64-inlined pre-fetched content with `restructured`'s replacement (via `RstToHtmlCompiler().compile(text)`), write the resulting `body` into `#content`, and run the same post-processing passes the Markdown loader uses — highlight.js code highlighting, MathJax typesetting, and Mermaid diagram rendering.
- Wire up code-block language classes so highlight.js actually highlights RST `.. code:: <lang>` / `.. highlight:: <lang>` blocks (the current renderer emitted a bare `<pre>` with no language class).
- Add **section navigation** parity: RST headings (which rst-compiler emits with section IDs) get the same in-viewer navigation the Markdown loader provides.
- Add **CSS theme parity**: ship light and dark RST theme stylesheets driven by the existing `[RST] CSS` / `[RST] CSSDark` ini keys, so RST theming matches Markdown's theme system instead of a single hard-coded `rst-style.css`.
- Reuse the existing `highlight_js/`, `mathjax/`, and `mermaid/` assets already present for Markdown — no new rendering stacks are vendored (rst-compiler's native Shiki/KaTeX integration is deliberately not adopted).

This is a static-assets-only change: no C++ source changes and no `vcpkg.json` changes. **BREAKING** (asset-level): the `restructured.bundle.min.js` and `render_rst.js` files are removed and replaced; any external reference to them (none exist) would break.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `rst`: The RST rendering requirement changes from "convert via the `restructured` AST library + partial hand renderer" to "convert via `rst-compiler` to complete HTML" and gains the same rendering/theme/navigation parity Markdown has. The RST virtual-host requirement is updated to serve `rst-compiler.bundle.min.js` and the code/math sub-assets (highlight_js/, katex/); Mermaid is dropped (rst-compiler has no generator). CSS theme selection keeps its existing `[RST] CSS`/`CSSDark` wiring (already spec'd) but now ships real light/dark themes.

## Impact

- **Affected assets**: `Resources/assets/rst/loader.html`, `Resources/assets/rst/rst-style.css`, removed `Resources/assets/rst/render_rst.js` and `Resources/assets/rst/restructured.bundle.min.js`, added `Resources/assets/rst/rst-compiler.bundle.min.js` plus new light/dark theme CSS. Shared `Resources/assets/highlight_js/` is reused (untouched); `katex/` is added under `Resources/assets/` (math, vendored). `mathjax/` and `mermaid/` are not used by the RST path.
- **Affected spec**: `openspec/specs/rst/spec.md` (delta).
- **No C++ changes**: `EdgeViewer/Processors/RstProcessor.{h,cpp}` and `BaseFileProcessor` are unchanged — the `[RST]` CSS/CSSDark and `__CSS_NAME__` placeholder machinery already exists.
- **No dependency changes**: `vcpkg.json` untouched. The JS bundle is vendored like the other renderer libraries; it is produced offline by building `rst-compiler` (TypeScript) and committing the resulting single-file browser bundle.
- **Both platforms**: behavior is driven entirely by static assets so Win32/x64 and Linux render identically; verified by build + manual load (no test suite).
