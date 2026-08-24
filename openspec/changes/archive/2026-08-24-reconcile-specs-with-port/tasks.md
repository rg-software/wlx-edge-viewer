## 1. html — remove encoding override and html.example

- [x] 1.1 In `openspec/specs/html/spec.md`, delete the `### Requirement: HTML Rendering With Encoding Override (DetectEncoding=1)` block and its scenarios.
- [x] 1.2 Delete `### Requirement: HTML BOM-Based Charset Detection` and `### Requirement: HTML Meta-Tag Charset Detection` blocks and their scenarios.
- [x] 1.3 Rename `### Requirement: HTML Rendering Without Encoding Override (DetectEncoding=0)` to `### Requirement: HTML Rendering`; rewrite the body to drop the `DetectEncoding` wording and state that engine sniffing is the only charset path (no header rewriting). Update its scenarios accordingly.
- [x] 1.4 In `### Requirement: HTML CSS Injection via a DOMContentLoaded Listener`, remove the `html.example` host from the listener condition and drop the `html.example` scenario; note `ev://local.example` for Linux.
- [x] 1.5 In `### Requirement: HTML Virtual Host Mapping`, remove the `html.example` bullet and its scenario; keep `local.example` + `assets.example`.
- [x] 1.6 Update the spec `## Purpose` to drop the "optional encoding-override mode" clause.

## 2. plugin-config — rename section and drop Chromium-only keys

- [x] 2.1 In `openspec/specs/plugin-config/spec.md`, replace the `### Requirement: [Chromium] section keys` block with the `[WebView]` section (retain `UserDir`, `ShowErrorBoxes`, `CleanupOnExit`, `KeepZoom`; drop `Switches`, `BrowserExecutable*`, `OfflineMode`), including the per-key platform-scope table from the delta. Drop the now-invalid scenarios (additional Chromium switches, fixed Edge binary, each-build browser-folder key).
- [x] 2.2 In `### Requirement: Per-type stylesheet sections`, remove the `[HTML] DetectEncoding` sentence; update the `HTML encoding override enabled` scenario to state the key is ignored.
- [x] 2.3 In `### Requirement: ForcedHtmlExt forced-HTML rendering`, add the Linux note (no temp-copy; `ev://` scheme handler default `text/html`).
- [x] 2.4 In `### Requirement: UserDir fallback default`, rename `[Chromium]` → `[WebView]` and add the Linux note (defaulted but unused by Qt Web Engine).
- [x] 2.5 In `### Requirement: 32-bit and 64-bit config parity`, remove the browser-executable-folder key-name distinction (the keys no longer exist); update the `Browser folder key is build-specific` scenario to state the keys are ignored.
- [x] 2.6 Update the "Ini file is absent" scenario wording (`Chromium keys` → `WebView keys`).

## 3. dark-mode — engine color scheme is Windows-only

- [x] 3.1 In `openspec/specs/dark-mode/spec.md`, rewrite `### Requirement: Web engine color scheme` to scope it to Windows (WebView2 `put_PreferredColorScheme`), add the Linux note (no public Qt Web Engine equivalent), and add the Linux scenario.

## 4. directory-view — GenDirThumbs is Windows-only

- [x] 4.1 In `openspec/specs/directory-view/spec.md`, add a Linux note to `### Requirement: Directory thumbnail for subdirectories` (GenDirThumbs silently ignored; static icons always) and add the `Linux always uses static icons` scenario.

## 5. popup-context-menu — Windows-only

- [x] 5.1 In `openspec/specs/popup-context-menu/spec.md`, scope `### Requirement: Shell context menu invocation from the directory viewer` to Windows and add the Linux no-op note + scenario.

## 6. zoom-control — [WebView] rename and Linux per-origin zoom

- [x] 6.1 In `openspec/specs/zoom-control/spec.md`, rename `[Chromium]` → `[WebView]` in the Purpose, persistence, per-type tracking, and parity requirements.
- [x] 6.2 Add the Linux note (no `ZoomFactorChanged` hook; single shared per-origin zoom, no per-processor isolation) to the persistence and per-type requirements.

## 7. accelerator-keys — key relay is Windows-only

- [x] 7.1 In `openspec/specs/accelerator-keys/spec.md`, scope `### Requirement: WebView key events are relayed to the parent` to Windows and add the Linux note + scenario (Qt Web Engine focus handling; ESC bridge / F-toggle are the Linux equivalents).

## 8. virtual-host-mapping — drop html.example / interceptor / OfflineMode

- [x] 8.1 In `openspec/specs/virtual-host-mapping/spec.md`, rewrite `### Requirement: Mapped folder resources bypass request interception` to remove the `html.example`/`WebResourceRequested`/`OfflineMode` references and state that no request interceptor exists on either platform.

## 9. temp-file-management — [WebView] rename and Windows-only temp/EBWebView

- [x] 9.1 In `openspec/specs/temp-file-management/spec.md`, rename `[Chromium]` → `[WebView]` in the cleanup, EBWebView, and parity requirements.
- [x] 9.2 Add the Linux notes: `ForcedHtmlExt` temp-copy is Windows-only; `RemoveTempFiles()` is never auto-called on Linux (`gs_tempFiles` accumulates); `EBWebView` is WebView2-only.

## 10. wlx-contract — [WebView] ShowErrorBoxes rename

- [x] 10.1 In `openspec/specs/wlx-contract/spec.md`, rename `[Chromium] ShowErrorBoxes` → `[WebView] ShowErrorBoxes` in the `File loading` requirement and its two error-box scenarios.

## 11. other-fallback — remove html.example host

- [x] 11.1 In `openspec/specs/other-fallback/spec.md`, remove the `html.example` host from the `PDF rendering via the browser's built-in PDF viewer` requirement and its `Companion CSS attaches only on matching hosts` scenario (only `local.example` remains).

## 12. linux-runtime — correct the [WebView] config description

- [x] 12.1 In `openspec/specs/linux-runtime/spec.md`, correct the `WebView configuration on Linux` requirement: `UserDir` is not applied on Linux (default Chromium profile); Windows retains `ShowErrorBoxes`/`CleanupOnExit`/`KeepZoom`; only `Switches`/`BrowserExecutable*`/`OfflineMode` are dropped. Rename the `Linux build honors UserDir` scenario to `Linux build uses the default Chromium profile`.

## 13. Verify

- [x] 13.1 `openspec validate reconcile-specs-with-port --strict` passes.
- [x] 13.2 Grep `openspec/specs/` for stale references and confirm none remain: `DetectEncoding`, `html.example`, `OfflineMode`, `BrowserExecutable`, `[Chromium]`. (The Linux-deferred `GenDirThumbs`/`KeepZoom`/accelerator/popup notes are expected to remain as Windows-only annotations.)
- [x] 13.3 Archive the change with `openspec archive reconcile-specs-with-port` once the main specs are reconciled.
