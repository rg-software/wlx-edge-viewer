# AGENTS.md

Lister plugin (32/64-bit Windows via Total Commander; 64-bit Linux via Double Commander) that renders Markdown, AsciiDoc, RST, HTML, MHT, images, directories and PDF through a WebView2 / Qt Web Engine backend. One C++23 source tree; the platform-specific parts are isolated in sibling files (`WebView2Backend.{h,cpp}`, `QtWebEngineBackend.{h,cpp}`, `Platform_Win.cpp`, `Platform_Linux.cpp`, `EdgeLister_Win.cpp`, `EdgeLister_Linux.cpp`, `DirProcessor_Win.cpp`). Build system differs per platform: MSBuild + vcpkg (Windows) vs CMake + Qt6 (Linux).

## Build

### Windows

- Requires MS Visual Studio 2022 (v143 toolset) + vcpkg with MSBuild integration. Deps are pinned in `vcpkg.json` (manifest mode): `webview2`, `wil`, `catch2`, static triplets `x86-windows-static`/`x64-windows-static` (set in `EdgeViewer.vcxproj`). `vcpkg_installed/` holds the built triplets (gitignored).
- `msbuild` is not on PATH: run from the "MSVS Developer Command Prompt", or as `BuildMakeSetup.bat` does — `vcvarsall.bat x86|x64` then `msbuild ... /p:UseEnv=true`.
- Release packaging: `BuildMakeSetup.bat` builds Release for both platforms, assembles `Build\Release\` (Resources + `EdgeViewer.wlx` = Win32 DLL renamed, `EdgeViewer.wlx64` = x64 DLL), zips to `Release-YYYYMMDD.zip`.
- Per-config outputs land in `Build\EdgeViewer_<Platform>_<Config>\` (gitignored): Release DLLs are `EdgeViewer-Win32.dll`/`EdgeViewer-x64.dll`, Debug are `EdgeViewerD-...`.
- Tests (Windows): `msbuild EdgeViewer.Tests/EdgeViewer.Tests.vcxproj /p:Configuration=Release /p:Platform=x64` produces `Build\EdgeViewer.Tests_x64_Release\EdgeViewer.Tests.exe` (and same for Win32). 45 tests / 186 assertions on both Windows platforms.

### Linux

- The Linux backend lives on the `port-to-double-commander-linux` branch. Build via CMake plus Qt6 development packages: `qt6-base-dev`, `qt6-webengine-dev` (Debian/Ubuntu) or the `qt6-qtbase-devel` + `qt6-qtwebengine-devel` equivalents (Fedora/Arch), plus `pkg-config` and `cmake`.
  ```bash
  cmake -B build -S .
  cmake --build build -j
  cmake --install build --prefix ~/.local
  ```
  Output: `build/EdgeViewer.wlx64`. Install rules lay out the `.wlx64` with `assets/` and `edgeviewer.ini` next to it (`~/.local/share/doublecmd/plugins/edgeviewer/`) — `ProcessorInterface::assetsPath()` is `GetModulePath()/assets`, matching the Windows package layout where `BuildMakeSetup.bat` flattens `Resources\` contents beside the DLL.
- No Linux test suite (manual DC verification per OpenSpec task 6).
- `vcpkg.json` is unchanged on Windows; Linux uses system pkg-config (no vcpkg equivalent).
- Linux exports are controlled by a GNU ld version script (`CMakeLists.txt` builds `EdgeViewer.version`: `global: ListLoadW; ...; local: *;`) with the 12 WLX symbols declared `extern "C"` in `DllMain.cpp`. Do **not** combine this with `-fvisibility=hidden`: empirically, hidden-visibility symbols are NOT exported by GNU ld even when listed in the version script, so the CMake visibility presets were removed. `EdgeViewer.wlx64` exposes the same WLX symbols Windows does (`ListLoadW`, `ListLoadNextW`, etc.).

### Branching

- `master` — upstream tip (pre-Section-4 work)
- `port-to-double-commander-linux` — Section 2 refactor (IWebView abstraction) + pre-fetch (BaseFileProcessor) + Section 4 Linux backend. **Develop here for Linux work.**
- Tag `windows-refactor-stable` anchors the Windows baseline at `5e44484` (clean cross-platform refactor before any Linux code). PR Linux work into `master` once it stabilizes.
- The port branch has an orphaned/rewritten history (no common ancestor with `master`), but its code is a functional superset: it already absorbs every `master` fix up to the baseline (issues #41–#49 — YAML frontmatter, Q-key `event.code`, zoom numeric keypad, horizontal-rule CSS, EML, remove Close button). The pre-rewrite commit `31b9e79` ("Strip diagnostic Log::Line calls…") is dangling and referenced only for archaeology — do not treat it as an ancestor.

## Architecture

The canonical architecture/conventions reference is `openspec/config.yaml` → `context`. It is loaded into every AI-assisted session automatically (local tooling, not part of the repo), so there is a single source of truth — do not duplicate it here.

Key headers (read these before touching the platform split):

- `EdgeViewer/IWebView.h` — 6-method abstract interface shared by both backends
- `EdgeViewer/Processors/BaseFileProcessor.h` — shared `OpenIn` for the 5 text loaders (Markdown, RST, AsciiDoc, MHTML, EML). Subclasses only declare three string getters (css section, loader directory, URL placeholder).
- `EdgeViewer/WebView/WebViewFactory.{h,cpp}` — Windows-only; the header is fully `#ifdef _WIN32`-guarded (compiles to nothing on Linux) and dispatches to `WebView2Backend`. The Linux branch constructs `WebKitBackend` directly in `EdgeLister_Linux.cpp` and never enters this file.
- `EdgeViewer/Platform.h` — abstract filesystem/env surface; `Platform_Win.cpp` and `Platform_Linux.cpp` provide per-OS implementations
- `EdgeViewer/Processors/DirProcessor.{h,cpp}` + `DirProcessor_Win.cpp` — the GDI+/shell thumbnail code lives in `DirProcessor_Win.cpp` (`#ifdef _WIN32`-guarded); the header declares it under `#ifdef _WIN32` and Linux uses static `folderThumb`/`fileThumb` icons.

## Known limitations / future work

Several features were deliberately deferred by the `port-to-double-commander-linux` change. The full list with re-introduction criteria is in `openspec/notes/future-work.md` (the user-facing `Readme.md` only summarizes them). Most importantly:

- `[HTML] DetectEncoding` ini key and the underlying BOM / `<meta>` / file-content charset detection are **removed**. The web engine's built-in sniffing is the only path. If a user reports a non-UTF-8 HTML file with no BOM and no `<meta charset>` (e.g. Windows-1251, KOI8-R) being mis-rendered, the override must be re-introduced as a separate dedicated change — see `openspec/changes/port-to-double-commander-linux/proposal.md` §Removed and the `design.md` Decision 6 for the future-work note. Do not silently re-add the `OverrideEncoding` / `WebResourceRequested` interceptor in ad-hoc patches: it is a cross-platform design issue (Windows WebView2 + Linux Qt Web Engine) that needs its own change.
- Flicker between ListLoad and first paint (~280ms) is pre-existing and documented but not addressed by the port. Likely fixes are CSS-visibility on loaders, `DefaultBackgroundColor`, or hiding the HWND until first paint.
- **Native-Wayland Ctrl+Q quick-view jump** — On native Wayland, opening a lister via Ctrl+Q (quick view) places the plugin window at an unspecified location and Double Commander's main window jumps to match it; F3 (standalone lister window) is unaffected; **first Ctrl+Q of a session only** (subsequent opens embed cleanly). The previously documented root cause (`TQtMainWindow.ChangeParent` retaining `Qt::Window`) was **falsified by instrumentation**: no widget in the chain DC hands to `ListLoadW` carries `Qt::Window` (top-of-chain flags `0x8800f000`, Window-type mask zero); the form embeds as a plain child and `parent->window()` resolves to DC's main window, so the escaping top-level surface is created after `ListLoadW`. The confirmed mechanism (evidence pack) is a **re-created ancestor toplevel**: on first Ctrl+Q, DC's main-window `xdg_toplevel` is destroyed and re-created (`wl_surface#39`/`xdg_surface#57`/`xdg_toplevel#59`, same title/geometry/app_id), and Chromium's EGL compositor attaches to it — no `wl_subsurface` anywhere; KWin `queryWindowInfo` returns the `doublecmd` PID with no transient parent. **Shipped: Branch C (documentation-only)** — the probe matrix proved `QT_QUICK_BACKEND=software` (alone; `QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu"` is not needed) eliminates the jump, so no C++ change landed; XWayland is demoted to fallback. Prior plugin-side mitigations (own container, deferred `show()`, `createWinId()`, flag-strip, QShowEvent deferrals, Initialize-time pool) remain exhausted-and-reverted. Full record: [`openspec/changes/archive/2026-08-24-revisit-wayland-ctrlq-jump/evidence.md`](openspec/changes/archive/2026-08-24-revisit-wayland-ctrlq-jump/evidence.md). Track at github.com/doublecmd/doublecmd.
- **QWebEngineView process overhead** — Each `QWebEngineView` spawns Chromium subprocesses (zygote + renderer + GPU). The first `ListLoadW` of a session therefore has a noticeable ~hundreds-of-ms cost compared to a plain Qt widget renderer like `QPdfView`. Subsequent loads are faster (Chromium reuses the profile's processes), and `QT_WEBENGINE_DISABLE_SANDBOX=1` plus `--single-process` reduce fork overhead at the cost of stability. This is inherent to using a full Chromium renderer for the loaders' JS/CSS stack; switching to a lighter web engine (e.g. Qt WebEngine's `webengine-minimal` feature) would lose the Chromium-grade rendering we depend on (marked.js, highlight.js, mermaid, mathjax). Document for users, not a defect.
- Linux dynamic directory thumbnails, Linux shell right-click menu, per-processor sticky zoom on Linux, Windows accelerator-key relaying list — all documented in `openspec/notes/future-work.md` and the proposal/design. (`WM_COPYDATA` was already simplified on Windows: Spike 2 confirmed same-thread WLX callbacks, so `ListLoadNextW`/`ListSearchTextW`/`ListPrintW` call `Navigator` directly; only the JS→host `CMD_MENU` hop remains.)

## Git commits

- Author commits using the git identity (`Maxim Mozgovoy <mozgovoy@u-aizu.ac.jp>`), which is already configured in `~/.gitconfig`. Never override the author with `git -c user.name=... -c user.email=...`, with `GIT_AUTHOR_NAME`/`GIT_AUTHOR_EMAIL`/`GIT_COMMITTER_NAME`/`GIT_COMMITTER_EMAIL` env vars, or with `--author`. The only legitimate commit authors in this repo are listed in `.mailmap` (currently just the human); anything else is a regression and must be fixed with `git filter-repo --mailmap .mailmap --force`.
- Don't run `git commit`, `git push`, `git reset`, or `git rebase` without explicit user confirmation each time.

## Cross-platform port notes (for future contributors)

- `Globals.h` wraps `<windows.h>` in `#ifdef _WIN32` so Linux builds don't try to include it. HWND/HINSTANCE types are visible only on Windows; on Linux the equivalent is `GtkWidget*`.
- The 5 text loaders (Markdown, RST, AsciiDoc, MHTML, EML) read the pre-fetched content from the base64 string literal inlined at the `"__FILE_CONTENT__"` placeholder (never `window.__FILE_CONTENT__` — dot notation makes base64 padding `=` collide with JS assignment; and a `window["__FILE_CONTENT__"]` lookup also breaks because `replacePlaceholders` regex-replaces the token inside the brackets). The `fetch()` branch is only a fallback for builds that don't pre-fetch.
- The Linux backend uses the `ev://` scheme (custom, not `http://`) per OpenSpec Decision 3 Fallback A (spike-confirmed). Qt Web Engine does not allow registering `http` as a custom URI scheme (Chromium reserves it). The `QtWebEngineBackend::NavigateToString` rewrites `http://` → `ev://` in loader HTML before passing to Qt Web Engine. Loaders can keep using `http://` references — the rewrite is invisible to them.
- `imgview` is intentionally excluded from pre-fetch (uses `<img src>` directly, not JS fetch). HTML/URL/Other processors use `Navigate()` to real file URLs — pre-fetch doesn't apply to them.
