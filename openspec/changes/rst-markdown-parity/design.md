## Context

The RST loader currently ships two assets that drive rendering: `restructured.bundle.min.js` (the `restructured` AST parser) and a project-authored `render_rst.js` that hand-converts a subset of that AST to HTML. That hand renderer omits tables, images/figures, most directives/admonitions, and emits code blocks as bare `<pre>` with no language class, so highlight.js never matches. The processor plumbing (`.h`/`.cpp`), the loader template placeholder substitution, and the `[RST] CSS`/`CSSDark` ini wiring all already work through `BaseFileProcessor` and need no C++ changes. See proposal.md for the motivation; this design covers only how to replace the renderer.

`rst-compiler` (npm, MIT) converts RST directly to HTML (`new RstToHtmlCompiler().compile(text)` → `{ header, body }`) with full docutils directive/role support. It is authored as ESM TypeScript; the repo's loaders are plain `<script>`-based, so it must be bundled to a single browser script before it can be used. Its native syntax highlighting uses Shiki and its math uses KaTeX. We **do not** use Shiki's output (it would add ~8MB of language grammars) — it is mapped at bundle time to a local shim so the module inits cleanly, and the loader reuses the repo's existing highlight.js via a language-mapping pass (D4). KaTeX IS used: rst-compiler pre-renders math, so `katex` is bundled in and `katex.min.css` + fonts are vendored (D5).

## Goals / Non-Goals

**Goals:**
- Replace the partial hand renderer with `rst-compiler` producing complete RST→HTML.
- Make RST rendering truly on par with Markdown: real syntax highlighting, math, Mermaid, tables, images/figures, full directives, and same-folder navigation.
- Keep the change confined to `Resources/assets/rst/` (plus the shared highlight_js/mathjax/mermaid reuse); no C++ and no new vcpkg dependency.

**Non-Goals:**
- Adopting Shiki/KaTeX. We reuse the existing highlight.js and MathJax stack for consistency with the Markdown renderer.
- Implementing every Sphinx/Python-only directive or role (rst-compiler itself documents that third-party/Sphinx Python extensions are out of scope). `:ref:`, `:doc:`, `:download:` and standard roles are in scope.
- Changing RST file detection, the `[RST]` CSS/CSSDark ini keys, or the virtual-host mapping — those already work and are merely re-verified.
- Converting RST to Markdown (rst-compiler also supports this; not needed here).

## Decisions

### D1: Bundle `rst-compiler` to a browser IIFE and commit it under `Resources/assets/rst/`
`rst-compiler` is an ESM/TypeScript package; the loaders use plain `<script src>` tags. We produce a single-file IIFE bundle exposing `RstC.RstToHtmlCompiler` and commit it as `Resources/assets/rst/rst-compiler.bundle.min.js`, mirroring how `restructured.bundle.min.js` and `marked.umd.js` are already vendored. The entry is a thin re-export (`export { RstToHtmlCompiler } from "rst-compiler"`) so esbuild's `--global-name=RstC` assigns that named export to the global — a side-effect-only entry (assigning `globalThis.RstC` inline) is tree-shaken into an empty `RstC={}` and must not be used. Build command:
```
esbuild entry.js --bundle --format=iife --global-name=RstC --minify \
  --platform=browser --alias:shiki=./shiki-shim.js --outfile=rst-compiler.bundle.min.js
```
The full command (with the committed `entry.js`/`shiki-shim.js` sources) is recorded in the bundle's header comment.

- *Alternative considered*: using `<script type="module">` + an import map / CDN. Rejected: the project vendors everything locally for offline use (see `offline-mode` spec) and never depends on runtime network fetch for libraries; a local module file would require restructuring the loader away from the established plain-script pattern.
- Build happens once offline (in the developer environment) and the output is committed; there is no build-time fetch in MSBuild/CMake. This matches the user's chosen approach.

### D2: Use the single-document `compile()` API, not the two-stage cross-document pipeline
RST in a lister is one file at a time. `compile(text)` returns `{ header, body }`; we write `body` into `#content` and drop `header` (which carries KaTeX/scripts we don't use). The two-stage `parse()`/`generate()` pipeline exists for multi-document `:doc:`/`download:` resolution and is not needed for a single-file viewer.

### D3: Keep the pre-fetched, base64-inlined content path
The loader already decodes `__FILE_CONTENT__` (base64) → bytes → `TextDecoder(detect_charset(...))` → string, exactly like Markdown. We pass that decoded string to `compile()`. The `fetch("http://local.example/...")` branch remains only as the no-prefetch fallback, matching the other loaders.

### D4: Reuse highlight.js — post-process `rst-compiler` code-block output to carry language classes
`rst-compiler` emits `.. code:: <lang>` / `.. highlight:: <lang>` blocks as plain `<pre>`/`<code>` (Shiki is a separate opt-in and we don't pass it). To make highlight.js work without Shiki, the loader post-processes the compiled `body`: it locates source-code blocks and their declared language (captured from the compiler's output / parsed from the source) and rewrites them to `<pre><code class="language-<lang>">`, then calls `hljs.highlightAll()`. The precise shape of the emitted markup is an implementation detail to confirm during Task 3 and adapt to; the spec's guarantee (highlighting actually applies) is what matters.
### D5: Math is KaTeX (rst-compiler built-in); Mermaid is dropped

Prolbing confirmed rst-compiler typesets `.. math::` and `:math:` to KaTeX HTML itself (and emits a KaTeX CDN `<link>` in `header`). MathJax is therefore NOT reused for RST — instead `katex.min.css` is vendored into `Resources/assets/katex/` (fonts included, CDN `<link>` stripped) so math is styled offline. Mermaid is NOT supported: rst-compiler has no Mermaid generator (`.. mermaid::` throws "Missing generator") and there is no standard RST Mermaid construct, so the Mermaid requirement is dropped from the spec. (This supersedes the original D5/OQ1 intent to reuse the Markdown MathJax+Mermaid stack.)

### D6: Same-folder `.rst` navigation, consistent with Markdown
Reuse the Markdown loader's navigation pattern (`_mdPushNav` equivalent, renamed for RST) so a click on a same-folder `.rst` link re-renders the target in-viewer via `compile()`; cross-folder links fall through. This is gated consistently with Markdown (navigation always intercepts same-folder links; the floating bar/history is the Markdown `NavigationUI` flag's concern — for RST we implement the interception so links work; the spec we wrote only requires interception of same-folder links).

### D7: Ship light and dark theme stylesheets
Replace the single `rst-style.css` with two shipped themes (`rst-github.css` for `[RST] CSS`, `rst-github-dark.css` for `[RST] CSSDark`), reading the existing ini keys, so the shipped defaults match Markdown's light/dark behavior. Update the shipped `edgeviewer.ini` `[RST]` section to point `CSS` at the light theme and `CSSDark` at the dark theme. (The `__CSS_NAME__` placeholder and CSS/CSSDark selection already work in `BaseFileProcessor`; only the files and their ini names change.)

### D8: No C++ and no `vcpkg.json` change
All behavior lives in static assets and the ini file. `RstProcessor.{h,cpp}` and `BaseFileProcessor` are untouched. Comply with the AGENTS.md guidance and the design rule to state where changes belong: **static assets only**.

## Risks / Trade-offs

- **Bundle size / dependency graph**: `rst-compiler` depends on Shiki and KaTeX (and their transitive deps) even though we don't use them for output; a bundled build may be large (hundreds of KB to low MB). → Mitigation: bundle is local/committed and loaded from the virtual host (cached); size is acceptable for a document viewer. If it proves problematic, evaluate `esbuild --tree-shaking` or the lighter rst2html approach in the future — but still commit a bundle.
- **Unknown emitted markup for code/math**: `rst-compiler`'s exact `<pre>`/math output shape isn't pinned in the README. → Mitigation: Task 3 is dedicated to empirically inspecting the compiled output (Node console) for representative `.rst` inputs and adapting the post-processing; the Open Question OQ1 captures the one genuinely deferrable unknown.
- **No XSS sanitization / ReDoS**: rst-compiler explicitly warns it does not sanitize output and uses lookahead/lookbehind regexes. This is a local file viewer on trusted local documents — same trust model as Markdown's marked.js. → Mitigation: no change; documents are user's own local files. Do not feed it untrusted remote content.
- **Behavior divergence from Python docutils**: rst-compiler can deviate from the reference implementation, and it only allows space indentation. → Mitigation: acceptable for a lister; standard reST is well covered, and the spec explicitly accommodates rst-compiler as the renderer.
- **Removing `restructured.bundle.min.js`/`render_rst.js`**: clean removal, no in-repo references remain. → Mitigation: Task 4 deletes them and Task 5 verifies nothing references the old assets.

## Migration Plan

- In-place migration of `Resources/assets/rst/`: add the new bundle + themes, rewrite `loader.html`, update `edgeviewer.ini` `[RST]` CSS/CSSDark pointers, then delete `restructured.bundle.min.js` and `render_rst.js` once the new loader is confirmed rendering.
- Rollback: the old `restructured.bundle.min.js` and `render_rst.js` are retained in git history; reverting the change + the `edgeviewer.ini` `[RST]` keys restores prior behavior. No data migration.

## Open Questions

- **OQ1 — Math rendering in rst-compiler output (RESOLVED)**: rst-compiler's math directive emits KaTeX HTML directly, so no MathJax re-render is needed. The loader vendors `katex.min.css` (with fonts) and strips the emitted KaTeX CDN `<link>` from `header` to stay offline. Confirmed empirically in Task 3.
