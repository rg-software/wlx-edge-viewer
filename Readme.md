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
