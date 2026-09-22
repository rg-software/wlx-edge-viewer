## ADDED Requirements

### Requirement: CHM rendering parity

When the user opens a `.chm` file (matched by `[Extensions] CHM=CHM`), the
plugin SHALL render the archive's default topic with the table-of-contents
sidebar, resolving relative resources inside the archive, on both platforms.

- Windows ref: `EdgeViewer/Processors/ChmProcessor.cpp`, `Resources/assets/chm/loader.html`, the archive-backed branch of the `evh://` handler in `EdgeViewer/WebView/WebView2Backend.cpp`.
- Linux ref: the archive-backed branch of the `EvSchemeHandler` in `EdgeViewer/WebView/QtWebEngineBackend.cpp`; the loader fetches the `.hhc` entry by XHR (Chromium's `fetch()` allowlist rejects the `ev://` scheme — see `openspec/notes/rendering-pipeline.md`).
- Status: `planned`.
- Test: `manual-dc` (open a representative CHM on Windows in TC and on Linux in DC; confirm the home topic renders, the sidebar lists the TOC, a TOC entry navigates, and a relative image loads). A headless `auto-shared` test MAY cover the pure archive-metadata and `.hhc` helpers.

#### Scenario: CHM opens with sidebar on both platforms

WHEN the user opens a `.chm` file on Windows (TC) or on Linux (DC),
THEN the lister shows the archive's default topic with a table-of-contents
sidebar, and activating a sidebar entry navigates to that topic.

#### Scenario: Relative resources resolve on both platforms

WHEN a rendered CHM topic references an image or stylesheet stored in the archive,
THEN the resource loads from the archive on both platforms rather than showing a
missing-resource placeholder.
