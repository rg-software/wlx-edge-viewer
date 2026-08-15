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

- One source tree producing two artifacts: `EdgeViewer-$(Platform).dll` (Windows, MSBuild + vcpkg) and `EdgeViewer.wlx.so` (Linux, CMake + system WebKitGTK).
- Processors, `Navigator`, and the WLX contract layer compile unchanged on both platforms.
- Zero `#ifdef` in shared headers; the only source-tree `#ifdef` lives in `WebView/WebViewFactory.cpp` (choosing the backend) plus build-system platform splits.
- A working Double Commander Lister on Linux for Markdown, AsciiDoc, RST, HTML, MHT, EML, URL, Images, the static-icon Directory view, and the generic "Other" viewer.

**Non-Goals:**

- Linux dynamic directory thumbnails (GdkPixbuf + GIO). Future change.
- Linux native shell-style right-click menu (GMenu + GAppInfo). Future change.
- Per-processor sticky zoom on Linux. Future change.
- CEF backend, Qt WebEngine backend, macOS port. Not attempted.
- Re-implementing the HTML charset-override on either platform. The feature is dropped; reintroduction is future-work.
- Bit-identical visual rendering across WebKitGTK and WebView2. Both are modern Chromium-grade engines; subtle rendering differences are acceptable.
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
- `EdgeViewer/WebView/WebView2Backend.{h,cpp}` (Windows only) wraps existing COM code; constructed with a child `HWND` and an `ICoreWebView2Controller*` it already owns; methods translate vir tuals to existing COM calls.
- `EdgeViewer/WebView/WebKitBackend.{h,cpp}` (Linux only) implements the interface above using `WebKitWebView*` and `WebKitUserContentManager`.

**Why over alternatives:**

- *Alternative: keep `wil::com_ptr<ICoreWebView2>` as `ViewPtr` and sprinkle `#ifdef` for Linux typedef.* Rejected: pulls COM headers into shared headers, forces processors to know which engine they are on, and makes every processor file `#ifdef`-aware in test.
- *Alternative: use the `webview/webview` single-header library for both backends.* Rejected: its tiny API lacks `RegisterVirtualHost` and resource interception. We would have to reach through to the underlying WebKit pointer and duck-paste ourselves back into the abstraction we just left.
- *Alternative: introduce a richer interface with zoom, settings, message-bridge, etc.* Rejected: scope. The 5 methods are exactly the processors' surface; everything else is backend setup, lifecycle, or future-work. Keep the interface minimal.

### Decision 2: `ProcessorInterface::mapDomains` becomes `IWebView::RegisterVirtualHost`

Today `ProcessorInterface::mapDomains` does `webView.try_query<ICoreWebView2_3>()->SetVirtualHostNameToFolderMapping(host, folder, ...)`. After the rework it becomes a single call to `IWebView::RegisterVirtualHost(host, folder)`, dispatched to the backend's platform-specific scheme mapping. The processors stop including `Globals.h` for COM reasons alone; `Globals.h` stops including `<wil/com.h>` and `<webview2.h>`.

**Alternative:** introduce a separate `IVirtualHostMapper` interface owned by the processor. Rejected: there is exactly one of these per view and the call surface is trivial; folding it into `IWebView` avoids an extra interface and matches how processors already invoke it.

### Decision 3: Virtual host URL convention stays `http://assets.example/` and `http://local.example/`

The processed loaders and JS/CSS bundles use absolute URLs like `http://assets.example/highlight_js/styles/github.css`. On Linux, WebKitGTK's `webkit_web_context_register_uri_scheme` is registered for an arbitrary scheme name, so the loader request reaches our handler regardless of the literal scheme portion of the URL — we use the *host* to dispatch (`assets.example` → assets folder, `local.example` → file root).

However, `webkit_web_context_register_uri_scheme` registers by *scheme*, not by host. The cleanest portable convention is:

- On Windows: WebView2's `SetVirtualHostNameToFolderMapping` dispatches by host under the `http` scheme — unchanged.
- On Linux: register `http` scheme globally, look at the host inside `WebKitURISchemeRequest`, and dispatch by host. This keeps the loader HTML unchanged across platforms.

This is the **single riskiest assumption of the change** — registering `http` as a custom scheme on WebKitGTK may conflict with WebKit's own handling of http(s) requests when the host is *not* `assets.example` / `local.example` (e.g., the URL processor's `webView->Navigate("https://example.com")` for real web URLs, or resource sub-loads from a web page already loaded from assets.example). Task 1 spikes this explicitly. Three fallback approaches considered, all preserving loader HTML unchanged:

- **Fallback A**: register a custom scheme name (e.g. `evassets://`) AND rewrite loader HTML at load time via the existing placeholder mechanism (`__ASSETS_ROOT__` and `__LOCAL_ROOT__` placeholders), with different concrete values per platform. Adds ~10 lines per loader; adds two substitutions in `ProcessorInterface` already done for other placeholders; loaders stay shared.
- **Fallback B**: keep `http` custom-scheme registered but route requests for non-plugin hosts through to the network by returning `WEBKIT_POLICY_IGNORE` and letting default fetch handle them. Lowest-cost if it works.
- **Fallback C**: use `webkit_web_view_load_alternate_html` plus base-URI tricks instead of an actual scheme. Risks base-URI-relative resolution behavior differences.

Task 1 explores Fallback A vs the primary approach and reports either "primary works" or "Fallback A confirmed working" before any other design step proceeds.

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
    WebViewFactory.cpp     ~10 LOC; the ONLY shared file with #ifdef — picks backend
    WebView2Backend.{h,cpp}  Windows-only; ~150 LOC
    WebKitBackend.{h,cpp}   Linux-only; ~200 LOC
  EdgeLister_Win.cpp       Windows-only: RegisterClassA, WindowProc, popup menu,
                           WM_COPYDATA dispatch (unless Spike 2 retires it)
  EdgeLister_Linux.cpp     Linux-only: gtk_container_add parent, signal handlers,
                           direct Navigator calls instead of WM_COPYDATA
  DllMain.cpp              shared; imports WebViewFactory + Platform.h + Navigator;
                           exports the WLX symbols (def on Windows, visibility on Linux)
  CMakeLists.txt           Linux-only build
  EdgeViewer.def           Windows-only exports
  EdgeViewer.vcxproj       Windows-only build (unchanged shape, updated file list)
```

Two source-tree `#ifdef`s remain: `WebViewFactory.cpp` (chooses backend by `#ifdef _WIN32`) and any unavoidable guard inside `Platform_Win.cpp` headers (none expected). Both are local to single files; no shared header includes platform headers.

### Decision 5: `edgeviewer.ini` section rename `[Chromium]` → `[WebView]`; drop Chromium-only keys on both platforms

The `[Chromium]` section was Windows+Edge-specific (`Switches`, `BrowserExecutableX86Folder`, `BrowserExecutableX64Folder`, `UserDir`, `CleanupOnExit`). The renamed section is `[WebView]` on both platforms. Only `UserDir` is honored (WebView2's EBWebView folder on Windows; WebKitGTK's data directory on Linux). The dropped keys are silently ignored (mINI ignores unknown keys silently already). This is a **BREAKING** config change for Windows users who had customized `Switches` or pinned a specific Edge binary folder.

**Alternative:** keep `[Chromium]` as legacy alias on Windows only, document migration. Rejected: doubles the config surface, adds `#ifdef` in `Globals.cpp`, and the feature being controlled (engine-specific command-line switches) is being explicitly removed for portability.

### Decision 6: `WebView2Backend.cpp` absorbs `WebView2.cpp`'s surviving code

The current `WebView2.cpp` (274 LOC) is reduced to roughly ~150 LOC after:
- dropping `OverrideEncoding` (encoding-override removal),
- dropping `AddWebResourceRequestedFilter`'s offline-mode block (Chromium `Switches`/`CleanupOnExit` removal drops the `OfflineMode` ini key use; the filter is no longer needed once `OverrideEncoding` is gone),
- folding all the COM event-handler registrations into `WebView2Backend`'s constructor.

The remaining body of `WebView2Backend.cpp` is `Navigate`, `NavigateToString`, `ExecuteScript`, `AddScriptToExecuteOnDocumentCreated`, `RegisterVirtualHost`, and the entry-point factory function. Anything not in that list (`AddAccleratorKeyHandler`, Zoom hotkey handling, `SetColorProfile`, `ParseAndPostMessage`) is **Windows-only** and stays inside `WebView2Backend.cpp` (or `EdgeLister_Win.cpp`); it is not part of the interface but still part of the Windows view setup.

### Decision 7: `EdgeLister` split per platform; `WM_COPYDATA` IPC simplified on Windows where safe (spike-gated)

`EdgeLister.cpp` currently bridges between the WLX callbacks (which DC/TC calls freely on any thread) and the WebView (which is HWND-thread-affine) using `WM_COPYDATA`. On Linux, that indirection is unnecessary because Double Commander calls the exported `ListLoadNextW` / `ListSearchTextW` etc. on the main GTK thread already; we can call `Navigator::Open` directly on the `WebKitWebView*` looked up in `gs_Views`.

On Windows, the current `WM_COPYDATA` pattern is preserved by default in `EdgeLister_Win.cpp`. **Spike 2** investigates whether `ListLoadNextW` etc. arrive on the WebView's HWND thread in Total Commander; if confirmed, `WM_COPYDATA` is retired on Windows too and the callbacks call `Navigator` directly. If the spike is inconclusive or refutes the assumption, `WM_COPYDATA` stays unchanged inside `EdgeLister_Win.cpp`. Either outcome is acceptable and registered as a task.

### Decision 8: vcpkg.json unchanged on Windows; Linux uses system pkg-config

- Windows: `vcpkg.json` keeps `webview2` and `wil` pinned as today; manifests and static triplets (x86-windows-static, x64-windows-static) are unchanged. The Windows build is unchanged in shape — only the `.vcxproj` file list is updated to include the new shared files and split-out platform files.
- Linux: no vcpkg. The CMake build calls `pkg_check_modules(WEBKIT webkit2gtk-4.1)` and `pkg_check_modules(GTK gtk+-3.0)` and links against system libraries. The static-triplet policy doesn't apply on Linux; we link shared system libraries, which is the norm for DC plugins.

This honors the project's "deps pinned in vcpkg.json" rule for Windows and replaces it on Linux with the OS package manager being the pinned dependency source (standard practice for Linux native plugins).

### Decision 9: Linux uses `webkit_web_view_load_html` with a base URI for `NavigateToString`

`NavigateToString(html)` on WebView2 loads HTML with an opaque origin (no base URL, no host). The way the EdgeViewer loaders make absolute `http://assets.example/...` and `http://local.example/...` URLs resolve is via WebView2's per-view `SetVirtualHostNameToFolderMapping`, which works regardless of base URL.

WebKitGTK's `webkit_web_view_load_html(html, content_uri)` takes an explicit base URI. We pass `http://assets.example/<type>/loader.html` as the base URI on Linux, so any relative references in the loader resolve within the assets tree naturally. Absolute references to `http://assets.example/...` or `http://local.example/...` reach the `http` custom-scheme handler registered globally (Decision 3).

This is the second aspect Task 1's spike validates: that loaded HTML, when its base URI is an `http://assets.example/...` URL, makes child resource requests that go through the registered scheme handler rather than being short-circuited.

### Decision 10: Removed behaviors `future-work`-tagged in `tasks.md`

Dropped on both platforms: `[HTML] DetectEncoding` ini key path and `gs_Htmls`/`OverrideEncoding` machinery in `HtmlProcessor.cpp` and `WebView2.cpp` (see spec requirement "HTML charset override unavailable on both platforms"). Roughly ~60 LOC removed.

Deferred on Linux only (Windows behavior preserved by gate inside the Win-only file):
- `DirProcessor::genDirThumbnail` GDI+/Shell path (gated by `#ifndef` inside `DirProcessor.cpp`'s helper files, or lifted into `DirProcessor_Win.cpp` if it pollutes shared headers). Task 4 picks the cleaner option.
- `EdgeLister::showPopupMenu` stays in `EdgeLister_Win.cpp` untouched.
- `AddAccleratorKeyHandler` + `WM_WEBVIEW_*` key posting stays in `WebView2Backend.cpp` and `EdgeLister_Win.cpp` untouched.

These three continue to compile on Windows but are not called on Linux because the Linux branch never enters their hosting files.

### Decision 11: The Linux build emits plugin exports via CMake visibility, not a def file

Windows uses `EdgeViewer.def` for export control. On Linux we mark exported WLX symbols with `__attribute__((visibility("default")))` (via the CMake `CMAKE_CXX_VISIBILITY_PRESET=hidden` + an explicit `extern "C" __attribute__((visibility("default")))` per exported function, or a small `exports.h` declaring them). The exported names match the Double Commander `wlxplugin.h` declarations verbatim.

## Risks / Trade-offs

**[Risk] WebKitGTK's `http` scheme registration conflicts with normal web navigation.** Loading real external URLs (the URL processor's `webView->Navigate("https://...")` path) might be intercepted by our custom-scheme handler and mishandled.
→ Mitigation: Task 1 spike explicitly tests `Navigate` to external URLs from inside a loader. If registering `http` as a custom scheme breaks normal navigation, switch to Fallback A (custom `evassets:` scheme + per-loader placeholder substitution) — the loaders already accept placeholder substitution, so this is a minimal change.

**[Risk] `webkit_web_view_load_html` base URI doesn't behave like WebView2's `NavigateToString` opaque origin.** Some loaders might assume "no base URL" semantics for their relative URLs.
→ Mitigation: Task 1 spike loads the actual markdown loader.html this way. We have not committed to the design until this works.

**[Risk] `WM_COPYDATA` retirement on Windows turns out unsafe** (Total Commander calls WLX callbacks on a different thread than the lister HWND was created on, requiring the existing message-marshaling for thread-affinity).
→ Mitigation: Decision 7 is spike-gated; `WM_COPYDATA` is preserved unchanged inside `EdgeLister_Win.cpp` if the spike does not clearly confirm thread-safety of direct call. The Linux path is unaffected and proceeds regardless.

**[Risk] `Globals.h` rework breaks the Windows build in subtle ways** (e.g., files that previously got `<windows.h>` via `Globals.h` no longer have it).
→ Mitigation: This is mostly a mechanical include-fix pass; affected files (`Processors/*.cpp`, `Navigator.cpp`, `DllMain.cpp`) will be re-pointed at `<windows.h>` directly where they actually need Win32 functions. The VCXPROJ build is the verification; the change compiles on both platforms before merge.

**[Risk] `edgeviewer.ini` users on Windows who customized `[Chromium] Switches` lose functionality silently.**
→ Mitigation: BREAKING is called out in `proposal.md`. We can also print a one-time console message the first time the plugin loads with a `[Chromium]` section present. Actual fix is documenting the migration in `Readme.md` as part of the change.

**[Risk] Visual rendering divergences between WebKitGTK and WebView2** for the same loader HTML (default body styles, form control themes, dark-mode color schemes).
→ Mitigation: Acceptable per Non-Goals. Both are Chromium/Blink-lineage engines; CSS we ship handles theming. Dark mode on Linux follows GTK theme via `prefers-color-scheme`, which WebKitGTK respects when the GTK theme is dark.

**[Risk] DC passes `GtkWidget*` as `HWND` but not every DC version guarantees this** (older DC's HWND-treated-as-pointer behavior is a documented contract but not type-safety-grade).
→ Mitigation: Verified by the DC wiki and `wlxplugin.h` semantics. If a specific DC build violates this assumption the embedded WebView simply won't appear and users will report it — we don't need to defend against it ahead of time.

**[Trade-off] Two backends to maintain going forward.** Adding a third WebView2-only feature in the future also requires the WebKit side or accepting platform-specific gates. The small `IWebView` interface (~5 methods) keeps the cost manageable but doesn't eliminate it.

## Migration Plan

1. Branch from `master` (already done as `port-to-double-commander-linux`).
2. Land the Windows-only refactor first (Tasks 2-7 below) on a separate PR so the Windows binary is verified unchanged in behavior before Linux work begins. This catches any regression on Windows early and keeps the `master` Windows build healthy at all times.
3. After the Windows refactor ships, add the Linux backend on the same branch (Tasks 8-13). The shared tree still builds cleanly on Windows; the Linux build is gated by CMake.
4. Update `Readme.md` with Linux build instructions and the `[Chromium]→[WebView]` migration note.
5. Archiving the change merges `openspec/changes/port-to-double-commander-linux/` into `openspec/specs/linux-runtime/spec.md`.

Rollback strategy: the change is contained in one branch; if a regression appears post-merge, the entire branch can be reverted. The shared-tree rework is also reversible by reintroducing the `wil::com_ptr<ICoreWebView2>` typedef where it was — that's a ~30 minute revert.

## Open Questions

- **Q1:** Does Double Commander guarantee that `ListLoadW` and `ListLoadNextW` are called on the GTK main thread (so the Linux side can call `Navigator` directly without `g_idle_add`)? Answered early in Task 1 spike by reading DC source; if not, the Linux side adds a `g_idle_add` wrapper around direct calls.
- **Q2:** Should the Linux build also produce a Wayland-tested binary, or is X11 sufficient for v1? Wayland should work without code changes given we use `gtk_container_add` (no X11-specific reparenting), but a Wayland smoke test is included in Task 13 to confirm.
- **Q3:** Does the user want the `Readme.md` updated to advertise Linux support now, or held back until enough users have validated the build? Default in `tasks.md`: update now with a "Linux support is new; please report issues" note.