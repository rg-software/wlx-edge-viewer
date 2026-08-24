## MODIFIED Requirements

### Requirement: Config file location and format

The plugin MUST read its configuration from a file named `edgeviewer.ini` located in the same directory as the loaded plugin DLL, using the mINI INI parser. The file MUST use standard INI syntax (section headers in `[brackets]`, `Key=Value` lines, `;` line comments) and is consumed as plain ASCII/UTF-8 (values are converted to UTF-16 for internal use). The shipped template lives at `Resources/edgeviewer.ini` and is deployed next to the DLL by the release packaging. The plugin MUST NOT write to this file at runtime.

#### Scenario: Ini located next to the DLL

- **WHEN** the plugin DLL is installed at `C:\TC\Plugins\EdgeViewer\EdgeViewer.wlx`
- **THEN** the plugin reads its configuration from `C:\TC\Plugins\EdgeViewer\edgeviewer.ini`

#### Scenario: Ini file is absent

- **WHEN** the plugin directory contains no `edgeviewer.ini`
- **THEN** the parsed configuration is empty (type sections, extensions, and WebView keys are all absent), and downstream behavior degrades to the code-level defaults for each missing key

### Requirement: [WebView] section keys

The `[WebView]` section MUST drive the web-engine runtime and MAY contain the following keys. The `Switches`, `BrowserExecutableX86Folder`, `BrowserExecutableX64Folder`, and `OfflineMode` keys were removed with the `[Chromium]` → `[WebView]` rename and MUST be silently ignored. All keys SHALL be read from the cached parse.

The section is a single flat section shared by both platforms; per-key platform scope is explicit. Windows-only keys preserve their Windows behavior and are silently ignored on Linux (the Linux build never references them; mINI ignores unrequested keys). This mirrors the `[Directory] GenDirThumbs` convention (Windows-only, ignored on Linux).

| Key | Scope |
|-----|-------|
| `UserDir` | Defaulted on both platforms (Globals.cpp); honored by WebView2 on Windows. The Linux `QtWebEngineBackend` uses the default Chromium profile and does not apply `UserDir`. |
| `ShowErrorBoxes` | Windows-only. `1` shows a Windows error message box when WebView2 environment creation fails; `0`/absent suppresses it. |
| `CleanupOnExit` | Windows-only. `1` removes the `<UserDir>/EBWebView` cache directory and any plugin temp files on `DLL_PROCESS_DETACH`; `0`/absent leaves them. (There is no process-detach hook on Linux.) |
| `KeepZoom` | Read on both. `1` persists zoom; per-processor isolation is Windows-only (via `gs_ZoomFactor`), while Linux persists a single per-origin zoom value. `0`/absent resets zoom on each load. |

#### Scenario: Error boxes suppressed

- **WHEN** WebView2 creation fails and `[WebView] ShowErrorBoxes=0`
- **THEN** no message box is shown; the load silently returns failure

#### Scenario: Cleanup on exit enabled

- **WHEN** `[WebView] CleanupOnExit=1` and the plugin DLL is unloaded
- **THEN** the `<UserDir>/EBWebView` cache directory and any temp files created by the plugin are deleted

#### Scenario: Zoom persisted across files

- **WHEN** `[WebView] KeepZoom=1` and the user zooms a Markdown view to 150%, then opens another Markdown file
- **THEN** the new Markdown view opens at 150% zoom

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
- **THEN** both builds ignore the key (the `BrowserExecutable*` keys were removed with the `[Chromium]` → `[WebView]` rename) and auto-detect the engine
