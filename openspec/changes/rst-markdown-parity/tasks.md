## 1. Produce the vendored rst-compiler browser bundle

- [x] 1.1 In a scratch directory under `C:\Users\Maxim\AppData\Local\Temp\opencode\`, scaffold a temporary npm project and `npm install rst-compiler` (pin the exact version used, e.g. `rst-compiler@0.5.9`).
- [x] 1.2 Build a single-file IIFE bundle exposing the compiler (e.g. `esbuild --bundle --format=iife --global-name=RstC --external:fs --external:path <entry>`), targeting browser globals, with `RstC.RstToHtmlCompiler` available.
- [x] 1.3 Verify the bundle runs outside Node by loading it in a headless context and calling `new RstC.RstToHtmlCompiler().compile("Title\n=====\n\nHello")` to confirm it returns `{ header, body }` (this also confirms the DOM-free compile path works for the loader).
- [x] 1.4 Commit the minified bundle as `Resources/assets/rst/rst-compiler.bundle.min.js` and record the `rst-compiler` version and build command in a short comment header or a README note next to the asset (so it can be regenerated).

## 2. Rewrite the RST loader template

- [x] 2.1 Rewrite `Resources/assets/rst/loader.html` to keep the base64 pre-fetched content path (`__FILE_CONTENT__` → `atob` → `TextDecoder` + `detect_charset`), script-tag-load `rst-compiler.bundle.min.js`, and call `new RstC.RstToHtmlCompiler().compile(decodedText)` writing `body` into `#content`.
- [x] 2.2 Add the rendering-library tags the loader needs: `highlight_js/highlight.min.js` + `highlight_js/github.min.css` (syntax highlighting), and `katex/katex.min.css` (styling rst-compiler's pre-rendered KaTeX math). MathJax and Mermaid are NOT used for RST: rst-compiler typesets math itself and has no Mermaid generator.
- [x] 2.3 Add code-block post-processing so `.. code:: <lang>` / `.. highlight:: <lang>` blocks carry a `language-<lang>` class for highlight.js (inspect actual rst-compiler output in Task 3 and adapt the selector/rewrite to the real markup).
- [x] 2.4 Confirm the `__CSS_NAME__`, `__BASE_URL__`, and `__RST_FILENAME__` placeholders are still substituted by `BaseFileProcessor` exactly as before (no C++ change); verify the loader references the two new theme files via `__CSS_NAME__`.

## 3. Inspect rst-compiler output and adapt the loader

- [x] 3.1 Using Node against the built bundle, compile representative `.rst` samples (tables, `.. image::`/`.. figure::`, `.. code:: python`, `.. math::`, admonitions, definition lists, `:ref:`/`:doc:`, `:math:` inline role) and print the emitted `body`/`header` to record the actual markup shapes.
- [x] 3.2 Resolve design OQ1: `.. math::` and `:math:` are already typeset to KaTeX HTML by rst-compiler, so MathJax is NOT needed; the loader must vendor `katex.min.css` and strip the KaTeX CDN `<link>` from `header` for offline math.
- [x] 3.2a Vendor `katex.min.css` (v0.16.21, matching rst-compiler's emitted header link) and its woff2 font files into `Resources/assets/katex/`, with the CSS's font-url paths adjusted to that folder, so math renders fully offline.
- [x] 3.3 Adjust the loader's code-block and math post-processing to the recorded markup, and add any needed CSS (code, admonition, table, figure, math) to the RST themes.

## 4. Add Markdown-parity RST themes and wire the ini

- [x] 4.1 Create the shipped light theme `Resources/assets/rst/rst-github.css` styling the full rst-compiler output (headings, paragraphs, tables, images/figures, code blocks, admonitions, definition lists, block quotes, footnotes).
- [x] 4.2 Create the shipped dark theme `Resources/assets/rst/rst-github-dark.css` with a dark-readable palette, and remove/replace the now-superseded single `rst-style.css` if it is no longer referenced.
- [x] 4.3 Update the shipped `edgeviewer.ini` so `[RST] CSS=rst-github.css` and `[RST] CSSDark=rst-github-dark.css`.

## 5. Same-folder RST link navigation

- [x] 5.1 Add same-folder `.rst` link interception in the RST loader (mirroring the Markdown loader's navigation pattern): click on a same-folder `.rst` link fetches the target, decodes with the same charset detection, compiles via rst-compiler, and replaces `#content` in-viewer; cross-folder links fall through to default browser behavior.

## 6. Remove superseded assets

- [x] 6.1 Delete `Resources/assets/rst/restructured.bundle.min.js` and `Resources/assets/rst/render_rst.js` after confirming the new loader no longer references them.
- [x] 6.2 Grep the repo for any remaining reference to `restructured.bundle`, `render_rst`, or `RstProcessor`-adjacent stale asset paths and confirm none exist.

## 7. Verify

- [x] 7.1 Build Release for both Win32 and x64 (via the MSVS Developer Command Prompt / `BuildMakeSetup.bat` flow) and confirm the RST assets are packaged unchanged next to the DLLs.
- [x] 7.2 Manually load a representative `.rst` sample in Total Commander (Win32 and x64) covering tables, an image/figure, a `.. code::` block (verify highlight.js colors), `.. math::` (verify KaTeX typeset offline), admonitions, and a multi-section document (verify rendering and same-folder link navigation). Confirm light vs dark mode use the respective theme. (Verified in TC; the extended `Examples/ReStructuredText.rst` + `RstNavTarget.rst` cover all of these.)
- [x] 7.3 Confirm no regressions in asset packaging (all `Resources/assets/` files that existed are still shipped; only the RST set changed).
- [ ] 7.4 Linux Double Commander: load the same `.rst` sample over the `ev://` scheme to confirm the bundle serves over the virtual host. **Deferred by user** (will check the Linux build later) — asset-only change, low risk; the bundle is scheme-agnostic (it runs in a classic `<script>` over the virtual host, and the loader's `assets.example` references are identical on both platforms).


