# AGENTS.md

Total Commander WLX lister plugin (32/64-bit) that renders Markdown, AsciiDoc, RST, HTML, MHT, images, directories and PDF via WebView2. One MSVC C++ DLL project; no test suite.

## Build (Windows-only)
- Requires MS Visual Studio 2022 (v143 toolset) + vcpkg with MSBuild integration. Deps are pinned in `vcpkg.json` (manifest mode): `webview2`, `wil`, static triplets `x86-windows-static`/`x64-windows-static` (set in `EdgeViewer.vcxproj`). `vcpkg_installed/` holds the built triplets (gitignored).
- `msbuild` is not on PATH: run from the "MSVS Developer Command Prompt", or as `BuildMakeSetup.bat` does — `vcvarsall.bat x86|x64` then `msbuild ... /p:UseEnv=true`.
- Release packaging: `BuildMakeSetup.bat` builds Release for both platforms, assembles `Build\Release\` (Resources + `EdgeViewer.wlx` = Win32 DLL renamed, `EdgeViewer.wlx64` = x64 DLL), zips to `Release-YYYYMMDD.zip`.
- Per-config outputs land in `Build\EdgeViewer_<Platform>_<Config>\` (gitignored): Release DLLs are `EdgeViewer-Win32.dll`/`EdgeViewer-x64.dll`, Debug are `EdgeViewerD-...`.
- No tests or linter. Verify by building and loading the plugin in Total Commander (`Configuration > Options > Plugins > Lister plugins`); `Examples/` holds sample files (md, adoc, rst, mhtml, svg, url) for manual checks.

## Architecture
The canonical architecture/conventions reference is `openspec/config.yaml` → `context`. It is loaded into every session automatically via the `instructions` array in `opencode.jsonc`, so there is a single source of truth — do not duplicate it here.

## Known limitations / future work
Several features were deliberately deferred by the `port-to-double-commander-linux` change. The full list with re-introduction criteria is in `Readme.md` ("Future work" table). Most importantly:

- `[HTML] DetectEncoding` ini key and the underlying BOM / `<meta>` / file-content charset detection are **removed**. The web engine's built-in sniffing is the only path. If a user reports a non-UTF-8 HTML file with no BOM and no `<meta charset>` (e.g. Windows-1251, KOI8-R) being mis-rendered, the override must be re-introduced as a separate dedicated change — see `openspec/changes/port-to-double-commander-linux/proposal.md` §Removed and the `design.md` Decision 6 for the future-work note. Do not silently re-add the `OverrideEncoding` / `WebResourceRequested` interceptor in ad-hoc patches: it is a cross-platform design issue (Windows WebView2 + Linux WebKitGTK) that needs its own change.

The other deferred items (Linux dynamic directory thumbnails, Linux shell right-click menu, per-processor sticky zoom on Linux, `[WebView] Switches` engine flags, Windows accelerator-key relaying list, `WM_COPYDATA` simplification) are documented in `Readme.md` and the proposal/design.
