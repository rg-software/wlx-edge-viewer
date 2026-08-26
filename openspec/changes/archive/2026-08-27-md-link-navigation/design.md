## Context

Markdown rendering currently happens in a single pass: `BaseFileProcessor::OpenIn` fills `loader.html` with the file content (base64-inlined), CSS name, and file path, then `NavigateToString` hands it to the web view. The loader's JS decodes the base64, runs `marked.parse`, and applies MathJax/Mermaid/highlight.js. Clicking a `.md` link inside the rendered page has no special handling — it either does nothing or navigates the web view away from the plugin context.

There is also a prerequisite bug: the `__BASE_URL__` placeholder in `loader.html` (`<base href=http://local.example/__BASE_URL__/`) is not replaced by `BaseFileProcessor::OpenIn`. Only `DirProcessor` substitutes it. This means relative links in markdown, RST, and AsciiDoc resolve to a non-existent path, causing raw source display with wrong encoding (issue #45).

The navigation feature adds a second rendering pass inside the same web view, reusing the existing `marked.js` + post-processing stack. It also adds a floating UI bar. Both are gated by an ini key so they ship off by default.

## Goals / Non-Goals

**Goals:**
- Click a same-folder `.md` link → fetch target, render in-place, maintain navigation history
- Back/forward via floating bar buttons
- Gate the entire feature behind `[Markdown] NavigationUI=1` (default off)
- Preserve existing post-processing (MathJax, Mermaid, highlight.js) and YAML frontmatter stripping for navigated pages

**Non-Goals:**
- Cross-folder `.md` link navigation (`../sub/page.md`, absolute paths) — these fall through to default browser behavior. Resolving paths across directory boundaries and re-mapping the virtual host root is out of scope; the system is not designed to be a full browser.
- Navigation across non-markdown links (external URLs, anchors only, images)
- Keyboard shortcuts for back/forward (the web view captures most key events; adding JS-level shortcuts risks conflicts with marked.js/mermaid)
- Persisting navigation state across lister close/reopen
- Navigation bar styling via CSS theme (the bar uses a fixed minimal style; theme-driven styling is a separate concern)

## Decisions

### D1: INI key placement — `[Markdown] NavigationUI=1`

**Chosen**: Read from the existing `[Markdown]` section alongside `CSS`/`CSSDark`.

**Alternatives considered**:
- Global `[EdgeViewer] MarkdownNavigation=1` — rejected because markdown-specific behavior belongs in the markdown section.
- Per-file opt-out via YAML frontmatter — rejected because the INI key is simpler and the feature is a user preference, not a per-document concern.

**C++ change**: `BaseFileProcessor::OpenIn` reads `sectionIni.get("Navigation")` and passes `"true"`/`"false"` to a new `__MD_NAVIGATION__` placeholder in the loader template. This follows the same pattern as `CSS`/`CSSDark`.

### D2: Loader template gating — JS-side guard

**Chosen**: The loader's inline `<script>` sets `window.__MD_NAVIGATION__` from the `__MD_NAVIGATION__` placeholder (replaced C++-side). `markdownExtension.js` checks this flag at `DOMContentLoaded` to decide whether to create the nav bar and register the `.md` link click interceptor.

**Alternatives considered**:
- Two separate loader.html files (one with nav, one without) — rejected because it doubles the template maintenance surface.
- C++-side conditional injection of the nav bar HTML — rejected because it mixes C++ string manipulation with HTML generation; the JS approach keeps all UI logic in one place.

### D3: Navigation mechanism — fetch + DOM replace, same-folder only

**Chosen**: `_mdNavigate(href)` fetches the target `.md` file via `fetch()`, decodes it with the same `TextDecoder`/`detect_charset` path as the initial load, runs `marked.parse`, replaces `#content` innerHTML, and re-runs MathJax/Mermaid/highlight.js. YAML frontmatter is stripped with the same regex. The click interceptor only matches links whose resolved URL has the same directory as the current page — cross-folder links are left unhandled.

**Same-folder detection**: Compare `new URL(href, document.baseURI).pathname` directory against the current page's `location.pathname` directory. If they match, intercept; otherwise, let the browser handle it naturally.

**Alternatives considered**:
- Hidden iframe per page — rejected because iframes add cross-origin complexity and make the nav bar integration harder.
- Service worker caching — rejected because it adds complexity with no real benefit; the virtual host serves files from disk which is already fast.
- Full cross-folder navigation — rejected because it requires re-mapping `local.example` root per target directory, which is genuine browser territory and outside the system's design scope.

### D4: Navigation stack — simple array with position pointer

**Chosen**: `_mdNav = { stack: [], pos: -1 }`. Push truncates forward entries. Back decrements pos, forward increments. No max-depth limit (documentation sets are typically shallow).

### D5: Nav bar injection point — `DOMContentLoaded`

**Chosen**: The nav bar is created in a `DOMContentLoaded` listener in `markdownExtension.js`. It appends a fixed-position `<div>` with two `<button>` elements and a `<span>` counter. The bar has `z-index: 9999` to overlay content.

**Rationale**: `DOMContentLoaded` fires after the loader's inline script has set `window.__MD_NAVIGATION__`, so the flag is available. The bar is minimal (no external CSS dependency) and uses inline styles to avoid interfering with the markdown theme.

## Risks / Trade-offs

- **[Risk] Linked `.md` file fetch fails silently** → The `_mdNavigate` function wraps the fetch in a `.catch()` that leaves the current page unchanged. No error overlay is shown per spec.
- **[Risk] Large navigation stacks consume memory** → Mitigated by the fact that documentation sets are typically shallow (< 20 pages). The stack holds URL strings and rendered HTML is replaced (not accumulated). No mitigation needed for now.
- **[Risk] `ev://` scheme on Linux** → The `fetch()` calls in `_mdNavigate` resolve against `http://local.example/...` which maps to the file's root dir via virtual host mapping. Qt Web Engine on Linux handles this identically to WebView2 on Windows. No platform-specific code needed.
- **[Trade-off] No keyboard shortcuts** → Back/forward is mouse-only via the floating bar. Keyboard shortcuts would require JS `keydown` listeners that could conflict with marked.js/mermaid/highlight.js or TC's own accelerator keys. The floating bar is sufficient for the initial implementation.
