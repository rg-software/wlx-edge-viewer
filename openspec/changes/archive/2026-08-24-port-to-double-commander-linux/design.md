## Context

Today the plugin is a single MSVC C++ DLL targeting Windows 32- and 64-bit. It embeds WebView2 via COM, uses WIL wrappers throughout, builds through MSBuild with vcpkg-pinned `webview2`/`wil` (static triplets), and exposes the WLX contract through `EdgeViewer.def`. All platform awareness is baked into a single tree: `Globals.h` directly includes `<windows.h>`, `<wil/com.h>`, `<webview2.h>`; `ProcessorInterface` exposes `wil::com_ptr<ICoreWebView2>` as `ViewPtr`; `WebView2.cpp` and `EdgeLister.cpp` are 100% Win32/COM. See `proposal.md` for the motivation behind this change and `specs/linux-runtime/spec.md` for the behavior contract.

The Double Commander WLX SDK (`sdk/wlxplugin.h`, `sdk/common.h`) re publishes the same WLX function names and types as Total Commander, with `HWND = void*`, `WPARAM = uintptr_t`, `LPARAM = intptr_t`, `DCPCALL = __cdecl` on Linux. The WLX export boundary is therefore byte-compatible across platforms; the portability problem lives entirely behind it.

The tenants guiding this design:

- One shared source tree, two platform-specific files per platform, no `#ifdef` inside shared headers.
- The shared processors already use only ~5 web-engine methods. Whatever they call is the abstraction; everything else is backend setup.
- "Usable software not full parity." Behaviors that exist on Windows but are hard to implement on Linux are deferred, not paginated into Linux-only stubs.
- The Windows tree is allowed to be reworked; some features removed there too if they aid portability and are underused.

## Goals / Non-Goals

**Goals:**

- One source tree producing two artifacts: `EdgeViewer-$(Platform).dll` (Windows, MSBuild + vcpkg) and `EdgeViewer.wlx64` (Linux, CMake + system Qt 6).
- Processors, `Navigator`, and the WLX contract layer compile unchanged on both platforms.
- Zero `#ifdef` in shared headers; the only source-tree `#ifdef` lives in `WebView/WebViewFactory.cpp` (choosing the backend) plus build-system platform splits.
- A working Double Commander Lister on Linux for Markdown, AsciiDoc, RST, HTML, MHT, EML, URL, Images, the static-icon Directory view, and the generic "Other" viewer.

**Non-Goals:**

- Linux dynamic directory thumbnails (GdkPixbuf + GIO). Future change.
- Linux native shell-style right-click menu (GMenu + GAppInfo). Future change.
- Per-processor sticky zoom on Linux. Future change.
- CEF backend, WebKitGTK backend, macOS port. Not attempted. (The original Linux backend was specified against WebKitGTK; the implementation switched to Qt Web Engine mid-port because DC's Qt6 build only accepts `QWidget*` as the lister parent. See `openspec/changes/docs-linux-backend-qt/` for the rationale.)
- Re-implementing the HTML charset-override on either platform. The feature is dropped; reintroduction is future-work.
- Bit-identical visual rendering across Qt Web Engine and WebView2. Both are modern Chromium-grade engines; subtle rendering differences are acceptable.
- A 32-bit Linux build. x86_64 only for v1; Linux 32-bit is a niche on the desktop and DC now defaults to 64-bit there.

## Decisions

### Decision 1: `IWebView` abstract interface lives in shared code; concrete backends live per platform

All executable code that drives the web engine stays behind a single pure-virtual interface `IWebView` declared in `EdgeViewer/IWebView.h` (shared). The interface methods are exactly the union of what `Processors/*.cpp` and `Navigator.cpp` already call today:

```
class IWebView {
public:
    virtual ~IWebView() = default;
    virtual void NavigateToString(const std::wstring& html) = 0;
    virtual void Navigate(const std::wstring& uri) = 0;
    virtual void ExecuteScript(const std::wstring& js) = 0;
    virtual void AddScriptToExecuteOnDocumentCreated(const std::wstring& js) = 0;
    virtual void RegisterVirtualHost(const std::wstring& host, const fs::path& folder) = 0;
};
```

Two implementations:
- `EdgeViewer/WebView/WebView2Backend.{h,cpp}` (Windows only) wraps existing COM code; constructed with a child `HWND` and an `ICoreWebView2Controller*` it already owns; methods translate virtuals to existing COM calls.
- `EdgeViewer/WebView/QtWebEngineBackend.{h,cpp}` (Linux only) implements the interface above using `QWebEngineView` and `QWebEngineUrlSchemeHandler` (with a custom `ev://` URI scheme registered per-process via `QWebEngineUrlScheme::registerScheme` and `QWebEngineProfile::defaultProfile()->installUrlSchemeHandler`).

**Why over alternatives:**

- *Alternative: keep `wil::com_ptr<ICoreWebView2>` as `ViewPtr` and sprinkle `#ifdef` for Linux typedef.* Rejected: pulls COM headers into shared headers, forces processors to know which engine they are on, and makes every processor file `#ifdef`-aware in test.
- *Alternative: use the `webview/webview` single-header library for both backends.* Rejected: its tiny API lacks `RegisterVirtualHost` and resource interception. We would have to reach through to the underlying WebKit pointer and duck-paste ourselves back into the abstraction we just left.
- *Alternative: introduce a richer interface with zoom, settings, message-bridge, etc.* Rejected: scope. The 5 methods are exactly the processors' surface; everything else is backend setup, lifecycle, or future-work. Keep the interface minimal.

### Decision 2: `ProcessorInterface::mapDomains` becomes `IWebView::RegisterVirtualHost`

Today `ProcessorInterface::mapDomains` does `webView.try_query<ICoreWebView2_3>()->SetVirtualHostNameToFolderMapping(host, folder, ...)`. After the rework it becomes a single call to `IWebView::RegisterVirtualHost(host, folder)`, dispatched to the backend's platform-specific scheme mapping. The processors stop including `Globals.h` for COM reasons alone; `Globals.h` stops including `<wil/com.h>` and `<webview2.h>`.

**Alternative:** introduce a separate `IVirtualHostMapper` interface owned by the processor. Rejected: there is exactly one of these per view and the call surface is trivial; folding it into `IWebView` avoids an extra interface and matches how processors already invoke it.

### Decision 3: Virtual host URL convention — Fallback A confirmed (custom `ev://` scheme)

**Spike result (Task 1):** The primary approach (registering `http` as a custom URI scheme on Qt Web Engine) is **dead**. Qt Web Engine (Chromium) explicitly reserves the `http` and `https` schemes for actual web traffic and rejects global custom-scheme registration for them. The scheme handler never fires.

**Fallback A confirmed:** A custom scheme `ev://` (short for EdgeViewer) is registered instead, dispatching by host exactly as the primary approach intended:

- `ev://assets.example/...` → plugin's `assets/` directory
- `ev://local.example/...` → the file's root directory

The spike loaded the actual `Resources/assets/markdown/loader.html` with base URI `ev://assets.example/markdown/loader.html`, dispatched asset requests to the scheme handler, and rendered Markdown from `Examples/tutorial #1.md` via cross-origin `fetch(ev://local.example/...)`. Three additional findings from the spike:

1. **CORS required:** The loader HTML page origin is `ev://assets.example` but `fetch()` calls `ev://local.example` — cross-origin. Qt Web Engine silently rejects cross-origin `fetch()` without `Access-Control-Allow-Origin: *`. The `QtWebEngineBackend` must use `QWebEngineUrlRequestJob::setAdditionalResponseHeaders()` with a `QMultiMap` containing the CORS header on every response.
2. **URL-encoding required:** Filenames with spaces (e.g. `tutorial #1.md`) break `fetch()` unless percent-encoded. The spike mirrors the real plugin's `urlPathW()` behavior: URL-encode the filename in the loader's `__MD_FILENAME__` substitution, and URL-decode the path in the scheme callback before mapping to the filesystem.
3. **Qt Web Engine on native Wayland surfaces a Ctrl+Q quick-view embedding limitation** in DC: `QWebEngineView` creates a compositor `wl_subsurface` attached to the nearest ancestor `wl_surface` in its widget tree. DC's `TQtMainWindow.ChangeParent` (`lcl/interfaces/qt6/qtwidgets.pas:7459-7484`) preserves `Qt::Window` on the embedded viewer form, so on native Wayland the form becomes its own top-level `wl_surface` and the QWebEngineView's compositor surface attaches to that form rather than to DC's main surface. The compositor positions both independently. Workaround: run DC under XWayland (`QT_QPA_PLATFORM=xcb doublecmd`). Confirmed against `j2969719/doublecmd-plugins/wlx/qtpdfview_qt` (uses a `QPdfView`, no compositor surface, not affected). Track at github.com/doublecmd/doublecmd.

**Implementation approach for the port:** Loader HTML templates continue to use `http://assets.example/...` references. On Windows, `http://` stays (WebView2's `SetVirtualHostNameToFolderMapping`). On Linux, `http://` is rewritten to `ev://` in the loader HTML before `NavigateToString`. The `QtWebEngineBackend::RegisterVirtualHost` implementation registers the `ev` scheme once and dispatches by host inside the callback, with CORS headers on all responses.

- On Windows: WebView2's `SetVirtualHostNameToFolderMapping` dispatches by host under the `http` scheme — unchanged.
- On Linux: `QtWebEngineBackend::NavigateToString` rewrites `http://` → `ev://` in the loader HTML before passing it to `QWebEngineView::setHtml()`. `QtWebEngineBackend::Navigate` does the same for the absolute `http://local.example/...` / `http://assets.example/...` URLs that the URL / Other processors pass.

The previous primary approach (registering `http` itself) was spiked and rejected — see the spike result above. Fallback A (custom `ev://` scheme) is the confirmed approach. The original fallback alternatives considered during design:

- ~~**Primary**: register `http` as a custom scheme.~~ **Rejected by spike (Task 1):** Qt Web Engine (Chromium) reserves `http`/`https` for actual web traffic.
- **Fallback A** (confirmed): custom `ev://` scheme, rewrite `http://` to `ev://` in loader HTML at load time. The spike rendered Markdown successfully with this approach.
- ~~**Fallback B**: keep `http` custom-scheme registered but route non-plugin hosts through to the network.~~ Rejected (primary is dead, so this is moot).
- ~~**Fallback C**: use `webkit_web_view_load_alternate_html` plus base-URI tricks.~~ Replaced by Qt Web Engine's `setHtml(html, baseUri)` which serves the same purpose.

### Decision 4: Platform-specific implementation lives in sibling files, not in `#ifdef`s

Concrete file split (see also Decision 1's WebView tree):

```
EdgeViewer/
  IWebView.h               shared, no platform includes
  Navigator.{h,cpp}        shared, operates on IWebView&
  ProcessorInterface.{h,cpp} shared, takes IWebView&
  Processors/*.cpp         shared, unchanged semantically
  Globals.{h,cpp}          shared; Globals.h includes only <filesystem>, <map>, <string>;
                           no windows.h, wil, or webview2.
  Platform.h               shared abstract: GetModulePath(), ExpandEnv(),
                           GetPhysicalPath(), GetTempDirPath(), RemoveTempFiles()
  Platform_Win.cpp         Windows-only impl of Platform.h
  Platform_Linux.cpp       Linux-only impl of Platform.h (dladdr for module path,
                           XDG_RUNTIME_DIR/TMPDIR for tmp, manual env expand,
                           std::filesystem::read_symlink for physical path)
  WebView/
    WebViewFactory.cpp     ~10 LOC; Windows-only with #ifdef _WIN32 guard — picks backend
    WebView2Backend.{h,cpp}  Windows-only; ~150 LOC
    QtWebEngineBackend.{h,cpp}  Linux-only; ~200 LOC (QWebEngineView + custom URI scheme)
  EdgeLister_Win.cpp       Windows-only: RegisterClassA, WindowProc, popup menu,
                           WM_COPYDATA dispatch (unless Spike 2 retires it)
  EdgeLister_Linux.cpp     Linux-only: own QWidget container + QVBoxLayout + setFocusProxy,
                           direct Navigator calls instead of WM_COPYDATA
  DllMain.cpp              shared; imports WebViewFactory + Platform.h + Navigator;
                           exports the WLX symbols (def on Windows, GNU ld version script on Linux)
  CMakeLists.txt           Linux-only build
  EdgeViewer.def           Windows-only exports
  EdgeViewer.vcxproj       Windows-only build (unchanged shape, updated file list)
```

The only source-tree `#ifdef` is in `WebView/WebViewFactory.h` (the header) and `WebView/WebViewFactory.cpp` (chooses backend by `#ifdef _WIN32`); the Linux side never enters this file at all. No `#ifdef` in any shared header.

### Decision 5: `edgeviewer.ini` section rename `[Chromium]` → `[WebView]`; drop Chromium-only keys on both platforms

The `[Chromium]` section was Windows+Edge-specific (`Switches`, `BrowserExecutableX86Folder`, `BrowserExecutableX64Folder`, `UserDir`, `CleanupOnExit`). The renamed section is `[WebView]` on both platforms. Only `UserDir` is honored (WebView2's EBWebView folder on Windows; Qt Web Engine's `QWebEngineProfile::defaultProfile()` cache directory on Linux). The dropped keys are silently ignored (mINI ignores unknown keys silently already). This is a **BREAKING** config change for Windows users who had customized `Switches` or pinned a specific Edge binary folder.

**Alternative:** keep `[Chromium]` as legacy alias on Windows only, document migration. Rejected: doubles the config surface, adds `#ifdef` in `Globals.cpp`, and the feature being controlled (engine-specific command-line switches) is being explicitly removed for portability.

### Decision 6: `WebView2Backend.cpp` absorbs `WebView2.cpp`'s surviving code

The current `WebView2.cpp` (274 LOC) is reduced to roughly ~150 LOC after:
- dropping `OverrideEncoding` (encoding-override removal),
- dropping `AddWebResourceRequestedFilter`'s offline-mode block (Chromium `Switches`/`CleanupOnExit` removal drops the `OfflineMode` ini key use; the filter is no longer needed once `OverrideEncoding` is gone),
- folding all the COM event-handler registrations into `WebView2Backend`'s constructor.

The remaining body of `WebView2Backend.cpp` is `Navigate`, `NavigateToString`, `ExecuteScript`, `AddScriptToExecuteOnDocumentCreated`, `RegisterVirtualHost`, and the entry-point factory function. Anything not in that list (`AddAccleratorKeyHandler`, Zoom hotkey handling, `SetColorProfile`, `ParseAndPostMessage`) is **Windows-only** and stays inside `WebView2Backend.cpp` (or `EdgeLister_Win.cpp`); it is not part of the interface but still part of the Windows view setup.

### Decision 7: `EdgeLister` split per platform; `WM_COPYDATA` IPC simplified on Windows where safe (spike-gated)

`EdgeLister.cpp` currently bridges between the WLX callbacks (which DC/TC calls freely on any thread) and the WebView (which is HWND-thread-affine) using `WM_COPYDATA`. On Linux, that indirection is unnecessary because Double Commander's Qt6 build calls the exported `ListLoadNextW` / `ListSearchTextW` etc. on the main Qt thread already; we can call `Navigator::Open` directly on the `QWebEngineView` looked up in `gs_Views`.

On Windows, the current `WM_COPYDATA` pattern is preserved by default in `EdgeLister_Win.cpp`. **Spike 2** investigates whether `ListLoadNextW` etc. arrive on the WebView's HWND thread in Total Commander; if confirmed, `WM_COPYDATA` is retired on Windows too and the callbacks call `Navigator` directly. If the spike is inconclusive or refutes the assumption, `WM_COPYDATA` stays unchanged inside `EdgeLister_Win.cpp`. Either outcome is acceptable and registered as a task.

**Result (Spike 2):** TC calls WLX callbacks on different threads, so `WM_COPYDATA` is preserved unchanged. Retiring it is deferred as future-work #7 (Readme.md).

### Decision 8: vcpkg.json unchanged on Windows; Linux uses system pkg-config

- Windows: `vcpkg.json` keeps `webview2` and `wil` pinned as today; manifests and static triplets (x86-windows-static, x64-windows-static) are unchanged. The Windows build is unchanged in shape — only the `.vcxproj` file list is updated to include the new shared files and split-out platform files.
- Linux: no vcpkg. The CMake build calls `find_package(Qt6 6.4 REQUIRED COMPONENTS WebEngineWidgets Widgets)` and links against system Qt6 libraries. The static-triplet policy doesn't apply on Linux; we link shared system libraries, which is the norm for DC plugins.

This honors the project's "deps pinned in vcpkg.json" rule for Windows and replaces it on Linux with the OS package manager being the pinned dependency source (standard practice for Linux native plugins).

### Decision 9: Linux uses `QWebEngineView::setHtml` with a base URI for `NavigateToString`

`NavigateToString(html)` on WebView2 loads HTML with an opaque origin (no base URL, no host). The way the EdgeViewer loaders make absolute `http://assets.example/...` and `http://local.example/...` URLs resolve is via WebView2's per-view `SetVirtualHostNameToFolderMapping`, which works regardless of base URL.

Qt Web Engine's `QWebEngineView::setHtml(html, baseUrl)` takes an explicit base URI. We pass `ev://assets.example/loader.html` as the base URI on Linux (after rewriting `http://` to `ev://` per Decision 3), so any relative references in the loader resolve within the assets tree naturally. Absolute references to `ev://assets.example/...` or `ev://local.example/...` reach the `ev` custom-scheme handler registered globally via `QWebEngineUrlScheme::registerScheme` + `QWebEngineProfile::defaultProfile()->installUrlSchemeHandler`.

This is the second aspect Task 1's spike validates: that loaded HTML, when its base URI is an `ev://assets.example/...` URL, makes child resource requests that go through the registered scheme handler rather than being short-circuited.

### Decision 10: Removed behaviors `future-work`-tagged in `tasks.md`

Dropped on both platforms: `[HTML] DetectEncoding` ini key path and `gs_Htmls`/`OverrideEncoding` machinery in `HtmlProcessor.cpp` and `WebView2.cpp` (see spec requirement "HTML charset override unavailable on both platforms"). Roughly ~60 LOC removed.

Deferred on Linux only (Windows behavior preserved by gate inside the Win-only file):
- `DirProcessor::genDirThumbnail` GDI+/Shell path (gated by `#ifndef` inside `DirProcessor.cpp`'s helper files, or lifted into `DirProcessor_Win.cpp` if it pollutes shared headers). Task 4 picks the cleaner option.
- `EdgeLister::showPopupMenu` stays in `EdgeLister_Win.cpp` untouched.
- `AddAccleratorKeyHandler` + `WM_WEBVIEW_*` key posting stays in `WebView2Backend.cpp` and `EdgeLister_Win.cpp` untouched.

These three continue to compile on Windows but are not called on Linux because the Linux branch never enters their hosting files.

### Decision 11: The Linux build emits plugin exports via a GNU ld version script

Windows uses `EdgeViewer.def` for export control. On Linux a version script (`CMakeLists.txt` writes `EdgeViewer.version`: `{ global: ListLoadW; ListLoadNextW; ...; local: *; };`) with the 12 WLX symbols declared `extern "C"` in `DllMain.cpp` controls exports; the names match the Double Commander `wlxplugin.h` declarations verbatim.

Empirical note: do **not** combine the version script with `-fvisibility=hidden`. During the port we set `CMAKE_CXX_VISIBILITY_PRESET=hidden` expecting per-function `visibility("default")` to win; instead `nm -D` showed that GNU ld does not export hidden-visibility symbols even when named in the version script. The visibility presets were therefore removed and the version script is the sole export mechanism (verified: exactly 12 symbols in `nm -D --defined-only`).

## Risks / Trade-offs

**[Risk] Qt Web Engine's `ev` scheme registration conflicts with normal web navigation.** Loading real external URLs (the URL processor's `webView->Navigate("https://...")` path) might be intercepted by our custom-scheme handler and mishandled.
→ Mitigation: `QtWebEngineBackend::Navigate` only rewrites `http://local.example` and `http://assets.example` to `ev://`; all other URLs (`https://...`, `http://external.example/...`) pass through to Qt Web Engine's real network navigation. Verified by harness.

**[Risk] `QWebEngineView::setHtml` base URI doesn't behave like WebView2's `NavigateToString` opaque origin.** Some loaders might assume "no base URL" semantics for their relative URLs.
→ Mitigation: Task 1 spike loads the actual markdown loader.html this way. We have not committed to the design until this works.

**[Risk] `WM_COPYDATA` retirement on Windows turns out unsafe** (Total Commander calls WLX callbacks on a different thread than the lister HWND was created on, requiring the existing message-marshaling for thread-affinity).
→ Mitigation: Decision 7 is spike-gated; `WM_COPYDATA` is preserved unchanged inside `EdgeLister_Win.cpp` if the spike does not clearly confirm thread-safety of direct call. The Linux path is unaffected and proceeds regardless.

**[Risk] `Globals.h` rework breaks the Windows build in subtle ways** (e.g., files that previously got `<windows.h>` via `Globals.h` no longer have it).
→ Mitigation: This is mostly a mechanical include-fix pass; affected files (`Processors/*.cpp`, `Navigator.cpp`, `DllMain.cpp`) will be re-pointed at `<windows.h>` directly where they actually need Win32 functions. The VCXPROJ build is the verification; the change compiles on both platforms before merge.

**[Risk] `edgeviewer.ini` users on Windows who customized `[Chromium] Switches` lose functionality silently.**
→ Mitigation: BREAKING is called out in `proposal.md`. We can also print a one-time console message the first time the plugin loads with a `[Chromium]` section present. Actual fix is documenting the migration in `Readme.md` as part of the change.

**[Risk] Visual rendering divergences between Qt Web Engine and WebView2** for the same loader HTML (default body styles, form control themes, dark-mode color schemes).
→ Mitigation: Acceptable per Non-Goals. Both are Chromium/Blink-lineage engines; CSS we ship handles theming. Dark mode on Linux uses `prefers-color-scheme` media query, which Qt Web Engine respects when the host palette is dark.

**[Risk] DC passes `QWidget*` as `HWND` but not every DC version guarantees this** (older DC's HWND-treated-as-pointer behavior is a documented contract but not type-safety-grade; older DC GTK2 builds actually pass a `GtkWidget*` which cannot be embedded in a Qt widget tree).
→ Mitigation: The plugin targets DC's Qt5/Qt6 build only. Verified by `/proc/$(pgrep -x doublecmd)/environ` showing `WAYLAND_DISPLAY=wayland-0` + `GDK_BACKEND=wayland` (Qt6 build banner: "Widgetset library: Qt 6.11.1"). If a specific DC build violates this assumption the embedded WebView simply won't appear and users will report it — we don't need to defend against it ahead of time.

**[Risk — known limitation] Native-Wayland Ctrl+Q quick-view embedding** (documented in `Readme.md` §"Ctrl+Q quick-view window jumps under native Wayland" and `AGENTS.md` §"Known limitations"). `QWebEngineView`'s compositor surface attaches to the embedded form's separate `wl_surface` (created because DC's `TQtMainWindow.ChangeParent` preserves `Qt::Window` on the form); the compositor positions both surfaces independently and DC's main window appears to "jump" to match. Plugin-side mitigations attempted (return own container widget, deferred `show()`, `createWinId()` on the parent) do not resolve the underlying compositor-surface promotion. Workaround: `QT_QPA_PLATFORM=xcb doublecmd` (XWayland). Track at github.com/doublecmd/doublecmd and bugs.qt.io (Qt Wayland platform plugin).

**[Trade-off] Two backends to maintain going forward.** Adding a third WebView2-only feature in the future also requires the Qt Web Engine side or accepting platform-specific gates. The small `IWebView` interface (~5 methods) keeps the cost manageable but doesn't eliminate it.

**[Trade-off] First `ListLoadW` of a session spawns Chromium subprocesses (zygote + GPU + renderer).** `QWebEngineView` is Chromium-backed; this is the cost of supporting the loaders' JS/CSS stack (marked.js, highlight.js, asciidoctor.js, mermaid, mathjax). Subsequent loads in the same session are faster because Chromium reuses the profile's processes. `QT_WEBENGINE_DISABLE_SANDBOX=1` and `--single-process` reduce fork overhead at the cost of stability. Documented in `Readme.md` §"Process overhead".

## Migration Plan

1. Branch from `master` (already done as `port-to-double-commander-linux`).
2. Land the Windows-only refactor first (Tasks 2-7 below) on a separate PR so the Windows binary is verified unchanged in behavior before Linux work begins. This catches any regression on Windows early and keeps the `master` Windows build healthy at all times.
3. After the Windows refactor ships, add the Linux backend on the same branch (Tasks 8-13). The shared tree still builds cleanly on Windows; the Linux build is gated by CMake.
4. Update `Readme.md` with Linux build instructions and the `[Chromium]→[WebView]` migration note.
5. Archiving the change merges `openspec/changes/port-to-double-commander-linux/` into `openspec/specs/linux-runtime/spec.md`.

Rollback strategy: the change is contained in one branch; if a regression appears post-merge, the entire branch can be reverted. The shared-tree rework is also reversible by reintroducing the `wil::com_ptr<ICoreWebView2>` typedef where it was — that's a ~30 minute revert.

## Open Questions

- **Q1:** Does Double Commander guarantee that `ListLoadW` and `ListLoadNextW` are called on the Qt main thread (so the Linux side can call `Navigator` directly without `g_idle_add`)? Answered early in Task 1 spike by reading DC source; if not, the Linux side adds a `QTimer::singleShot(0, ...)` wrapper around direct calls. Confirmed by direct testing.
- **Q2:** Should the Linux build also produce a Wayland-tested binary, or is X11 sufficient for v1? Wayland mostly works (the plugin embeds and renders), but Ctrl+Q quick-view exhibits the surface-promotion limitation documented in the Risks section. `QT_QPA_PLATFORM=xcb` is the recommended configuration for now.
- **Q3:** Does the user want the `Readme.md` updated to advertise Linux support now, or held back until enough users have validated the build? Default in `tasks.md`: update now with a "Linux support is new; please report issues" note.