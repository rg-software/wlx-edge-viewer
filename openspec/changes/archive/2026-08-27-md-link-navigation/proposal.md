## Why

When viewing a Markdown documentation set, clicking a relative `.md` link currently navigates the web view away from the plugin context, opening the raw source with broken encoding (issue #45). Users must manually reopen each linked file from Total Commander, which breaks the reading flow. The root cause is that link clicks bypass the markdown processor entirely.

## What Changes

- **Fix `__BASE_URL__` placeholder** (prerequisite): `BaseFileProcessor::OpenIn` now substitutes `__BASE_URL__` with the file's directory path, matching `DirProcessor`'s existing behavior. This fixes relative link resolution in markdown, RST, and AsciiDoc loaders (issue #45).
- **Same-folder `.md` link interception**: Clicking a `.md` link whose target is in the same directory as the current file fetches the target, renders it via marked.js, and replaces the current page content — all without leaving the lister. Links to files in other directories (`../sub/page.md`) are not intercepted and fall through to default behavior.
- **Back/forward navigation stack**: A navigation history tracks visited pages. Back/forward controls let the user retrace their reading path.
- **Floating navigation bar**: A minimal `← →` button bar with a page counter (`2/5`) is injected into the top-right corner of rendered Markdown pages.
- **INI switch**: The entire feature is gated by `[Markdown] Navigation=1` in `edgeviewer.ini`. Default is off (no UI injected, `.md` links are not intercepted).

## Capabilities

### Modified Capabilities
- `openspec/specs/markdown/spec.md`: Add requirement for ini-gated in-viewer `.md` link navigation with back/forward stack.

## Impact

- `EdgeViewer/Processors/BaseFileProcessor.cpp` — fix missing `__BASE_URL__` substitution (prerequisite, affects markdown/RST/AsciiDoc); read `Navigation` key from the `[Markdown]` ini section and substitute it into the loader template
- `Resources/assets/markdown/markdownExtension.js` — add navigation functions and `DOMContentLoaded` bar injection
- `Resources/assets/markdown/loader.html` — add `__MD_NAVIGATION__` placeholder for the ini-driven flag
