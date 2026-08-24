# WLX Edge Viewer

A general-purpose lister plugin for Total Commander (32/64-bit, Windows) and [Double Commander](https://doublecmd.sourceforge.io/) (64-bit, Linux). Both builds share the same source tree via an `IWebView` abstraction (`EdgeViewer/IWebView.h`) with two backends:

- **Windows**: [WebView2](https://developer.microsoft.com/en-us/microsoft-edge/webview2/) (Chromium) via `WebView2Backend` (`EdgeViewer/WebView/WebView2Backend.cpp`)
- **Linux**: [Qt 6](https://www.qt.io/product/qt6) WebEngine via `QtWebEngineBackend` (`EdgeViewer/WebView/QtWebEngineBackend.cpp`)

Configuration files are processed with [mINI](https://github.com/pulzed/mINI).

The following rendering libraries are used:

- Markdown: [marked.js](https://github.com/markedjs/marked), [highlight.js](https://highlightjs.org), and [detect-charset](https://github.com/treyhunner/detect-charset).
- ReStructuredText: [restructured](https://github.com/seikichi/restructured) (converted via [Browserify](https://browserify.org)).
- AsciiDoc: [Asciidoctor.js](https://docs.asciidoctor.org/asciidoctor.js/latest/).
- MHTML: [mhtml2html](https://github.com/rg-contributions/mhtml2html).
- EML: [postal-mime](https://github.com/postalsys/postal-mime).
- Directory: [Thumbnail viewer](https://github.com/rg-contributions/thumbnail-viewer).


The plugin is tested under Windows 10/11 (WebView2 runtime required) and Linux distributions that ship Qt 6.4+ with QtWebEngine (Ubuntu 24.04+, Debian 12+, Fedora 40+, Arch). CHM files are not supported, but they can be opened with [TC SumatraPDF](https://totalcmd.net/plugring/wlx_TCSumatraPDF.html).

## Fine Tuning

Plugin configuration is stored in the `edgeviewer.ini` file, located in the plugin folder.

## Setup

**Windows**: Binary plugin archives come with the setup script. Just enter the archive, and confirm installation.

**Linux**: `cmake --install build --prefix ~/.local` (or copy manually to `~/.doublecmd/plugins/edgeviewer/`).

## Pre-fetched file content (cross-platform)

Every loader-based processor (Markdown, AsciiDoc, RST, MHTML, EML) is implemented via a thin subclass of `BaseFileProcessor` (`EdgeViewer/Processors/BaseFileProcessor.h`). The base class:

1. Reads the loader template (`Resources/assets/<dir>/loader.html`)
2. Reads the actual file content
3. **Base64-encodes the file content** into a placeholder named `__FILE_CONTENT__`
4. Substitutes the other placeholders (`__FILENAME__`, `__CSS_NAME__`, `__BASE_URL__`)
5. Calls `NavigateToString` with the inlined HTML

The loaders read `window["__FILE_CONTENT__"]` (bracket notation, not dot — base64 padding `=` would otherwise break JS syntax). Each loader's render-time helper scripts (`marked.js`, `asciidoctor.min.js`, `restructured.bundle.min.js`, `mhtml2html.min.js`, `postal-mime.min.js`, `highlight.js`, `MathJax`, `Mermaid`) are still loaded normally from the platform's virtual-host equivalent (`http://assets.example/...` on Windows; `ev://assets.example/...` on Linux — see "Linux scheme note" below) — only the **content** is pre-fetched, the renderer is not replaced.

This eliminates the JS-side `fetch()` round-trip for the file content. The loader still has its two-stage render (initial empty body → DOM replace), but the DOM replace happens immediately because the content is already inlined as a base64 string.

**Linux scheme note (Qt Web Engine only)**: Qt Web Engine (Chromium) does not allow registering `http` as a custom URI scheme (Chromium reserves it for actual web traffic). The Linux backend uses a custom scheme `ev://EdgeViewer` and rewrites `http://` → `ev://` in loader HTML before passing to Qt Web Engine. The loaders' templates keep using `http://` references — the rewrite happens in C++. The host→folder map is process-wide; the scheme callback serves files with MIME type guessed by extension (`.css`, `.js`, `.png`, `.svg`, `.json`); everything else falls back to `text/html`.

Processors with pre-fetch: Markdown, AsciiDoc, RST, MHTML, EML. `imgview/loader.html` is **not** updated — it uses `<img src="...">` directly (browser fetches the image), not a JS `fetch()`. Pre-fetching binary images would require inlining as a data URL or Blob URL, both of which have issues with large images. Left as future work.

The pre-fetch pattern falls back to `fetch()` if `window.__FILE_CONTENT__` is absent, so old plugin builds still work against the new loaders and vice versa.

## Known Limitations

### `[HTML] DetectEncoding` removed — HTML files render via web-engine sniffing only

The plugin used to detect the charset of HTML files lacking a BOM or `<meta charset>` declaration and inject a `Content-Type` header to override the engine's default. This `[HTML] DetectEncoding=1` path has been **removed** because:

- Both WebView2 (Chromium) and Qt Web Engine already sniff charset from `<meta charset>` and BOM headers.
- The override leaked into many shared code paths (`gs_Htmls` map, `WebResourceRequested` interceptor, `OverrideEncoding` callback). It was the only feature using that interceptor.
- The override was off by default in the shipped `ini`; almost no users enabled it.

**Affected case:** an HTML file with **no BOM, no `<meta charset>`, and a non-UTF-8 encoding** (e.g. Windows-1251, KOI8-R) will be rendered by the engine's sniffing fallback, which usually picks UTF-8 and may mis-render specific characters. Re-introducing the override is on the future-work list — see below.

### Dark mode — sampling semantics on both platforms

The plugin uses a single global `gs_IsDarkMode` flag, sampled at every
`ListLoad*` / `ListLoadNext*` call, to pick between each processor's
`CSS` and `CSSDark` style overrides. The flag is **not** a live binding
to the system palette: an existing lister keeps its current CSS until
the next load, even if the user toggles the theme in the meantime.

| Platform | Source of `gs_IsDarkMode` | Where the bit comes from |
|----------|---------------------------|--------------------------|
| Windows | `ShowFlags & lcp_darkmode` (bit 0x80) | Total Commander sets the bit based on the active Windows app/theme mode and passes it in the WLX show-flags argument. |
| Linux | `ShowFlags & lcp_darkmode`, **falling back** to `QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark` when the bit is clear | Double Commander's Qt6 WLX caller does not currently propagate `lcp_darkmode` in `ShowFlags` (confirmed empirically: `ShowFlags=0x00` in both KDE light and dark themes). The Qt fallback reads the active system color scheme as reported by the host application's style hints. |

**Implications for users:**

- On Linux, dark mode tracks the system palette (KDE/GNOME/XFCE theme). Switching themes mid-session does not retroactively re-style an open lister; the next `ListLoad*` picks up the new value.
- On Windows, dark mode tracks Total Commander's `lcp_darkmode` bit, which Total Commander sets from the Windows app/theme mode. Same sampling semantics.
- A Linux user who wants dark mode must have a system-wide dark theme (Breeze Dark, Adwaita-dark, etc.). The plugin does not provide an ini override for forcing dark mode independently of the system theme.
- The fallback is **OR** with `lcp_darkmode`: if DC ever starts propagating the bit, dark mode would activate from the bit alone regardless of the system scheme.
- **HTML processor:** CSS injection (`AddApplyStyleScript` analog) is wired on Linux; users who want plugin-side dark styling on HTML pages opt in by setting `[HTML] CSSDark=style-dark.css` in `edgeviewer.ini` (same opt-in semantics as Windows). The shipped `[HTML] CSSDark=none.css` is intentionally a no-op — both platforms ship it this way.

**Implementation:** the Linux branch of `EdgeViewer/DllMain.cpp` defines a `ComputeDarkMode(int showFlags)` helper that ORs the bit with the Qt fallback; the three `gs_IsDarkMode` assignment sites (`DoListLoad`, the Linux `ListLoadNextW`, the Linux `ListPrintW`) call it. Windows code path is unchanged. See `openspec/changes/fix-linux-dark-mode-fallback/`.

### Ctrl+Q quick-view window jumps under native Wayland

**Symptom:** Opening a file with F3 (standalone lister window) works correctly. Opening the same file with Ctrl+Q (quick view, embedded in the panel) makes the plugin's window appear at an unspecified position and Double Commander's main window jumps to match it. The panel does not contain the rendered content.

> **Investigated and shipped (Branch C)** — see [`openspec/changes/revisit-wayland-ctrlq-jump/`](openspec/changes/revisit-wayland-ctrlq-jump/) for the full evidence pack. The root-cause text below reflects instrumented facts, not the (falsified) narrative recorded here before 2026-08. The task-2.5 `qt.qpa.wayland*` logging produced no Qt-level class attribution for the re-created toplevel; the surface-level and KWin attribution is definitive.

**Root cause (confirmed by instrumentation):** On the first Ctrl+Q open of a session, the plugin's `ListLoadW` receives a parent chain in which **no widget carries `Qt::Window`** (top-of-chain window flags `0x8800f000`; Window-type mask `0x1ff` = 0) — the earlier claim that DC's LCL `TQtMainWindow.ChangeParent` retains `Qt::Window` was **falsified**. The viewer form embeds as a **plain child** (`QAbstractScrollArea`-wrapped); `parent->window()` resolves to DC's real main window. The escaping surface is therefore created *after* `ListLoadW` returns. A `WAYLAND_DEBUG=1` trace correlated with the first Ctrl+Q shows what it is: DC's main-window toplevel (`xdg_surface`/`xdg_toplevel` of the "Double Commander 1.2.8~383" window) is **destroyed and re-created as a new `xdg_toplevel`** (`wl_surface#39` → `xdg_surface#57` → `xdg_toplevel#59`, same title, same geometry 1370×866, `app_id=doublecmd`), and Chromium's EGL compositor surface attaches to that re-created toplevel. KWin `queryWindowInfo` on the stray confirms `pid` = the `doublecmd` process, `resourceClass=doublecmd`, `hasTransientParent=false` — a **DC-owned re-created ancestor toplevel**, not a plugin/Chromium subsurface (no `wl_subsurface` appears in either trace). The jump is **first-Ctrl+Q-of-session only**; subsequent opens embed cleanly. F3 (standalone lister) is unaffected. Full evidence pack: `openspec/changes/revisit-wayland-ctrlq-jump/evidence.md`.

**Shipped mitigation: Branch C (documentation-only).** The probe matrix (evidence §6) shows the jump survives `QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu"` alone but is eliminated by adding `QT_QUICK_BACKEND=software` — reproduced on a clean retry. Because only software rendering reliably removes the jump, no plugin C++ change ships (Branches A/B's native-boundary and transient-parent levers are not warranted when the env workaround is decisive and revertible). The env workaround below is the shipped outcome; `[WebView] Switches` engine flags on Linux remain separate future work.

**Workaround (shipped, Branch C):** Force software rendering — eliminates the first-Ctrl+Q jump on the tested KDE/Wayland stack (KWin 6.7.4, DC 1.2.8, Qt 6.11):

```sh
QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu" QT_QUICK_BACKEND=software doublecmd
```

The `QT_QUICK_BACKEND=software` part is what actually removes the jump; `--disable-gpu` alone does not (see the probe matrix in `openspec/changes/revisit-wayland-ctrlq-jump/evidence.md` §6). Cost: Chromium renders without GPU acceleration — expect higher CPU use and slower scrolling/zooming inside the lister. `[WebView] Switches` engine flags on Linux remain separate future work and are intentionally not re-introduced here.

**Workaround (fallback):** Run Double Commander under XWayland:

```sh
QT_QPA_PLATFORM=xcb doublecmd
```

Under XWayland the embedded form just works. With the software-rendering workaround available, XWayland is no longer the only option; it remains the recommended fallback where software rendering is too costly or insufficient for a given compositor.

**Tracking:** A Double Commander issue documenting the re-created-ancestor-toplevel mechanism on first Ctrl+Q (details in `openspec/changes/revisit-wayland-ctrlq-jump/evidence.md`) is posted at github.com/doublecmd/doublecmd — issue URL recorded here once filed (see task 8.2 of that change). The `doublecmd/plugins/wlx/kate/defects.md` notes (Wayland subsurface focus architecture) list the same family of issues.

### ESC closes the lister via in-page JS bridge (not a Qt event filter)

The Linux lister wires ESC through an in-page `keydown` listener that
triggers `new Image().src = 'ev://_close/<id>'` (custom `ev://` scheme,
registered per-instance; `fetch()` is not used because Chromium rejects
custom schemes in its `fetch()` allowlist). The scheme handler looks up
the container `QWidget*` by id and posts a **synthetic `Q` keypress** to
the container's parent widget (DC's viewer panel). DC's hotkey handler
processes the `Q` identically to a physical press, invoking
`cm_ExitViewer` → lister close. This approach was chosen because direct
`hide()`/`close()` on the container from within the scheme handler
either exits DC entirely (synchronous) or has no effect (deferred),
and `QCloseEvent` to the parent is ignored. A `QObject::eventFilter` on
`QWebEngineView` was the first attempt but was abandoned after
instrumentation showed Chromium intercepts keyboard input below Qt's
event system: the filter logged 38 events over a test session but none
were `QEvent::KeyPress` (type 6) or `QEvent::KeyRelease` (type 7). The
image-viewer's fullscreen toggle uses `F` (its own keydown listener) and
is unaffected by the ESC bridge.

### Process overhead — each `ListLoadW` spawns Chromium subprocesses

`QWebEngineView` is backed by a full Chromium renderer (zygote + GPU + renderer processes). The first `ListLoadW` of a session has a noticeable (~hundreds-of-ms) cost compared to lighter Qt widget renderers like `QPdfView`. Subsequent loads in the same session are faster because Chromium reuses the profile's processes. `QT_WEBENGINE_DISABLE_SANDBOX=1` and `--single-process` reduce fork overhead at the cost of stability. This is inherent to using Chromium for the loaders' JS/CSS stack (marked.js, highlight.js, asciidoctor.js, mermaid, mathjax); switching to a lighter web engine (e.g. Qt WebEngine's `webengine-minimal` build) would lose the Chromium-grade rendering we depend on. Document for users, not a defect.

### Future work

The items that have been deliberately deferred (not implemented in the current build) are tracked as future work. They will be re-introduced as separate dedicated changes once a real-world need is confirmed.

| # | Item | Notes |
|---|---|---|
| 1 | **HTML charset override** (`[HTML] DetectEncoding` + `gs_Htmls` + `OverrideEncoding` + `WebResourceRequested` interceptor) | Re-introduce when a user can demonstrate a non-UTF-8 HTML file (no BOM, no `<meta>`) that the engine sniffs incorrectly. Needs to be designed cross-platform (Linux Qt Web Engine + Windows WebView2) without re-introducing the per-platform plumbing that was removed. |
| 2 | Linux dynamic directory thumbnails (Qt Image Provider + KIO / freedesktop thumbnails) | Currently the static `folder.png`/`file.png` icons are used. |
| 3 | Linux native shell-style right-click menu (Qt menu) | Right-click inside the rendered view does nothing on Linux. |
| 4 | Per-processor sticky zoom on Linux | `KeepZoom` ini key is partially effective on Linux: within-session zoom persistence works because `QtWebEngineBackend` reuses the same `QWebEngineView` instance, and cross-Ctrl+Q-session persistence works because Qt Web Engine remembers zoom per origin (`ev://local.example/`). Per-processor isolation (different zoom factors for different file types) does **not** work — Qt Web Engine's per-origin memory gives a single shared zoom value. See `openspec/changes/characterize-linux-parity/specs/linux-parity/spec.md` Row 4 for the manual test. |
| 5 | `[WebView] Switches =...` engine command-line flags (Windows) | The Chromium-specific `Switches` ini key was dropped along with the `[Chromium]` → `[WebView]` rename. Re-introduce only if a real Edge-specific flag is needed (e.g. `--disable-gpu`). |
| 6 | Windows accelerator-key relaying for `Ctrl+1`..`8` (the `KeyQ`/`Digit1..8` JS bridge) | Currently only works on Windows; on Linux Qt Web Engine's own focus handling applies. |
| 7 | Windows `WM_COPYDATA` � `Navigator` direct-call simplification | Optional: investigate whether `ListLoadNextW`/`ListSearchTextW`/`ListPrintW` can be called directly on the WebView's thread without `WM_COPYDATA` IPC. Pending confirmation on Total Commander's calling thread. |
| 8 | Linux-only flicker between ListLoad and first paint (~280ms) | Pre-existing in the spike work; documented but not addressed by the port. |
| 9 | Ctrl+Q quick-view jumps on native Wayland | The previously documented root cause (`TQtMainWindow.ChangeParent` retaining `Qt::Window`) was falsified by instrumentation; the confirmed mechanism is a re-created ancestor toplevel (DC main window's `xdg_toplevel` destroyed and re-created on first Ctrl+Q, `wl_surface#39`/`xdg_toplevel#59`). Shipment: Branch C (documentation-only) — software rendering via `QT_QUICK_BACKEND=software` (+ `QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu"`) removes the jump; XWayland remains the fallback. Full evidence pack + probe matrix in [`openspec/changes/revisit-wayland-ctrlq-jump/`](openspec/changes/revisit-wayland-ctrlq-jump/evidence.md). Track at github.com/doublecmd/doublecmd. |
| 10 | First `ListLoadW` of a session is noticeably heavy | `QWebEngineView` spawns Chromium subprocesses (zygote + GPU + renderer). Inherent to Chromium-backed rendering; switching to a lighter web engine would lose the JS/CSS stack (marked.js, highlight.js, mermaid, mathjax). |

## Development

### Windows build

[MS Visual Studio 2022](https://visualstudio.microsoft.com/) and [vcpkg](https://vcpkg.io) with MSBuild integration are required. Run `BuildMakeSetup.bat` from `MSVS Development Command Prompt` to build the project.

```
msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=x64
```

### Linux build

The Linux backend lives on the `port-to-double-commander-linux` branch. The shared source tree builds via CMake plus Qt6 development packages: `qt6-base-dev`, `qt6-webengine-dev` (Debian/Ubuntu) or the `qt6-qtbase-devel` + `qt6-qtwebengine-devel` equivalents (Fedora/Arch), plus `pkg-config` and `cmake`.

CMake ≥ 3.16 is required for the `cmake -B build -S .` form. If you have an older CMake, use the classic two-step:

```bash
# CMake >= 3.16 (one-liner)
cmake -B build -S .
cmake --build build -j
cmake --install build --prefix ~/.local

# CMake < 3.16 (classic out-of-source build)
mkdir build && cd build
cmake ..
make -j
cd ..
cmake --install build --prefix ~/.local
```

Output: `build/EdgeViewer.wlx64`. The install rules lay out the `.wlx64` next to `assets/` and `edgeviewer.ini` (`~/.local/share/doublecmd/plugins/edgeviewer/`), matching the layout DC expects.

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
