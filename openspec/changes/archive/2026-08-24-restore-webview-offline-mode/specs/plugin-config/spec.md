# plugin-config delta

## MODIFIED Requirements

### Requirement: [WebView] section keys

The `[WebView]` section MUST drive the web-engine runtime and MAY contain the following keys. All keys SHALL be read from the cached parse.

The section is a single flat section shared by both platforms; per-key platform scope is explicit. Windows-only keys preserve their Windows behavior and are silently ignored on Linux (the Linux build never references them; mINI ignores unrequested keys). This mirrors the `[Directory] GenDirThumbs` convention (Windows-only, ignored on Linux).

| Key | Scope |
|-----|-------|
| `UserDir` | Defaulted on both platforms (Globals.cpp); honored by WebView2 on Windows. The Linux `QtWebEngineBackend` uses the default Chromium profile and does not apply `UserDir`. |
| `ShowErrorBoxes` | Windows-only. `1` shows a Windows error message box when WebView2 environment creation fails; `0`/absent suppresses it. |
| `CleanupOnExit` | Windows-only. `1` removes the `<UserDir>/EBWebView` cache directory and any plugin temp files on `DLL_PROCESS_DETACH`; `0`/absent leaves them. (There is no process-detach hook on Linux.) |
| `KeepZoom` | Read on both. `1` persists zoom; per-processor isolation is Windows-only (via `gs_ZoomFactor`), while Linux persists a single per-origin zoom value. `0`/absent resets zoom on each load. |
| `Switches` | Windows-only. Space-separated Chromium command-line flags forwarded verbatim as `ICoreWebView2EnvironmentOptions::AdditionalBrowserArguments`; empty/absent adds none. Ignored on Linux (the Qt Web Engine equivalent is the `QTWEBENGINE_CHROMIUM_FLAGS` environment variable, not an ini key). |
| `BrowserExecutableX86Folder` / `BrowserExecutableX64Folder` | Windows-only, build-specific: the 32-bit DLL reads only `BrowserExecutableX86Folder`, the 64-bit DLL reads only `BrowserExecutableX64Folder`. `%ENV%`-expanded and passed as the browser executable folder to `CreateCoreWebView2EnvironmentWithOptions`, pinning a specific Edge installation; empty/absent auto-detects the installed engine. |
| `OfflineMode` | Read on both. `1` blocks every request not resolving to plugin-local content so documents render offline (see the `offline-mode` capability); `0`/absent allows external fetches. The value is applied when each web view is created from the cached parse. |

#### Scenario: Error boxes suppressed

- **WHEN** WebView2 creation fails and `[WebView] ShowErrorBoxes=0`
- **THEN** no message box is shown; the load silently returns failure

#### Scenario: Cleanup on exit enabled

- **WHEN** `[WebView] CleanupOnExit=1` and the plugin DLL is unloaded
- **THEN** the `<UserDir>/EBWebView` cache directory and any temp files created by the plugin are deleted

#### Scenario: Zoom persisted across files

- **WHEN** `[WebView] KeepZoom=1` and the user zooms a Markdown view to 150%, then opens another Markdown file
- **THEN** the new Markdown view opens at 150% zoom

#### Scenario: Offline mode read on both platforms

- **WHEN** `[WebView] OfflineMode=1` in the ini shared by both platforms
- **THEN** both the 32-bit and 64-bit Windows DLLs and the Linux build block non-local requests while rendering local content normally
