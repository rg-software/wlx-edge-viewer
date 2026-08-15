# AGENTS.md

Total Commander WLX lister plugin (32/64-bit) that renders Markdown, AsciiDoc, RST, HTML, MHT, images, directories and PDF via WebView2. One MSVC C++ DLL project; no test suite.

## Build (Windows-only)
- Requires MS Visual Studio 2022 (v143 toolset) + vcpkg with MSBuild integration. Deps are pinned in `vcpkg.json` (manifest mode): `webview2`, `wil`, static triplets `x86-windows-static`/`x64-windows-static` (set in `EdgeViewer.vcxproj`). `vcpkg_installed/` holds the built triplets (gitignored).
- `msbuild` is not on PATH: run from the "MSVS Developer Command Prompt", or as `BuildMakeSetup.bat` does — `vcvarsall.bat x86|x64` then `msbuild ... /p:UseEnv=true`.
- Release packaging: `BuildMakeSetup.bat` builds Release for both platforms, assembles `Build\Release\` (Resources + `EdgeViewer.wlx` = Win32 DLL renamed, `EdgeViewer.wlx64` = x64 DLL), zips to `Release-YYYYMMDD.zip`.
- Per-config outputs land in `Build\EdgeViewer_<Platform>_<Config>\` (gitignored): Release DLLs are `EdgeViewer-Win32.dll`/`EdgeViewer-x64.dll`, Debug are `EdgeViewerD-...`.
- The `$(SolutionDir)Hoedown\src\`/`lib\` paths in `EdgeViewer.vcxproj` are dead references — no such folder exists and no code includes it. Don't "fix" them; the build works without Hoedown.
- No tests or linter. Verify by building and loading the plugin in Total Commander (`Configuration > Options > Plugins > Lister plugins`); `Examples/` holds sample files (md, adoc, rst, mhtml, svg, url) for manual checks.

## Architecture
- TC WLX exports live in `DllMain.cpp` (`ListLoadW`, `ListLoadNextW`, `ListSearchTextW`, `ListPrintW`, `ListCloseWindow`, `ListGetDetectString`), exported via `EdgeViewer.def`.
- File types are per-type classes in `EdgeViewer/Processors/` implementing `ProcessorInterface` (`InitPath()` picks the type, `OpenIn()` renders). Each self-registers via a namespace-scope instance (`namespace { MdProcessor md; }`); no manual registry wiring.
- Rendering: processors read a `loader.html` template from `Resources/assets/<type>/`, replace `__PLACEHOLDER__` tokens, then `NavigateToString`. Real HTML files instead load via `http://html.example/` so per-file encoding is overridden (`OverrideEncoding` in `WebView2.cpp`).
- Virtual host mapping (`ProcessorInterface::mapDomains`): `assets.example` → `<plugin dir>\assets`, `local.example` → the file's root dir.
- Most rendering logic is static JS/CSS under `Resources/assets/` (marked.js, highlight.js, asciidoctor.js, mermaid, mathjax, thumbnailViewer, mhtml2html, ...). Changing renderer behavior usually means editing those files, not C++.
- Config `edgeviewer.ini` sits next to the DLL, parsed with mINI (header-only, `mINI/src/`); `GlobalSettings()` lazy-loads it once. `ListGetDetectString` builds the TC detect string from the `[Extensions]` section — adding a type section there must also be reflected in `DllMain.cpp`.
- C++ standard is `stdcpplatest`; code uses `std::format`, `std::string::starts_with`, `std::map::contains`, WRL/wil COM wrappers. Stick to those idioms; new deps must be added to `vcpkg.json`.
- Dark mode: TC passes `lcp_darkmode`, setting `gs_IsDarkMode` which selects `CSSDark` ini values.
