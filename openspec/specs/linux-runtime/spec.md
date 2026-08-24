# linux-runtime Specification

## Purpose
Describes the observable behavior of the EdgeViewer Lister plugin when built and loaded on Linux inside Double Commander: which file types render, which config keys are honored, and which behaviors present on Windows are explicitly deferred. Cross-platform file-type rendering (Markdown, AsciiDoc, RST, HTML, MHT, EML, URL, Images, Other) is governed by `Resources/assets/<type>/loader.html` templates and shared JS/CSS assets that are byte-identical across platforms; this spec captures the Linux-side runtime contract and the deferred behaviors that are visible to users.
## Requirements
### Requirement: Linux build artifact

The plugin SHALL build on Linux (x86_64) as a shared object named `EdgeViewer.wlx64` loadable by Double Commander as a WLX lister. It SHALL link against `Qt6WebEngineWidgets` and `Qt6Widgets` via `find_package(Qt6 6.4 REQUIRED COMPONENTS WebEngineWidgets Widgets)`. It SHALL be shipped into the plugin directory alongside `assets/` and `edgeviewer.ini` directly (no `Resources/` wrapper) — `ProcessorInterface::assetsPath()` is `GetModulePath()/assets`, mirroring the Windows package layout. On Linux the project SHALL NOT depend on vcpkg, WebView2, WIL, or any Microsoft-specific library. The Linux build is independent of the Windows MSBuild project; both builds SHALL pull from the same shared source files.

#### Scenario: Building on Linux
- **WHEN** a developer runs CMake on a Linux system with `qt6-base-dev` and `qt6-webengine-dev` installed (Debian/Ubuntu) — or `qt6-qtbase-devel` and `qt6-qtwebengine-devel` on Fedora/Arch
- **THEN** the build produces `EdgeViewer.wlx64` without requiring vcpkg, MSBuild, or any Windows SDK component

#### Scenario: Loading in Double Commander
- **WHEN** a user installs `EdgeViewer.wlx64` alongside `assets/` and `edgeviewer.ini` directly in the plugin directory and registers it in Double Commander under Lister plugins
- **THEN** Double Commander successfully loads the plugin and dispatches supported file types to it

### Requirement: WLX contract conformance on Linux

The plugin SHALL export the WLX symbols `ListLoadW`, `ListLoadNextW`, `ListCloseWindow`, `ListGetDetectString`, `ListSearchTextW`, `ListPrintW`, `ListSendCommand`, and `ListSetDefaultParams` on Linux using the Double Commander `wlxplugin.h` / `common.h` SDK headers. The exported names, argument types (`HWND = void*`, `WCHAR = uint16_t`, `LPARAM = intptr_t`, `WPARAM = uintptr_t`), and struct layouts (`ListDefaultParamStruct`, `RECT`) SHALL match the Double Commander SDK definitions. The detect-string generation (`ListGetDetectString`) SHALL produce the same `EXT="..."` expression form as on Windows, built from the `[Extensions]` section of `edgeviewer.ini`.

#### Scenario: Detect string built from ini on Linux
- **WHEN** `ListGetDetectString` is called on Linux and `edgeviewer.ini` `[Extensions]` lists `Markdown=md,markdown`, `HTML=html,htm`, etc.
- **THEN** the returned string has the form `EXT="md"|EXT="markdown"|EXT="html"|EXT="htm"|...` identical to the Windows build

#### Scenario: Opening a file on Linux
- **WHEN** Double Commander calls `ListLoadW(parent, "/home/user/readme.md", lcp_darkmode)` on Linux
- **THEN** the plugin creates a web view embedded in `parent` and renders the Markdown file inside it

### Requirement: Cross-platform file-type rendering

File types SHALL render identically on Linux and Windows because the rendering is performed by the embedded web engine running shared JavaScript and CSS from `Resources/assets/<type>/`. Each processor (Markdown, AsciiDoc, RST, HTML, MHT, EML, URL, Images, Other) SHALL load the same `loader.html` template and shared library bundles (`marked.js`, `highlight.js`, `asciidoctor.js`, `mermaid`, `mathjax`, `mhtml2html`, `postal-mime`, `detect-charset`, `thumbnail-viewer`) on both platforms. The processors SHALL select the file type purely from the `[Extensions]` section of `edgeviewer.ini`, identical to the Windows build.

#### Scenario: Markdown renders the same on both platforms
- **WHEN** the same `readme.md` is opened on Linux (Qt Web Engine) and on Windows (WebView2)
- **THEN** both render the document through `Resources/assets/markdown/loader.html` with the same CSS, syntax highlighting, and mermaid/mathjax behavior

#### Scenario: AsciiDoc renders the same on both platforms
- **WHEN** the same `article.adoc` is opened on Linux and on Windows
- **THEN** both render via `Resources/assets/asciidoctor/asciidoctor.js` and apply the same dark/light theme

#### Scenario: EML renders identically on both platforms
- **WHEN** the same `message.eml` is opened on Linux and on Windows
- **THEN** both render via `Resources/assets/eml/loader.html` and the `postal-mime` bundle, honoring the existing `eml` spec requirements (MIME parsing, multipart, encoded-word headers) without any Linux-specific deviations

### Requirement: Virtual host mapping for asset and local resources

The plugin SHALL map the synthetic hostnames `assets.example` and `local.example` to local folders so that loader HTML loaded via `NavigateToString` can reference assets and the user's file root via ordinary absolute URLs. On Windows this is achieved via `ICoreWebView2_3::SetVirtualHostNameToFolderMapping`; on Linux this is achieved via `QWebEngineUrlScheme::registerScheme("ev")` + `QWebEngineProfile::defaultProfile()->installUrlSchemeHandler()` (a custom `ev://` scheme is registered because Chromium reserves `http`/`https` for actual web traffic; the `http://` references in loader HTML are rewritten to `ev://` in `QtWebEngineBackend::NavigateToString` before the HTML reaches `QWebEngineView::setHtml`). The `assets.example` host SHALL map to the plugin's `assets/` directory on both platforms; the `local.example` host SHALL map to the root directory of the file being viewed on both platforms.

#### Scenario: Asset URL resolves on Linux
- **WHEN** a loader HTML on Linux references `ev://assets.example/highlight_js/styles/github.css`
- **THEN** the Qt Web Engine scheme handler returns the file at `assets/highlight_js/styles/github.css`

#### Scenario: Local file URL resolves on Linux
- **WHEN** a loader HTML on Linux references `ev://local.example/path/to/file.png`
- **THEN** the Qt Web Engine scheme handler returns the file on the user's filesystem at that path

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

### Requirement: Directory view on Linux uses static icons

The `DirProcessor` (`EdgeViewer/Processors/DirProcessor.cpp`, `Resources/assets/dirviewer/`) SHALL render directory listings on Linux using the static `folder.png` and `file.png` assets already shipped under `Resources/assets/dirviewer/`. The dynamic shell-thumbnail generation path (`GenDirThumbs=1` with GDI+ and `IShellItemImageFactory`) SHALL remain available on Windows and SHALL be a no-op on Linux in this change. On Linux, `GenDirThumbs` in `edgeviewer.ini` SHALL be ignored if set, and static icons SHALL always be used.

#### Scenario: Linux directory listing shows static icons
- **WHEN** the user opens a directory on Linux
- **THEN** the rendered directory view shows `folder.png` for subdirectories and `file.png` for non-image files, regardless of `GenDirThumbs` setting

#### Scenario: Windows directory listing keeps dynamic thumbnails
- **WHEN** the user opens a directory on Windows with `GenDirThumbs=1`
- **THEN** dynamic shell thumbnails continue to be generated as before

### Requirement: Right-click context menu unavailable on Linux

The native shell right-click context menu inside the Lister (the `EdgeLister::showPopupMenu` path driven by `IShellFolder` / `IContextMenu` on Windows) SHALL NOT be present on Linux in this change. Right-clicking inside the rendered view on Linux SHALL produce no plugin-defined popup; the user's file management actions continue to be available through Double Commander's own panels and menus.

#### Scenario: Right-click on Linux does nothing plugin-specific
- **WHEN** the user right-clicks inside the rendered Lister view on Linux
- **THEN** no plugin-defined context menu appears (no crash, no GIO-based menu in this change)

### Requirement: HTML charset override unavailable on both platforms

The HTML processor (`EdgeViewer/Processors/HtmlProcessor.cpp`) SHALL no longer intercept HTML resource requests to inject a detected charset. The `[HTML] DetectEncoding` ini key SHALL be ignored on both Windows and Linux. When the HTML file does not declare its charset via BOM or `<meta charset>`, the embedded web engine's default sniffing SHALL apply.

> **Known limitation (future-work item #1):** an HTML file with **no BOM, no `<meta charset>` declaration, and a non-UTF-8 encoding** (e.g. Windows-1251, KOI8-R, GBK) will be rendered via the web engine's sniffing fallback, which almost always picks UTF-8 and may mis-render specific characters. The previous `OverrideEncoding` path detected this case and injected a `Content-Type` header to force the encoding. That detection is gone; users encountering this case should re-introduce the override as a dedicated change (see `openspec/notes/future-work.md` item 1 and `proposal.md` §Removed). Do **not** silently re-add the `WebResourceRequested` interceptor in an ad-hoc patch: a re-introduction has to be cross-platform (WebView2 + Qt Web Engine) and requires its own design.

#### Scenario: Windows user with DetectEncoding=1 in ini
- **WHEN** a Windows user has `[HTML] DetectEncoding=1` set and opens an HTML file without charset metadata
- **THEN** the file is rendered using the web engine's default charset sniffing, with no plugin-side override

#### Scenario: Linux user opens an HTML file
- **WHEN** a Linux user opens an HTML file containing `<meta charset="windows-1251">`
- **THEN** Qt Web Engine honors the declared charset and renders the file correctly

#### Scenario: Linux user opens HTML without charset declaration
- **WHEN** a Linux user opens an HTML file with no BOM and no `<meta charset>`
- **THEN** Qt Web Engine sniffs the charset from content; the plugin does not intervene

### Requirement: Sticky per-processor zoom not honored on Linux

The `KeepZoom` ini key and the per-processor zoom-persistence feature (`gs_ZoomFactor` map populated by `ZoomFactorChanged` events) SHALL continue to work on Windows and SHALL NOT be implemented on Linux in this change. On Linux, zoom interaction SHALL be handled by Qt Web Engine's built-in Ctrl+scroll / Ctrl+0 / Ctrl+plus / Ctrl+minus behavior inside the rendered view; per-processor sticky zoom across files is future-work.

#### Scenario: Linux user zooms with Ctrl+wheel
- **WHEN** the user presses Ctrl and scrolls inside the Lister on Linux
- **THEN** Qt Web Engine zooms the page in or out for that view session

#### Scenario: Linux user opens another file
- **WHEN** the user, after zooming in on one Markdown file, navigates to another file via `ListLoadNextW` on Linux
- **THEN** the new view opens at the default zoom (not the previous view's zoom)

### Requirement: Accelerator-key relaying not implemented on Linux

The Windows-specific accelerator-key relaying path (`AddAccleratorKeyHandler` posting `WM_WEBVIEW_KEYDOWN` to the parent window and the JS `KeyQ` / `Digit1..Digit8` message bridge in `WebView2.cpp`) SHALL continue unchanged on Windows and SHALL NOT be implemented on Linux in this change. On Linux, key events inside the rendered view SHALL be handled by Qt Web Engine and Double Commander's own focus management.

#### Scenario: Linux user presses Ctrl+F inside the Lister
- **WHEN** the user presses Ctrl+F inside the rendered view on Linux
- **THEN** the browser's in-page find behavior runs; no plugin-side accelerator handling occurs

