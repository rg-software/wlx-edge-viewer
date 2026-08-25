# WLX Edge Viewer

A general-purpose lister plugin for Total Commander (32/64-bit, Windows) and [Double Commander](https://doublecmd.sourceforge.io/) (64-bit, Linux). It renders documents inside the file panel using a real web engine instead of a plain-text view, with syntax highlighting, math formulas (MathJax), diagrams (Mermaid), dark mode, zoom, text search and printing.

Rendering backends:

- **Windows**: [WebView2](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) (Chromium)
- **Linux**: [Qt 6 WebEngine](https://www.qt.io/product/qt6)

The plugin is tested under Windows 10/11 (WebView2 runtime required) and Linux distributions that ship Qt 6.4+ with QtWebEngine (Ubuntu 24.04+, Debian 12+, Fedora 40+, Arch). CHM files are not supported, but they can be opened with [TC SumatraPDF](https://totalcmd.net/plugring/wlx_TCSumatraPDF.html).

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

**Linux**: unpack the release archive into `~/.local/share/doublecmd/plugins/edgeviewer/`, then register it in Double Commander (*Configuration → Options → Plugins → Lister plugins → Add*):

```sh
mkdir -p ~/.local/share/doublecmd/plugins/edgeviewer
unzip Release-YYYYMMDD-Linux.zip -d ~/.local/share/doublecmd/plugins/edgeviewer/
```

## Configuration

Plugin configuration is stored in the `edgeviewer.ini` file, located in the plugin folder. The shipped template documents every available key. Highlights:

- Per-format styling: each `[<Format>]` section accepts `CSS` and `CSSDark` stylesheet overrides picked from the bundled stylesheets.
- `[WebView] Switches` — extra command-line flags for the web engine (e.g. `--disable-gpu`).
- `[WebView] BrowserExecutableX86Folder` / `BrowserExecutableX64Folder` — use a specific WebView2 Runtime instead of the installed one.
- `[WebView] CleanupOnExit` — remove the browser cache directory and temp files when the plugin unloads (Windows).
- `[WebView] KeepZoom` — remember the zoom factor per format across files.
- `[WebView] OfflineMode=1` — render offline: every request that does not resolve to plugin-local content (plugin assets, the viewed file, inline data) is blocked before any network access; remote images, styles and `.url` targets are not fetched. Default `0`.
- `[Extensions] ForcedHtmlExt` — extensions whose content should be treated as HTML even when their extension says otherwise (shipped: `xml|xhtml`).

## Keyboard shortcuts

| Key | Action | Availability |
|-----|--------|--------------|
| `Esc` | Close the lister | Both platforms |
| `Q` | Close the lister | Windows |
| `1`–`8` | Switch Total Commander's Quick View tabs | Windows |
| `Ctrl` + `Plus` / `Minus` / `0` (also numpad) | Zoom in / out / reset through fixed steps | Windows; on Linux the engine's own `Ctrl+wheel` / `Ctrl+0` / `Ctrl+plus` / `Ctrl+minus` applies |
| `F` | Toggle fullscreen in the image viewer | Both platforms |
| `Ctrl+H` / `Ctrl+G` / `F` | Show/hide names, show/hide folders, fit thumbnails to screen | Directory view |

## Manual encoding override

Legacy-codepage files whose declared encoding lies (or is absent) can be fixed in place: **right-click** inside an HTML or MHT view and pick a code page from the **Encoding** submenu of the context menu (Windows-1251, KOI8-R, Shift-JIS, GBK, Big5, and other common labels, plus `Auto-detect` to reset). The override is transient — it lives only in the current view until the next navigation/reload, nothing is remembered or written to disk. Other formats keep the stock context menu.

## Known limitations

### HTML files without a charset declaration may render with wrong encoding

Files with **no BOM, no `<meta charset>` tag, and a non-UTF-8 encoding** (e.g. Windows-1251, KOI8-R) are decoded by the web engine's built-in sniffing on load, which usually assumes UTF-8 and may mis-render some characters. Use the right-click **Encoding** submenu (above) to re-display the file with the correct code page; re-saving as UTF-8 remains the permanent fix. Automatic detection was deliberately not reintroduced — see [future-work item 1](openspec/notes/future-work.md).

### Dark mode is applied when a file is opened

Dark mode is sampled at each load, so an already-open document keeps its current style until you open another file — toggling the system theme mid-view does not restyle it live. On Linux, dark mode follows the system color scheme (Double Commander does not yet forward TC's dark-mode flag). There is no ini override to force dark mode independent of the system theme. For plain HTML pages, plugin-side dark styling is opt-in via `[HTML] CSSDark=style-dark.css`.

### Ctrl+Q quick-view window jumps under native Wayland

**Symptom:** opening a file with `Ctrl+Q` (quick view embedded in the panel) as the *first* lister open of a session places the rendered content in a separate window near the screen center and Double Commander's main window jumps to match. Later `Ctrl+Q` opens embed cleanly, and `F3` (standalone lister) is unaffected.

**Cause (instrumented):** on that first quick-view open, Double Commander destroys and re-creates its own main window's Wayland toplevel surface, and the web engine's compositor attaches to the newly created surface — the content escapes the panel because of the recreated ancestor, not because of a plugin-created window.

**Workaround:** force software rendering for the session — eliminates the jump (at the cost of higher CPU use while viewing):

```sh
QT_QUICK_BACKEND=software doublecmd
```

Fallback: run Double Commander under XWayland (`QT_QPA_PLATFORM=xcb doublecmd`). Full investigation record: [`openspec/changes/archive/2026-08-24-revisit-wayland-ctrlq-jump/evidence.md`](openspec/changes/archive/2026-08-24-revisit-wayland-ctrlq-jump/evidence.md).

### First file open of a session is slower

The web engine starts its rendering processes when the first document is opened, which adds a noticeable delay to the first view of a session (mainly visible on Linux). Subsequent opens reuse the running processes and are fast. This is inherent to browser-grade rendering and not scheduled to change ([future-work item 8](openspec/notes/future-work.md)).

### Other Linux gaps

Dynamic directory thumbnails, a right-click context menu inside the view, per-format zoom memory, and the digit-key tab switching above are Windows-only today. The full deferred list with rationale lives in [`openspec/notes/future-work.md`](openspec/notes/future-work.md).

## Development

### Windows build

[MS Visual Studio 2022](https://visualstudio.microsoft.com/) and [vcpkg](https://vcpkg.io) with MSBuild integration are required. Run `BuildMakeSetup.bat` from `MSVS Development Command Prompt` to build the project.

```
msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=x64
```

### Linux build

The Linux backend lives on the `port-to-double-commander-linux` branch. Requirements: a C++23-capable compiler (the default GCC/Clang of any distribution shipping Qt 6.4+ suffices), CMake ≥ 3.16, `pkg-config`, `zip`, and the Qt 6 development packages — `qt6-base-dev`, `qt6-webengine-dev` (Debian/Ubuntu) or `qt6-qtbase-devel`, `qt6-qtwebengine-devel` (Fedora/Arch).

Run `./BuildMakeSetup.sh`: it configures and builds Release (`build/EdgeViewer.wlx64`) and assembles a distributable `Release-YYYYMMDD-Linux.zip` containing the plugin, `assets/`, and `edgeviewer.ini` (mirrors the Windows `BuildMakeSetup.bat` workflow).

**Branching**: `master` carries the upstream tip (no Section 4 work). `port-to-double-commander-linux` carries the IWebView refactor + pre-fetch + Linux backend. Develop on `port-to-double-commander-linux`; PR against `master` when the port stabilizes.

### Tests

The solution includes `EdgeViewer.Tests` — a [Catch2](https://github.com/catchorg/Catch2) test project covering pure helpers, config parsing, path handling, the IWebView mock (tier 5 — verifies processors call RegisterVirtualHost + NavigateToString in the right order), and the extracted logic from `WlxDetect`, `ZoomHotkey`, and `Navigator`. Build from the Developer Command Prompt:

```
msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=x64
```

Run the test executable:

```
Build\EdgeViewer.Tests_x64_Release\EdgeViewer.Tests.exe
```

For a quick smoke run:

```
Build\EdgeViewer.Tests_x64_Release\EdgeViewer.Tests.exe "[smoke]"
```

See `EdgeViewer.Tests/readme.md` for the full tier model and coverage details.

## Project documentation

Developer-facing design records live under [`openspec/`](openspec/config.yaml):

- [`openspec/specs/`](openspec/specs/) — capability specifications (the behavioral contract of the plugin)
- [`openspec/notes/rendering-pipeline.md`](openspec/notes/rendering-pipeline.md) — how loaders receive file content (pre-fetch/base64) and the Linux `ev://` scheme rewrite
- [`openspec/notes/future-work.md`](openspec/notes/future-work.md) — deliberately deferred features with re-introduction criteria
