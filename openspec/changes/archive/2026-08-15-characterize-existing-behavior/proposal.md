## Why

Only 1 of the project's ~17 functional areas has a spec today (`specs/eml/`). An upcoming cross-platform port (`port-to-double-commander-linux`) needs to delta against baselines that define "what current behavior must be preserved" and "what current behavior is being changed or dropped." Without baseline specs, the port's preservation claims are unverifiable and its breaking changes untraceable. Capturing existing behavior as specs *before* the port lets the port's specs become precise deltas, makes review objective, and permanently raises the project's documentation floor.

## What Changes

- **New**: ~18 capability spec files under `openspec/specs/` documenting the *current observed behavior* of the existing Windows-only plugin. No code changes. No behavior changes. Pure characterization.
- Each spec follows the same format as the existing `specs/eml/spec.md` (`## Purpose`, `### Requirement:`, `#### Scenario:` with WHEN/THEN). Content is derived from reading the source code and the static web assets.
- The spec files are written under `openspec/changes/characterize-existing-behavior/specs/<capability>/spec.md` as ADDED requirements; upon archive they are promoted to `openspec/specs/<capability>/spec.md`.
- No `design.md` complexity beyond the capability decomposition rationale (this is a documentation change, not an architectural one).
- No tests are introduced in this change. Unit/characterization tests require the `IWebView` abstraction that the port change introduces; they are added during the port's refactor phase.

## Capabilities

### New Capabilities

Per-file-type capabilities (following the existing `eml` pattern):

- `markdown`: Markdown rendering via `MdProcessor` + `Resources/assets/markdown/` (marked.js, highlight.js, detect-charset, mathjax, mermaid)
- `asciidoc`: AsciiDoc rendering via `AdocProcessor` + `Resources/assets/asciidoctor/` (asciidoctor.js)
- `rst`: ReStructuredText rendering via `RstProcessor` + `Resources/assets/rst/` (restructured.js)
- `html`: HTML rendering via `HtmlProcessor` + `Resources/assets/html/`, including BOM/meta charset detection and the `DetectEncoding` override path
- `mhtml`: MHTML rendering via `MhtProcessor` + `Resources/assets/mhtml/` (mhtml2html)
- `url-files`: `.url` shortcut files via `UrlProcessor` (reads `URL=` line, dispatches `file:///` to HtmlProcessor vs external URL to Navigate)
- `images`: Image rendering via `ImgProcessor` + `Resources/assets/imgview/` (thumbnail-viewer)
- `directory-view`: Directory listing via `DirProcessor` + `Resources/assets/dirviewer/`, including dynamic shell thumbnail generation (`GenDirThumbs`), static icon fallback, sorting (dirs first then alphabetical), and image/other extension filtering
- `other-fallback`: Fallback viewer via `OtherProcessor` for PDF and any configured extension not claimed by another processor

Cross-cutting infrastructure capabilities:

- `wlx-contract`: WLX plugin exports (`ListLoadW`, `ListLoadNextW`, `ListCloseWindow`, `ListGetDetectString`, `ListSearchTextW`, `ListPrintW`, `ListSendCommand`, `ListSetDefaultParams`), detect-string generation from `[Extensions]`, `ShowFlags` handling, 32/64-bit parity
- `virtual-host-mapping`: `assets.example` → plugin's `Resources/assets/` and `local.example` → file's root directory via `ProcessorInterface::mapDomains` / `SetVirtualHostNameToFolderMapping`
- `plugin-config`: `edgeviewer.ini` parsing via mINI, `[Chromium]` section (UserDir, Switches, BrowserExecutableFolder, CleanupOnExit, ShowErrorBoxes, KeepZoom, OfflineMode), `[Extensions]` section and per-type subsections (CSS, CSSDark, etc.), `[HTML]` section (DetectEncoding, CSS, CSSDark), `ForcedHtmlExt`
- `dark-mode`: `gs_IsDarkMode` flag set from `ShowFlags & lcp_darkmode`, selects `CSSDark` ini value over `CSS` per processor
- `zoom-control`: Ctrl+OEM_PLUS/OEM_MINUS/0 zoom with discrete step table, `KeepZoom` ini key persistence per processor type via `gs_ZoomFactor` map
- `text-search`: `ListSearchTextW` → `Navigator::Search` → `window.find()` with case-sensitive/backwards/whole-words/find-first parameters
- `print`: `ListPrintW` → `Navigator::Print` → `window.print()`
- `accelerator-keys`: WebView accelerator-key relaying (`AddAcceleratorKeyHandler`), JS key bridge for `KeyQ` (close) and `Digit1`–`Digit8` (TC quickview tabs), `WM_WEBVIEW_KEYDOWN`/`WM_WEBVIEW_JS_KEYDOWN` posting to parent
- `temp-file-management`: `GetPhysicalPath` — symlink resolution via `GetFinalPathNameByHandle`, UNC path (`\\?\UNC\`) temp-copy via `GenTempFile`, `ForcedHtmlExt` temp-copy with `.html` extension, `RemoveTempFiles` cleanup on `DLL_PROCESS_DETACH` when `CleanupOnExit` is set
- `popup-context-menu`: `EdgeLister::showPopupMenu` — native Windows shell context menu via `IShellFolder`/`IContextMenu`/PIDL, triggered by right-click in `DirProcessor` view, `WM_COPYDATA` with `CMD_MENU`

### Modified Capabilities

- (none) — the existing `eml` spec already describes its behavior fully; no modification needed.

## Impact

- **Code**: none. No source files are created, modified, or deleted.
- **Build**: none. No build system changes. No new dependencies.
- **OpenSpec**: ~18 new spec files under `openspec/changes/characterize-existing-behavior/specs/`. Upon archive, these are promoted to `openspec/specs/<capability>/spec.md`.
- **Dependencies**: none.
- **Systems**: none.
- **Downstream**: the `port-to-double-commander-linux` change (on branch `port-to-double-commander-linux`, paused) will rebase onto master after this change archives, and its proposal/specs will be amended to reference the new baseline specs as delta targets.