# WLX Edge Viewer

An AsciiDoc, Markdown, and HTML lister plugin for Total Commander (32/64-bit version).

The plugin uses a modern Chromium-based [WebView2](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) component to display documents. Markdown rendering is performed via [hoedown library](https://github.com/hoedown/hoedown). AsciiDoc files are displayed with [Asciidoctor.js](https://docs.asciidoctor.org/asciidoctor.js/latest/). Markdown and AsciiDoc processing should work reliably for UTF-8 and UTF-16 encoded files. The specific encoding type is detected using [text-encoding-detect library](https://github.com/AutoItConsulting/text-encoding-detect).

The plugin is tested under Windows 10 and Windows 11, but should theoretically work on Windows 7 and Windows 8 if WebView2 Runtime is installed. On older machines, use [WLX Markdown Viewer](https://github.com/rg-software/wlx-markdown-viewer) and [HTMLView](https://sites.google.com/site/htmlview/). CHM files are not supported, but they can be opened with [sLister](https://totalcmd.net/plugring/slister.html).

## Fine Tuning

Plugin configuration is stored in the `edgeviewer.ini` file, located in the TC-suggested folder (typically, `<UserDir>\AppData\Roaming\Ghisler`). For example, you can customize the output with CSS files (four themes from [Markdown CSS](https://markdowncss.github.io/) and a [Github-like](https://gist.github.com/tuzz/3331384) theme are included into the package). You can also customize Markdown rendering by modifying hoedown arguments (check [this quick reference](https://htmlpreview.github.io?https://raw.githubusercontent.com/rg-software/wlx-edge-viewer/master/hoedown.html)).

## Setup

Binary plugin archives come with the setup script. Just enter the archive, and confirm installation.

## Development

[MS Visual Studio 2022](https://visualstudio.microsoft.com/) and [vcpkg](https://vcpkg.io) with MSBuild integration are required. Run `setup.bat` to install dependencies, then run `BuildMakeSetup.bat` from `MSVS Development Command Prompt` to build the project.
