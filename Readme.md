# WLX Edge Viewer

A general-purpose lister plugin for [Total Commander](https://www.ghisler.com/) (32/64-bit, Windows) and [Double Commander](https://doublecmd.sourceforge.io/) (64-bit, Linux). It renders documents inside the file panel using a real web engine instead of a plain-text view, with syntax highlighting, math formulas (MathJax), diagrams (Mermaid), dark mode, zoom, text search and printing.

Rendering backends:

- **Windows**: [WebView2](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) (Chromium)
- **Linux**: [Qt 6 WebEngine](https://www.qt.io/product/qt6)

The plugin is tested under Windows 10/11 (WebView2 runtime required) and Linux distributions that ship Qt 6.4+ with QtWebEngine (Ubuntu 24.04+, Debian 12+, Fedora 40+, Arch). CHM files can be opened with [Multidoc Viewer](https://github.com/rg-software/wlx-multidoc-viewer).

## Supported formats

| Format | Rendered with |
|--------|---------------|
| Markdown (`.md`, `.markdown`) | [marked.js](https://github.com/markedjs/marked), [highlight.js](https://highlightjs.org), [MathJax](https://www.mathjax.org), [Mermaid](https://mermaid.js.org), [detect-charset](https://github.com/treyhunner/detect-charset) |
| AsciiDoc | [Asciidoctor.js](https://docs.asciidoctor.org/asciidoctor.js/latest/) |
| reStructuredText | [restructured](https://github.com/seikichi/restructured) |
| HTML / XHTML / XML (`.html`, `.htm`, `.xhtml`, `.xml`) | rendered natively by the web engine |
| MHT / MHTML | [mhtml2html](https://github.com/rg-contributions/mhtml2html) |
| EML email files | [postal-mime](https://github.com/postalsys/postal-mime) |
| Images | built-in `<img>` rendering with the [thumbnail viewer](https://github.com/rg-contributions/thumbnail-viewer) (zoom, pan, fullscreen) |
| Directories | thumbnail listing; real shell thumbnails on Windows (`GenDirThumbs`), static icons elsewhere |
| `.url` internet shortcuts | navigates to the target page |
| Everything else (incl. PDF) | loaded directly by the web engine (Chromium's built-in PDF viewer handles PDF) |

Configuration files are processed with [mINI](https://github.com/pulzed/mINI).

## Setup

**Windows**: Binary plugin archives come with the setup script. Just enter the archive, and confirm installation.

**Linux**: unpack the release archive into `~/.local/share/doublecmd/plugins/edgeviewer/`, then register it in Double Commander (*Configuration → Options → Plugins → Lister plugins → Add*).

## Configuration

Plugin configuration is stored in the `edgeviewer.ini` file, located in the plugin folder. The shipped template documents every available key. Highlights:

- Per-format styling: each `[<Format>]` section accepts `CSS` and `CSSDark` stylesheet overrides picked from the bundled stylesheets.
- `[WebView] Switches`: extra command-line flags for the web engine (e.g. `--disable-gpu`).
- `[WebView] BrowserExecutableX86Folder` / `BrowserExecutableX64Folder`: use a specific WebView2 Runtime instead of the installed one.
- `[WebView] CleanupOnExit`: remove the browser cache directory and temp files when the plugin unloads (Windows).
- `[WebView] KeepZoom`: remember the zoom factor per format across files.
- `[WebView] OfflineMode`: every request that does not resolve to plugin-local content (e.g., remote images and styles) is blocked before any network access.
- `[Extensions] ForcedHtmlExt`: extensions whose content should be treated as HTML even when their extension says otherwise (e.g., `xml`, `xhtml`).

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| `Ctrl+` / `Ctrl-` / `0` / `Ctrl`+Wheel | Zoom in / out / reset through fixed steps |
| `F` | Toggle fullscreen in the Image viewer |
| `Ctrl+H` / `Ctrl+G` / `F` and `Shift+F` | Show/hide names, show/hide folders, fit thumbnail to screen in Directory viewer|

## Encoding handling

Codepage of legacy HTML is autodetected with [jschardet](https://github.com/aadsm/jschardet). It is always possible to right-click inside an HTML or MHT view and choose another code page from the **Encoding** submenu. Set `[HTML] ForceDetectEncoding=1` to also correct HTML files that *declare* a wrong charset (overrides Chromium detection functionality).

## Known limitations

### Dark mode is applied when a file is opened

Dark mode is set at load time, so an already-open document keeps its current style until you open another file. On Linux, dark mode follows the system color scheme (Double Commander does not yet forward TC's dark-mode flag). For plain HTML pages, plugin-side dark styling is opt-in via `[HTML] CSSDark=style-dark.css` (use `none.css` to switch off custom styling).

### Ctrl+Q quick-view window jumps under native Wayland

Opening a file with `Ctrl+Q` (quick view embedded in the panel) as the *first* lister open of a session places the rendered content in a separate window near the screen center and Double Commander's main window jumps to match. This can be prevented by switching to software rendering:

```sh
QT_QUICK_BACKEND=software doublecmd
```

### Hotkeys intercepted by DC on Linux

When a single image is opened via F3 (ImgProcessor), Double Commander intercepts the `F` and `Shift+F` keys before they reach the embedded QWebEngineView. The fullscreen toggle in our JavaScript handler never fires. There are possibly other similar hotkey-intercepting issues on Linux.

## Development

### Windows build

[MS Visual Studio 2022](https://visualstudio.microsoft.com/) and [vcpkg](https://vcpkg.io) with MSBuild integration are required. Run `BuildMakeSetup.bat` from `MSVS Development Command Prompt` to build the project.

### Linux build

The Linux backend requires a C++23-capable compiler, CMake ≥ 3.16, `pkg-config`, `zip`, and the Qt 6 development packages: `qt6-base-dev`, `qt6-webengine-dev` (Debian/Ubuntu) or `qt6-qtbase-devel`, `qt6-qtwebengine-devel` (Fedora/Arch).

Run `./BuildMakeSetup.sh` to build the project.

### Tests (Windows-only)

The solution includes a [Catch2](https://github.com/catchorg/Catch2)-backed test project`EdgeViewer.Tests`. Build and run from the Developer Command Prompt:

```
msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=x64
winbuild\EdgeViewer.Tests_x64_Release\EdgeViewer.Tests.exe
```

See `EdgeViewer.Tests/readme.md` for the full tier model and coverage details.

## Project documentation

Developer-facing design records live under [`openspec/`](openspec/config.yaml).

- [`openspec/specs/`](openspec/specs/): capability specifications (the behavioral contract of the plugin).
- [`openspec/notes/rendering-pipeline.md`](openspec/notes/rendering-pipeline.md): how loaders receive file content (pre-fetch/base64) and the Linux `ev://` scheme rewrite.
- [`openspec/notes/future-work.md`](openspec/notes/future-work.md): deliberately deferred features with re-introduction criteria.
