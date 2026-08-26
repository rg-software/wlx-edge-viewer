## 0. Fix `__BASE_URL__` placeholder (prerequisite)

The `__BASE_URL__` placeholder in `loader.html` (`<base href=http://local.example/__BASE_URL__/`) is replaced by `DirProcessor` but **not** by `BaseFileProcessor`. This means relative links in markdown, RST, and AsciiDoc loaders resolve to `http://local.example/__BASE_URL__/other.md` — a non-existent path. The virtual host serves raw bytes with no encoding hint, causing both the raw-source-display and wrong-encoding symptoms (issue #45).

- [x] 0.1 In `EdgeViewer/Processors/BaseFileProcessor.cpp`, add `{ L"__BASE_URL__", urlPathW(mPath.relative_path()) }` to the `replacePlaceholders` call in `OpenIn`, matching `DirProcessor`'s existing substitution.

## 1. INI key plumbing (C++)

- [x] 1.1 In `EdgeViewer/Processors/BaseFileProcessor.cpp`, read `sectionIni.get("NavigationUI")` from the `[Markdown]` section alongside the existing `CSS`/`CSSDark` read (line 40). Add a `__MD_NAVIGATION__` placeholder to the `replacePlaceholders` call with value `"true"` or `"false"` based on the ini value.
- [x] 1.2 In `Resources/edgeviewer.ini`, add `NavigationUI=0` to the `[Markdown]` section with a comment explaining the key.

## 2. Loader template (JS/HTML)

- [x] 2.1 In `Resources/assets/markdown/loader.html`, add a `<script>` block before the existing inline script that sets `window.__MD_NAVIGATION__` from the `__MD_NAVIGATION__` placeholder (e.g. `window.__MD_NAVIGATION__ = ("__MD_NAVIGATION__" === "true");`).
- [x] 2.2 In `Resources/assets/markdown/loader.html`, after the initial render completes (inside the `getBytes.then` block, after `hljs.highlightAll()`), call `_mdPushNav(url)` if `window.__MD_NAVIGATION__` is true.

## 3. Navigation JS (markdownExtension.js)

- [x] 3.1 Add the `_mdNav` state object (`{ stack: [], pos: -1 }`) at the top level of `markdownExtension.js`, after the `customRenderer` definition.
- [x] 3.2 Add `_mdNavigate(href)` function: fetch the target `.md` file, decode with `TextDecoder`/`detect_charset`, strip YAML frontmatter, render via `marked.parse`, replace `#content` innerHTML, re-run MathJax/Mermaid/highlight.js, handle hash anchors, call `_mdUpdateButtons`. Wrap fetch in `.catch()` that silently ignores errors.
- [x] 3.3 Add `_mdPushNav(href)` function: truncate forward entries from `_mdNav.stack`, push the new href, update `_mdNav.pos`, call `_mdUpdateButtons`.
- [x] 3.4 Add `_mdGoBack()` and `_mdGoForward()` functions: bounds-check `_mdNav.pos`, decrement/increment, call `_mdNavigate` with the stack entry at the new position.
- [x] 3.5 Add `_mdUpdateButtons()` function: find `#md-nav-bar`, `#md-nav-back`, `#md-nav-fwd`, `#md-nav-info` elements; set button opacity based on stack position; set info text to `pos/total` or empty.
- [x] 3.6 Add `DOMContentLoaded` listener gated on `window.__MD_NAVIGATION__`: create the floating nav bar div (fixed, top-right, z-index 9999) with back/forward buttons and info span; append to `document.body`; attach click listeners to buttons.
- [x] 3.7 Modify the existing `document.addEventListener('click', ...)` handler to intercept `.md` links when `window.__MD_NAVIGATION__` is true: for links matching `^(.+\/)?([^/]+)\.md(#.*)?$`, resolve the href against `document.baseURI`, compare the directory portion (`pathname` minus filename) against the current page's directory — if same folder, call `e.preventDefault()`, `_mdPushNav(href)`, `_mdNavigate(href)`. Cross-folder `.md` links and non-`.md` links fall through to default behavior. Preserve existing `#` anchor handling.

    **Revision (final behavior):** As implemented, same-folder `.md` link interception is NOT gated on `__MD_NAVIGATION__`. The click is always intercepted and rendered in-viewer via `_mdNavigate(url.href)`; only `_mdPushNav(url.href)` (history + bar) is called when the flag is true. This keeps in-viewer rendering working even with `NavigationUI=0`.

## 4. Verify

- [x] 4.1 Build Release for Win32 and x64. Confirm no compiler errors or warnings.
