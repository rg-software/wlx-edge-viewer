# plugin-config Specification

## Purpose
Defines the `edgeviewer.ini` configuration file parsed by the plugin (via the mINI library, in `EdgeViewer/Globals.cpp`) — its location next to the plugin DLL, its one-time lazy load, and the observable meaning of every section and key that drives extension detection, the web-engine runtime, per-type stylesheet selection, the directory viewer, and the forced-HTML extension override — with identical parsing on 32-bit and 64-bit builds.
## Requirements
### Requirement: Config file location and format

The plugin MUST read its configuration from a file named `edgeviewer.ini` located in the same directory as the loaded plugin DLL, using the mINI INI parser. The file MUST use standard INI syntax (section headers in `[brackets]`, `Key=Value` lines, `;` line comments) and is consumed as plain ASCII/UTF-8 (values are converted to UTF-16 for internal use). The shipped template lives at `Resources/edgeviewer.ini` and is deployed next to the DLL by the release packaging. The plugin MUST NOT write to this file at runtime.

#### Scenario: Ini located next to the DLL

- **WHEN** the plugin DLL is installed at `C:\TC\Plugins\EdgeViewer\EdgeViewer.wlx`
- **THEN** the plugin reads its configuration from `C:\TC\Plugins\EdgeViewer\edgeviewer.ini`

#### Scenario: Ini file is absent

- **WHEN** the plugin directory contains no `edgeviewer.ini`
- **THEN** the parsed configuration is empty (type sections, extensions, and WebView keys are all absent), and downstream behavior degrades to the code-level defaults for each missing key

### Requirement: Config lazy loading and caching

The plugin MUST parse `edgeviewer.ini` exactly once, on the first access to the configuration, and MUST cache the parsed result in a process-lifetime static structure so that subsequent accesses read the cached parse without re-reading the file. The cache SHALL be populated no later than the first call to any lister entry point that needs configuration (detect-string generation, file loading, or DLL detach), and the file SHALL NOT be re-read even if it changes on disk while the plugin is loaded.

#### Scenario: First access parses the file

- **WHEN** Total Commander calls `ListGetDetectString` (the first configuration-touching entry point) before any other access
- **THEN** `edgeviewer.ini` is parsed once and the extensions table is available to build the detect string

#### Scenario: Edits after load are not observed

- **WHEN** `edgeviewer.ini` is edited while the plugin is loaded in Total Commander
- **THEN** the running plugin keeps using the cached parse until the plugin is reloaded

### Requirement: [Extensions] section

The `[Extensions]` section MUST map each type name to a comma-separated, uppercase list of file extensions that the corresponding processor claims. The shipped section defines `HTML=HTM,HTML,XHTML,XML`, `Markdown=MD,MARKDOWN`, `AsciiDoc=ADOC,ASCIIDOC`, `URL=URL`, `MHTML=MHT,MHTML`, `EML=EML`, `RST=RST`, `Images=PNG,GIF,BMP,JPG,JPEG,ICO,WEBP,SVG`, and `Other=PDF`. The section MUST additionally support the `Dirs` flag (`Dirs=1` makes the plugin claim directory paths via an empty-extension detect term; `Dirs=0` or absent disables directory handling) and the `ForcedHtmlExt` key (a `|`-separated, case-insensitive regex of extensions that are force-rendered as HTML, shipped as `xml|xhtml`). Type matching against these lists SHALL be case-insensitive and SHALL produce identical results on 32-bit and 64-bit builds.

#### Scenario: Adding a new extension to a type

- **WHEN** the user edits `[Extensions] Markdown=MD,MARKDOWN,MDOWN` and reloads the plugin
- **THEN** `.mdown` files are claimed by the Markdown processor

#### Scenario: Disabling directory handling

- **WHEN** the user sets `Dirs=0` (or removes the `Dirs` key) and reloads the plugin
- **THEN** directory paths are no longer claimed by the plugin (the detect string gains no trailing `EXT=""` term)

#### Scenario: Case-insensitive extension match

- **WHEN** the user opens a file named `IMAGE.PNG` (uppercase) and `Images=PNG,...`
- **THEN** the Images processor claims the file, because extension matching is case-insensitive

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

### Requirement: Per-type stylesheet sections

The plugin MUST read a per-type stylesheet section for each rendered type — `[HTML]`, `[Markdown]`, `[AsciiDoc]`, `[RST]`, `[EML]`, `[Images]`, and `[Directory]` — each keyed by `CSS` for the light-mode stylesheet and `CSSDark` for the dark-mode stylesheet (selected when the `lcp_darkmode` flag is active). The shipped `edgeviewer.ini` defines `CSS`/`CSSDark` pairs for `[HTML]`, `[Markdown]`, `[RST]`, `[EML]`, `[Images]`, and `[Directory]`; `[AsciiDoc]` ships with `CSS` only (no `CSSDark` key, so dark-mode AsciiDoc has no dark-specific stylesheet configured in the ini). Stylesheet files are loaded from each type's `Resources/assets/<type>/` folder via the `assets.example` virtual host. The `[Images]` section additionally defines `FitToScreen` (`1` fits the image to the viewport, `0` shows it at native size; toggle at runtime with F). The `[HTML] DetectEncoding` key was removed and MUST be silently ignored. Stylesheet selection SHALL be identical on 32-bit and 64-bit builds.

#### Scenario: Light vs dark stylesheet for Markdown

- **WHEN** `[Markdown] CSS=github.css` and `[Markdown] CSSDark=github.dark.css`
- **THEN** light-mode Markdown views use `github.css` and dark-mode Markdown views (lcp_darkmode set) use `github.dark.css`

#### Scenario: AsciiDoc has no dark stylesheet key

- **WHEN** `lcp_darkmode` is set and the user opens an AsciiDoc file
- **THEN** no `CSSDark` value is read for AsciiDoc (the shipped section omits the key), so no dark-specific stylesheet is applied from the ini

#### Scenario: HTML encoding override enabled

- **WHEN** `[HTML] DetectEncoding=1` is set and the user opens an HTML file
- **THEN** the key is ignored (the encoding-override mechanism was removed on both platforms); the WebView's built-in charset sniffing is the only path

### Requirement: [Directory] section keys

The `[Directory]` section MUST drive the directory/thumbnail viewer (`EdgeViewer/Processors/DirProcessor.cpp`, `Resources/assets/dirviewer/`) and MAY contain: `DirImageExt` (a `|`-separated regex of extensions for which real thumbnails are generated, shipped as `jpg|jpeg|png|gif|svg|bmp|webp`); `DirOtherExt` (a `|`-separated regex of extensions shown as placeholder thumbnails while all others are ignored; shipped as `mp4|avi|txt`, with `.*` meaning show all files); `CSS`/`CSSDark` (directory viewer stylesheets from `Resources/assets/dirviewer/`); `ShowNames` (`1` shows file/folder names, `0` hides them; toggle with Ctrl+H); `ShowFolders` (`1` shows folders in the listing, `0` hides them; toggle with Ctrl+G); `FitToScreen` (`1` fits thumbnails to the viewport, `0` shows them at native size; toggle with F); `TruncateNames` (`1` truncates long names to fit the thumbnail cell); `NamesUnderThumbnails` (`1` renders names beneath thumbnails rather than over them); `GenDirThumbs` (`1` generates real shell thumbnails, `0` uses static icon fallback); and `DirThumbSize` (the thumbnail cell size in pixels, shipped as `256`). All keys SHALL parse identically on 32-bit and 64-bit builds.

#### Scenario: Only configured extensions get thumbnails

- **WHEN** a directory contains `.jpg`, `.mp4`, and `.zip` files, with `DirImageExt=jpg|jpeg|png|gif|svg|bmp|webp` and `DirOtherExt=mp4|avi|txt`
- **THEN** `.jpg` gets a generated thumbnail, `.mp4` gets a placeholder thumbnail, and `.zip` is ignored

#### Scenario: Showing all files

- **WHEN** `[Directory] DirOtherExt=.*`
- **THEN** every file in the directory that does not match `DirImageExt` gets a placeholder thumbnail (nothing is ignored)

#### Scenario: Hiding names

- **WHEN** `[Directory] ShowNames=0` (or the user presses Ctrl+H at runtime)
- **THEN** file and folder names are not rendered in the directory view

#### Scenario: Custom thumbnail size

- **WHEN** `[Directory] DirThumbSize=128`
- **THEN** directory thumbnails are generated at 128-pixel cells

### Requirement: ForcedHtmlExt forced-HTML rendering

On Windows, files whose extension matches the `ForcedHtmlExt` regex in `[Extensions]` (shipped as `xml|xhtml`, matched case-insensitively against the file's full path) MUST be copied to a temporary location with a `.html` suffix, and the temporary `.html` path SHALL be what the plugin renders, so that Edge/WebView2 treats the content as an HTML document rather than applying its native XML tree rendering. The temporary file MUST be tracked for later cleanup. On Linux, the temp-copy path is not implemented; the `ev://` scheme handler's default `Content-Type: text/html` achieves the same user-visible result for HTML-sniffable content. Non-matching files SHALL be rendered from their original path.

#### Scenario: XHTML file forced to HTML

- **WHEN** the user opens `page.xhtml` and `[Extensions] ForcedHtmlExt=xml|xhtml`
- **THEN** the file is copied to a temp file named `<random>.html` and the HTML processor renders that temp path as HTML

#### Scenario: Non-listed extension is not forced

- **WHEN** the user opens `data.xml` but `ForcedHtmlExt` is empty or removed
- **THEN** the file is rendered from its original path (no `.html` temp copy is made)

#### Scenario: Temp file cleaned up on exit

- **WHEN** a forced-HTML temp copy exists and the plugin unloads with `[WebView] CleanupOnExit=1`
- **THEN** the temp copy is deleted during detach

### Requirement: UserDir fallback default

When the `[WebView]` section omits the `UserDir` key entirely, the plugin MUST default `UserDir` to the plugin DLL's own directory (so the WebView2 profile lives next to the plugin). When `UserDir` is present, the plugin MUST use the supplied value after expanding `%ENV%` placeholders (so the shipped `%USERPROFILE%` resolves to the user's profile directory). The fallback SHALL be applied at first parse and stored in the cache, and SHALL resolve the same value on 32-bit and 64-bit builds.

#### Scenario: UserDir key absent

- **WHEN** `edgeviewer.ini`'s `[WebView]` section has no `UserDir` key
- **THEN** the plugin DLL's directory is used as the WebView2 user-data directory

#### Scenario: UserDir key present with environment variable

- **WHEN** `[WebView] UserDir=%USERPROFILE%\EdgeViewer`
- **THEN** the WebView2 user-data directory resolves to `<expanded USERPROFILE>\EdgeViewer`

### Requirement: 32-bit and 64-bit config parity

The `edgeviewer.ini` format, section/key names, value syntax (comma-separated extension lists, `|`-separated regexes, `%ENV%` expansion), parsing rules, and observable effects on plugin behavior SHALL be identical for the 32-bit and 64-bit plugin DLLs. The two builds read the same `edgeviewer.ini` layout from their respective plugin directories.

#### Scenario: Same ini shared by both builds

- **WHEN** the same `edgeviewer.ini` file is placed next to both the 32-bit and 64-bit plugin DLLs
- **THEN** both builds parse it into the same sections and values and exhibit the same observable behavior

#### Scenario: Browser folder key is build-specific

- **WHEN** `edgeviewer.ini` sets `BrowserExecutableX64Folder` but not `BrowserExecutableX86Folder`
- **THEN** the 64-bit DLL pins the named folder as the browser executable folder while the 32-bit DLL auto-detects the engine (each build reads only its own key)

#### Scenario: Engine flags forwarded

- **WHEN** `[WebView] Switches=--disable-smooth-scrolling --disable-gpu`
- **THEN** the 32-bit and 64-bit WebView2 environments are created with both flags as additional browser arguments

