## MODIFIED Requirements

### Requirement: WebView configuration on Linux

`edgeviewer.ini` SHALL use a `[WebView]` section (replacing the Windows-only `[Chromium]` section) on both platforms. The section is a single flat section with per-key platform scope (see the `plugin-config` capability). On Linux, the `QtWebEngineBackend` uses Qt Web Engine's default Chromium profile, so `UserDir` is not applied; the Windows-only keys (`ShowErrorBoxes`, `CleanupOnExit`) and the dropped keys (`Switches`, `BrowserExecutableX86Folder`, `BrowserExecutableX64Folder`, `OfflineMode`) are ignored if present. The Windows build SHALL read `UserDir`, `ShowErrorBoxes`, `CleanupOnExit`, and `KeepZoom` from `[WebView]` unchanged.

#### Scenario: Existing user upgrades from Windows to shared [WebView] section

- **WHEN** an existing user has `[Chromium] UserDir=...` and runs the updated build
- **THEN** the plugin starts its own profile directory under the default location and no longer reads the legacy `[Chromium]` section

#### Scenario: Linux build uses the default Chromium profile

- **WHEN** `edgeviewer.ini` has `[WebView] UserDir=~/.cache/edgeviewer` on Linux
- **THEN** the `QtWebEngineBackend` uses Qt Web Engine's default profile (the `UserDir` value is not applied on Linux)

#### Scenario: Linux build ignores Chromium-specific keys

- **WHEN** `edgeviewer.ini` has `[WebView] Switches=--disable-gpu` on Linux
- **THEN** the plugin does not pass any switch to Qt Web Engine and rendering works normally
