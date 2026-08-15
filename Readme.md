# WLX Edge Viewer

A general-purpose lister plugin for Total Commander (32/64-bit version).

The plugin uses a modern Chromium-based [WebView2](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) component to display documents. Configuration files are processed with [mINI](https://github.com/pulzed/mINI).

The following rendering libraries are used:

- Markdown: [marked.js](https://github.com/markedjs/marked), [highlight.js](https://highlightjs.org), and [detect-charset](https://github.com/treyhunner/detect-charset).
- ReStructuredText: [restructured](https://github.com/seikichi/restructured) (converted via [Browserify](https://browserify.org)).
- AsciiDoc: [Asciidoctor.js](https://docs.asciidoctor.org/asciidoctor.js/latest/).
- MHTML: [mhtml2html](https://github.com/rg-contributions/mhtml2html).
- EML: [postal-mime](https://github.com/postalsys/postal-mime).
- Directory: [Thumbnail viewer](https://github.com/rg-contributions/thumbnail-viewer).


The plugin is tested under Windows 10 and Windows 11, but should theoretically work on Windows 7 and Windows 8 if WebView2 Runtime is installed. On older machines, use [WLX Markdown Viewer](https://github.com/rg-software/wlx-markdown-viewer). CHM files are not supported, but they can be opened with [TC SumatraPDF](https://totalcmd.net/plugring/wlx_TCSumatraPDF.html).

## Fine Tuning

Plugin configuration is stored in the `edgeviewer.ini` file, located in the plugin folder.

## Setup

Binary plugin archives come with the setup script. Just enter the archive, and confirm installation.

## Known Limitations

### `[HTML] DetectEncoding` removed — HTML files render via web-engine sniffing only

The plugin used to detect the charset of HTML files lacking a BOM or `<meta charset>` declaration and inject a `Content-Type` header to override the engine's default. This `[HTML] DetectEncoding=1` path has been **removed** because:

- Both WebView2 (Chromium) and WebKitGTK already sniff charset from `<meta charset>` and BOM headers.
- The override leaked into many shared code paths (`gs_Htmls` map, `WebResourceRequested` interceptor, `OverrideEncoding` callback). It was the only feature using that interceptor.
- The override was off by default in the shipped `ini`; almost no users enabled it.

**Affected case:** an HTML file with **no BOM, no `<meta charset>`, and a non-UTF-8 encoding** (e.g. Windows-1251, KOI8-R) will be rendered by the engine's sniffing fallback, which usually picks UTF-8 and may mis-render specific characters. Re-introducing the override is on the future-work list — see below.

### Future work

The items that have been deliberately deferred (not implemented in the current build) are tracked as future work. They will be re-introduced as separate dedicated changes once a real-world need is confirmed.

| # | Item | Notes |
|---|---|---|
| 1 | **HTML charset override** (`[HTML] DetectEncoding` + `gs_Htmls` + `OverrideEncoding` + `WebResourceRequested` interceptor) | Re-introduce when a user can demonstrate a non-UTF-8 HTML file (no BOM, no `<meta>`) that the engine sniffs incorrectly. Needs to be designed cross-platform (Linux WebKitGTK + Windows WebView2) without re-introducing the per-platform plumbing that was removed. |
| 2 | Linux dynamic directory thumbnails (GdkPixbuf + GIO) | Currently the static `folder.png`/`file.png` icons are used. |
| 3 | Linux native shell-style right-click menu (GMenu + GAppInfo) | Right-click inside the rendered view does nothing on Linux. |
| 4 | Per-processor sticky zoom on Linux | `KeepZoom` ini key is currently a no-op on Linux; Linux uses WebKitGTK's built-in Ctrl+wheel zoom. |
| 5 | `[WebView] Switches =...` engine command-line flags (Windows) | The Chromium-specific `Switches` ini key was dropped along with the `[Chromium]` → `[WebView]` rename. Re-introduce only if a real Edge-specific flag is needed (e.g. `--disable-gpu`). |
| 6 | Windows accelerator-key relaying for `Ctrl+1`..`8` (the `KeyQ`/`Digit1..8` JS bridge) | Currently only works on Windows; on Linux WebKitGTK's own focus handling applies. |
| 7 | Windows `WM_COPYDATA` ↔ `Navigator` direct-call simplification | Optional: investigate whether `ListLoadNextW`/`ListSearchTextW`/`ListPrintW` can be called directly on the WebView's thread without `WM_COPYDATA` IPC. Pending confirmation on Total Commander's calling thread. |

## Development

[MS Visual Studio 2022](https://visualstudio.microsoft.com/) and [vcpkg](https://vcpkg.io) with MSBuild integration are required. Run `BuildMakeSetup.bat` from `MSVS Development Command Prompt` to build the project.

### Tests

The solution includes `EdgeViewer.Tests` — a [Catch2](https://github.com/catchorg/Catch2) test project covering pure helpers, config parsing, path handling, and the extracted logic from `WlxDetect`, `ZoomHotkey`, and `Navigator`. Build from the Developer Command Prompt:

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

### Linux build (future)

When the Linux backend lands (see `openspec/changes/port-to-double-commander-linux/`), the build will be driven by CMake plus system packages (`libwebkit2gtk-4.1-dev`, `gtk3-dev`, `pkg-config`), see `EdgeViewer/CMakeLists.txt` once it is added.
