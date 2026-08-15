## Why

The plugin currently runs only on Windows inside Total Commander via WebView2. Double Commander uses the same WLX contract on Linux, but there is no Linux build and no engine that can target it. We want a workable Double Commander Lister on Linux without forking the source tree, and we are willing to simplify the Windows tree (drop underused features, encapsulate platform specifics) to get there cleanly.

## What Changes

- **New**: Linux build via CMake + `libwebkit2gtk-4.1`, producing `EdgeViewer.wlx.so` loadable by Double Commander.
- **New**: `IWebView` abstract interface in shared code; processors stop taking `wil::com_ptr<ICoreWebView2>` and stop including `<wil/com.h>`/`<webview2.h>`. Two concrete backends (`WebView2Backend` for Windows, `WebKitBackend` for Linux) live in platform-only `.cpp` files.
- **New**: Platform helpers split by file (`Platform_Win.cpp` / `Platform_Linux.cpp`), not by `#ifdef` inside shared files. `Globals.h` no longer pulls in `windows.h`, `wil`, or `webview2.h`.
- **New**: `EdgeLister` split per platform. The Linux side uses a `GtkWidget*` parent directly (no `RegisterClassA`/`WNDPROC`/`WM_COPYDATA` IPC).
- **Modified**: `edgeviewer.ini` `[Chromium]` section renamed to `[WebView]`; Chromium-specific keys (`Switches`, `BrowserExecutableX64Folder`, etc.) are dropped on both platforms. `UserDir` is retained semantically (per-engine profile directory).
- **BREAKING** (config): the `[Chromium]` section no longer exists; users with custom `Switches`/`BrowserExecutableX86Folder`/`BrowserExecutableX64Folder` keys must migrate to `[WebView]` (only `UserDir` is honored).
- **Removed** (both platforms, future-work): HTML per-file encoding override (`[HTML] DetectEncoding=1` path in `HtmlProcessor` + the `gs_Htmls` map + the `WebResourceRequested` interceptor in `WebView2.cpp`). Modern HTML self-describes charset via BOM or `<meta charset>`; both engines sniff when absent. The feature can be reintroduced as a separate change if real-world need appears.
- **Future-work (Linux only)**: native shell right-click context menu inside the Lister; dynamic directory thumbnails (GDI+ + `IShellItemImageFactory` path in `DirProcessor`); per-processor sticky zoom hot-key handling. These continue to work on Windows and are simply not implemented on Linux in this change.
- **Refactor (Windows only)**: simplify `WM_COPYDATA` IPC between WLX callbacks and the WebView, if the early spike confirms it is safe. If the spike fails, the existing mechanism is preserved unchanged inside `EdgeLister_Win.cpp`.

## Capabilities

### New Capabilities

- `linux-runtime`: the plugin builds on Linux and runs inside Double Commander. Specifies which file types render on Linux, the build artifact (`EdgeViewer.wlx.so`) and how it is packaged, the configuration layout of `[WebView]` on Linux, and the deferred (future-work) behaviors that work on Windows but are not implemented on Linux in this change.

### Modified Capabilities

- (none) — the existing `eml` spec already requires identical behavior across 32/64-bit Windows builds and its rendering is pure JS, so it carries over to Linux without spec-level changes. No other capability specs exist today.

## Impact

- **Code**:
  - New shared: `IWebView.h`, `Platform.h`.
  - New per-platform files: `Platform_Win.cpp`, `Platform_Linux.cpp`, `WebView/WebView2Backend.{h,cpp}`, `WebView/WebKitBackend.{h,cpp}`, `WebView/WebViewFactory.cpp`, `EdgeLister_Win.cpp`, `EdgeLister_Linux.cpp`, `CMakeLists.txt`.
  - Refactored shared: `Globals.{h,cpp}` (Win-only headers moved out), `ProcessorInterface.{h,cpp}` (`mapDomains` becomes a virtual on `IWebView`), all `Processors/*.cpp` (parameter type `wil::com_ptr<ICoreWebView2>` → `IWebView&`), `Navigator.{h,cpp}` (operates on `IWebView&`), `DllMain.cpp` (WLX callbacks call `WebViewFactory` + direct `Navigator` invocations instead of `WM_COPYDATA` where possible), `HtmlProcessor.cpp` (drop `gs_Htmls`/`DetectEncoding` path), `WebView2.cpp` absorbed into `WebView/WebView2Backend.cpp` minus encoding-override plumbing.
  - `EdgeViewer/EdgeLister.{h,cpp}` → split per platform.
  - `Resources/assets/*` (JS/CSS) untouched.
- **Build**: MSBuild `.vcxproj` keeps being the Windows build; new `CMakeLists.txt` is the Linux build. `vcpkg.json` unaffected (Linux use system WebKitGTK via pkg-config). The `.def` file stays Windows-only; Linux exports go through CMake visibility settings.
- **Dependencies**: Linux adds `libwebkit2gtk-4.1` (system pkg), `gtk3` (transitively), standard C++23. No vcpkg change. Windows deps unchanged.
- **Config**: `edgeviewer.ini` `[Chromium]` → `[WebView]`; dropped keys are silently ignored (mINI ignores unknown keys).
- **Systems**: Windows Total Commander behavior is preserved except for the removed HTML encoding-override feature (off by default in the shipped ini) and any `WM_COPYDATA` simplification the spike approves. Linux Double Commander gains a working Lister for Markdown, AsciiDoc, RST, HTML, MHT, EML, URL, images, directories (static icons), and the generic "Other" viewer.