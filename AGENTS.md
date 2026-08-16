# AGENTS.md

Lister plugin (32/64-bit Windows via Total Commander; 64-bit Linux via Double Commander) that renders Markdown, AsciiDoc, RST, HTML, MHT, images, directories and PDF through a WebView2 / WebKitGTK backend. One C++23 source tree; the platform-specific parts are isolated in sibling files (`WebView2Backend.{h,cpp}`, `WebKitBackend.{h,cpp}`, `Platform_Win.cpp`, `Platform_Linux.cpp`, `EdgeLister_Win.cpp`, `EdgeLister_Linux.cpp`). Build system differs per platform: MSBuild + vcpkg (Windows) vs CMake + pkg-config (Linux).

## Build

### Windows

- Requires MS Visual Studio 2022 (v143 toolset) + vcpkg with MSBuild integration. Deps are pinned in `vcpkg.json` (manifest mode): `webview2`, `wil`, `catch2`, static triplets `x86-windows-static`/`x64-windows-static` (set in `EdgeViewer.vcxproj`). `vcpkg_installed/` holds the built triplets (gitignored).
- `msbuild` is not on PATH: run from the "MSVS Developer Command Prompt", or as `BuildMakeSetup.bat` does — `vcvarsall.bat x86|x64` then `msbuild ... /p:UseEnv=true`.
- Release packaging: `BuildMakeSetup.bat` builds Release for both platforms, assembles `Build\Release\` (Resources + `EdgeViewer.wlx` = Win32 DLL renamed, `EdgeViewer.wlx64` = x64 DLL), zips to `Release-YYYYMMDD.zip`.
- Per-config outputs land in `Build\EdgeViewer_<Platform>_<Config>\` (gitignored): Release DLLs are `EdgeViewer-Win32.dll`/`EdgeViewer-x64.dll`, Debug are `EdgeViewerD-...`.
- Tests (Windows): `msbuild EdgeViewer.Tests/EdgeViewer.Tests.vcxproj /p:Configuration=Release /p:Platform=x64` produces `Build\EdgeViewer.Tests_x64_Release\EdgeViewer.Tests.exe` (and same for Win32). 45 tests / 186 assertions on both Windows platforms.

### Linux

- The Linux backend lives on the `port-to-double-commander-linux` branch. Build via CMake plus system packages: `libwebkit2gtk-4.1-dev`, `gtk+-3.0-dev`, `pkg-config`, `cmake`.
  ```bash
  cmake -B build -S .
  cmake --build build -j
  cmake --install build --prefix ~/.local
  ```
  Output: `build/EdgeViewer.wlx.so`. Install rules lay out the `.so` next to a `Resources/` directory (`~/.local/share/doublecmd/plugins/edgeviewer/`), matching the layout DC expects.
- No Linux test suite (manual DC verification per OpenSpec task 6).
- `vcpkg.json` is unchanged on Windows; Linux uses system pkg-config (no vcpkg equivalent).
- Linux export list is built from CMake visibility (no `.def` file): `EdgeViewer.wlx.so` exposes the same WLX symbols Windows does (`ListLoadW`, `ListLoadNextW`, etc.).

### Branching

- `master` — upstream tip (pre-Section-4 work)
- `port-to-double-commander-linux` — Section 2 refactor (IWebView abstraction) + pre-fetch (BaseFileProcessor) + Section 4 Linux backend. **Develop here for Linux work.**
- Tag `windows-refactor-stable` anchors the Windows baseline at `31b9e79` (clean cross-platform refactor before any Linux code). PR Linux work into `master` once it stabilizes.

## Architecture

The canonical architecture/conventions reference is `openspec/config.yaml` → `context`. It is loaded into every session automatically via the `instructions` array in `opencode.jsonc`, so there is a single source of truth — do not duplicate it here.

Key headers (read these before touching the platform split):

- `EdgeViewer/IWebView.h` — 6-method abstract interface shared by both backends
- `EdgeViewer/Processors/BaseFileProcessor.h` — shared `OpenIn` for the 5 text loaders (Markdown, RST, AsciiDoc, MHTML, EML). Subclasses only declare three string getters (css section, loader directory, URL placeholder).
- `EdgeViewer/WebView/WebViewFactory.{h,cpp}` — only shared file with `#ifdef _WIN32`; dispatches to `WebView2Backend` (Windows) or the Linux factory stub (in `WebKitBackend.cpp`)
- `EdgeViewer/Platform.h` — abstract filesystem/env surface; `Platform_Win.cpp` and `Platform_Linux.cpp` provide per-OS implementations

## Known limitations / future work

Several features were deliberately deferred by the `port-to-double-commander-linux` change. The full list with re-introduction criteria is in `Readme.md` ("Future work" table). Most importantly:

- `[HTML] DetectEncoding` ini key and the underlying BOM / `<meta>` / file-content charset detection are **removed**. The web engine's built-in sniffing is the only path. If a user reports a non-UTF-8 HTML file with no BOM and no `<meta charset>` (e.g. Windows-1251, KOI8-R) being mis-rendered, the override must be re-introduced as a separate dedicated change — see `openspec/changes/port-to-double-commander-linux/proposal.md` §Removed and the `design.md` Decision 6 for the future-work note. Do not silently re-add the `OverrideEncoding` / `WebResourceRequested` interceptor in ad-hoc patches: it is a cross-platform design issue (Windows WebView2 + Linux WebKitGTK) that needs its own change.
- Flicker between ListLoad and first paint (~280ms) is pre-existing and documented but not addressed by the port. Likely fixes are CSS-visibility on loaders, `DefaultBackgroundColor`, or hiding the HWND until first paint.
- Linux dynamic directory thumbnails, Linux shell right-click menu, per-processor sticky zoom on Linux, `[WebView] Switches` engine flags, Windows accelerator-key relaying list, `WM_COPYDATA` simplification — all documented in `Readme.md` and the proposal/design.

## Cross-platform port notes (for future contributors)

- `Globals.h` wraps `<windows.h>` in `#ifdef _WIN32` so Linux builds don't try to include it. HWND/HINSTANCE types are visible only on Windows; on Linux the equivalent is `GtkWidget*`.
- The 5 text loaders (Markdown, RST, AsciiDoc, MHTML, EML) read the pre-fetched content via `window["__FILE_CONTENT__"]` (bracket notation) — never `window.__FILE_CONTENT__` (dot notation). Base64 padding `=` collides with JS assignment if you use dot notation.
- The Linux backend uses the `ev://` scheme (custom, not `http://`) per OpenSpec Decision 3 Fallback A (spike-confirmed). WebKitGTK 2.38+ blocks registering `http` as a custom URI scheme. The `WebKitBackend::NavigateToString` rewrites `http://` → `ev://` in loader HTML before passing to WebKitGTK. Loaders can keep using `http://` references — the rewrite is invisible to them.
- `imgview` is intentionally excluded from pre-fetch (uses `<img src>` directly, not JS fetch). HTML/URL/Other processors use `Navigate()` to real file URLs — pre-fetch doesn't apply to them.
