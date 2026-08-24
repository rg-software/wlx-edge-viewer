## Why

The port-to-double-commander-linux change shipped a cross-platform refactor that removed or deferred several Windows-only behaviors. The port's `proposal.md` listed 15 "Modified Capabilities," but those modifications were never materialized as delta spec files in the change — only `linux-runtime` was added. As a result, the main specs under `openspec/specs/` still describe the pre-port state: they document `[HTML] DetectEncoding=1`, `[Chromium]` section keys, GDI+ shell thumbnails, per-processor zoom persistence, and other features that were either removed on both platforms or deferred to Linux-only future-work. The `linux-parity` living checklist tracks the current state per feature, but the canonical specs are out of sync.

This change brings every affected main spec into agreement with the shipped code and the `Readme.md` future-work table, using the `linux-parity` checklist as the ground-truth reference. No source-tree changes — this is a spec-only reconciliation.

## What Changes

- **REMOVED** (both platforms): `[HTML] DetectEncoding` requirement, BOM/meta charset detection, the `html.example` virtual host, and the `[Chromium]` section keys `Switches`, `BrowserExecutableX86Folder`/`BrowserExecutableX64Folder`, and `OfflineMode`.
- **MODIFIED** (both platforms): `[HTML]` renders via `local.example` only; the `[Chromium]` section renamed to `[WebView]`, retaining `UserDir`, `ShowErrorBoxes`, `CleanupOnExit`, and `KeepZoom` (dropping `Switches`, `BrowserExecutable*`, `OfflineMode`); the `ForcedHtmlExt` temp-copy path is Windows-only (Linux uses the scheme handler's default `text/html` MIME).
- **MODIFIED** (Linux-deferred): dynamic directory thumbnails, shell right-click context menu, per-processor sticky zoom, accelerator-key relaying, and engine-level `PreferredColorScheme` — all marked as Windows-only with Linux future-work notes.
- **MODIFIED** (dark-mode): `PreferredColorScheme` is Windows-only; Linux already has the `QGuiApplication` fallback and HTML CSS injection from the `fix-linux-dark-mode-fallback` and `fix-linux-html-css-injection` changes.

## Capabilities

### Modified Capabilities

- `html`: Remove `DetectEncoding=1`, BOM detection, meta-tag detection, and `html.example` virtual host. Rename `DetectEncoding=0` to "HTML Rendering (cross-platform)".
- `plugin-config`: Rename `[Chromium]` section to `[WebView]`; retain `UserDir`, `ShowErrorBoxes`, `CleanupOnExit`, `KeepZoom`; remove `Switches`, `BrowserExecutable*`, `OfflineMode`, `DetectEncoding`; update `ForcedHtmlExt` to note the Linux scheme-handler fallback.
- `directory-view`: `GenDirThumbs=1` is Windows-only; Linux uses static icons (`folder.png`/`file.png`).
- `popup-context-menu`: Entire capability is Windows-only; Linux has no equivalent.
- `zoom-control`: Per-processor `KeepZoom` persistence is Windows-only; Linux has per-origin zoom memory but no per-processor isolation.
- `accelerator-keys`: HWND-level key relaying is Windows-only; Linux defers to Qt's focus management.
- `dark-mode`: Engine-level `PreferredColorScheme` is Windows-only; Linux already has the `QGuiApplication` fallback + HTML CSS injection (synced from the fix changes).
- `virtual-host-mapping`: Remove `html.example` (was only used by the removed `DetectEncoding=1` path).
- `temp-file-management`: Remove `EBWebView` cache cleanup (was tied to `[Chromium] CleanupOnExit`); `ForcedHtmlExt` temp-copy is Windows-only.
- `wlx-contract`: `[Chromium] ShowErrorBoxes` → `[WebView] ShowErrorBoxes` (section rename).
- `other-fallback`: `html.example` host removed from the PDF companion-CSS listener (only `local.example` remains).
- `linux-runtime`: Correct the `[WebView]` config description — `UserDir` is not applied on Linux (default Chromium profile), and the Windows build retains `ShowErrorBoxes`/`CleanupOnExit`/`KeepZoom`.

## Impact

- **Code**: none.
- **Build**: none.
- **Dependencies**: none.
- **Systems**: no runtime change. The `linux-parity` checklist is the working reference for current behavior; this change makes the canonical specs agree with it.