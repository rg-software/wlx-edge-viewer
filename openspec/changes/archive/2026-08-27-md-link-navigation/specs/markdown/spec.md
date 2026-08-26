## ADDED Requirements

### Requirement: INI-gated same-folder markdown link navigation

The Markdown processor SHALL read the `NavigationUI` key from the `[Markdown]` section of `edgeviewer.ini`. When the value is `1`, the loader SHALL enable in-viewer navigation UI for same-folder `.md` links only. When the value is `0` or absent (the default), no navigation UI is injected and no navigation history is tracked. In both modes, a same-folder `.md` link click SHALL be intercepted and the target rendered in-viewer via marked.js; only the floating bar and back/forward history are gated on the flag. This setting SHALL be identical on 32-bit and 64-bit builds.

A same-folder link is one whose resolved URL has the same directory path as the current page (i.e. the `pathname` portion of the URL differs only by the filename, not by any directory segment). Links to files in other directories (e.g. `../sub/page.md`, `/other/page.md`) SHALL NOT be intercepted and SHALL fall through to default browser behavior.

#### Scenario: Navigation enabled — same-folder link

- **WHEN** the user opens a Markdown file and `[Markdown] NavigationUI=1` is set, and clicks a `.md` link whose target is in the same directory
- **THEN** the rendered view intercepts the click, fetches the target file, renders it via marked.js, and replaces the current page content in the same view, and the target is pushed onto the navigation history

#### Scenario: Navigation enabled — cross-folder link not intercepted

- **WHEN** the user clicks a `.md` link whose target is in a different directory (e.g. `../other/file.md`)
- **THEN** the link is not intercepted and the browser navigates normally (default behavior)

#### Scenario: Navigation disabled (default) — same-folder link still renders

- **WHEN** the user opens a Markdown file and `[Markdown] NavigationUI` is not set or set to `0`, and clicks a `.md` link whose target is in the same directory
- **THEN** the target file is still fetched and rendered in-viewer via marked.js (the link click is intercepted), but no navigation bar appears and no history is tracked

### Requirement: Back/forward navigation stack

When navigation is enabled, the Markdown loader SHALL maintain a per-page navigation stack. Clicking a `.md` link pushes the target onto the stack. Back and forward operations pop/push the stack and re-render the corresponding page. The stack SHALL truncate any forward history when a new link is clicked (standard browser navigation semantics).

#### Scenario: Click a .md link

- **WHEN** the user clicks a relative `.md` link on a rendered Markdown page with navigation enabled
- **THEN** the target file is fetched, rendered, and displayed in place; the navigation stack grows by one entry

#### Scenario: Back navigation

- **WHEN** the user presses the back button after navigating to a linked `.md` page
- **THEN** the previously viewed page is re-rendered and the stack position moves back by one

#### Scenario: Forward navigation after back

- **WHEN** the user presses back and then forward
- **THEN** the page that was forward on the stack is re-rendered

#### Scenario: New link truncates forward history

- **WHEN** the user navigates back and then clicks a new `.md` link
- **THEN** any forward entries beyond the current stack position are discarded and the new page is pushed

### Requirement: Floating navigation bar

When navigation is enabled, the Markdown loader SHALL inject a fixed-position navigation bar into the top-right corner of the rendered page containing a back button (`←`), a forward button (`→`), and a page counter showing `position/total` (e.g. `2/5`). Buttons that are not actionable (back at the start, forward at the end) SHALL be visually dimmed. The bar SHALL overlay the document content without displacing it.

#### Scenario: Navigation bar visible

- **WHEN** the user views a Markdown page with navigation enabled
- **THEN** a floating bar with back/forward buttons and a page counter is visible in the top-right corner

#### Scenario: Navigation bar hidden when disabled

- **WHEN** the user views a Markdown page with navigation disabled
- **THEN** no floating bar is present and no DOM elements for navigation are injected

### Requirement: Linked .md file rendering

When a linked `.md` file is navigated to in-viewer, the target file SHALL be fetched, decoded using the same charset detection as the initial page load, rendered via marked.js, and post-processed with the same MathJax, Mermaid, and highlight.js passes as the initial load. YAML frontmatter in the target file SHALL be stripped before rendering.

#### Scenario: Linked file with frontmatter

- **WHEN** the user clicks a `.md` link whose target contains YAML frontmatter (`---` delimited block at the top)
- **THEN** the frontmatter is stripped and the remaining content is rendered normally

#### Scenario: Linked file with MathJax content

- **WHEN** the user navigates to a linked `.md` file containing LaTeX formulas
- **THEN** the formulas are typeset by MathJax after the page content is replaced

#### Scenario: Non-existent linked file

- **WHEN** the user clicks a `.md` link whose target file does not exist or fails to load
- **THEN** the current page content remains unchanged and no error overlay is shown
