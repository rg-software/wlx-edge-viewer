# docs-linux-backend-qt — tasks

## 1. Update top-level docs (already done)

The bulk of the top-level doc updates has already landed in
commits `738c3d1` and `3970960`:

- [x] 1.1 `AGENTS.md` — substitute "WebKitGTK" → "Qt Web Engine" / "Qt6" / `QtWebEngineBackend` in the architecture intro, the Linux build instructions, the `[HTML] DetectEncoding` cross-platform note, and the `ev://` scheme note.
- [x] 1.2 `AGENTS.md` — add the "Native-Wayland Ctrl+Q quick-view surface promotion" entry to "Known limitations / future work", citing `TQtMainWindow.ChangeParent`, the `QWebEngineView` compositor surface, the `qtpdfview_qt` counter-example, and the `QT_QPA_PLATFORM=xcb` workaround.
- [x] 1.3 `AGENTS.md` — add the "QWebEngineView process overhead" entry documenting the first-`ListLoadW` Chromium subprocess spawn cost.
- [x] 1.4 `Readme.md` — same `WebKitGTK` → `Qt Web Engine` substitution as task 1.1.
- [x] 1.5 `Readme.md` — add the "Ctrl+Q quick-view window jumps under native Wayland" subsection to "Known Limitations" with the refined root-cause analysis (linking the `qtpdfview_qt` plugin as a counter-example).
- [x] 1.6 `Readme.md` — add the "Process overhead — each `ListLoadW` spawns Chromium subprocesses" subsection.
- [x] 1.7 `Readme.md` — update the "Future work" table with rows 9 (Ctrl+Q Wayland) and 10 (process overhead).

## 2. Update the in-progress change's proposal.md

- [ ] 2.1 Replace `libwebkit2gtk-4.1` with `Qt 6 (Qt Web Engine)` in §"What Changes", bullet 1.
- [ ] 2.2 Replace `WebKitBackend` with `QtWebEngineBackend` in §"What Changes", bullet 2.
- [ ] 2.3 Replace `GtkWidget*` with `QWidget*` in §"What Changes", bullet 4 (Linux parent type).
- [ ] 2.4 In §Removed (HTML charset override), update the cross-engine reference from "WebView2 (Windows) and WebKitGTK (Linux)" to "WebView2 (Windows) and Qt Web Engine (Linux)" and replace the "OverrideEncoding callback in WebKitBackend plus a unified IWebView::OverrideEncoding method" suggestion with a Qt Web Engine equivalent (`QWebEngineUrlRequestInterceptor` or an `IWebView::OverrideEncoding` hook that the `QtWebEngineBackend` can wire to `QWebEnginePage`).
- [ ] 2.5 In the future-work section, replace per-bullet references from WebKitGTK behavior to Qt Web Engine behavior:
  - "zoom-control" — replace "wire it to WebKitGTK's API" with "wire it to `QWebEngineView`'s zoom controls" or "skip the discrete-step table per the future-work note".
  - "accelerator-keys" — replace "WebKitGTK's and Double Commander's own focus management" with "Qt Web Engine's and Double Commander's own focus management".
  - "dark-mode" — replace the "WebKitGTK follows a dark GTK theme" sentence with a note that engine-level dark scheme is Qt Web Engine-specific (uses `prefers-color-scheme` with Qt6's palette) and remains Windows-only future-work.
- [ ] 2.6 Update §Dependencies to read `Linux adds Qt6 (Qt6Widgets, Qt6WebEngineWidgets), standard C++3. No vcpkg change. Windows deps unchanged.`

## 3. Update the in-progress change's design.md

- [ ] 3.1 In §"Cross-platform Decisions", replace `libwebkit2gtk-4.1` (decision 8) with `Qt6WebEngineWidgets` and update the prose to mention `find_package(Qt6 6.4 REQUIRED COMPONENTS WebEngineWidgets Widgets)` instead of `pkg_check_modules(WEBKIT webkit2gtk-4.1)`.
- [ ] 3.2 In §"Spike result (Task 1)", replace the WebKitGTK 2.38+ scheme-rejection rationale with the Chromium-`http`-reserved rationale and note that `QWebEngineUrlScheme` is the registration API.
- [ ] 3.3 In §"CORS required", replace `WebKitBackend` with `QtWebEngineBackend` and replace the SoupMessageHeaders reference with the `QMultiMap` of headers passed to `QWebEngineUrlRequestJob::setAdditionalResponseHeaders` (which the implementation actually does).
- [ ] 3.4 In §"WebKitGTK requires `WEBKIT_DISABLE_DMABUF_RENDERER=1`" bullet, replace with "Qt Web Engine on Wayland may need `QT_QPA_PLATFORM=xcb` (entire DC run under XWayland) or `QT_WEBENGINE_DISABLE_SANDBOX=1` for stable rendering on some setups" and note that this is a runtime environment concern, not a code issue.
- [ ] 3.5 In §"Rejected approaches", update "Primary: register `http` as a custom scheme" to note the rejection reason is Chromium reserving `http`/`https` for actual web traffic (the same root cause for both the WebKitGTK 2.38+ rejection and the Qt Web Engine case).
- [ ] 3.6 In the file-listing block (or wherever the per-file split is described), replace `EdgeLister_Linux.cpp ... gtk_container_add parent, signal handlers` with `EdgeLister_Linux.cpp ... own QWidget container + QVBoxLayout + setFocusProxy`.
- [ ] 3.7 In §"Chromium-specific keys" (the `[WebView]` section discussion), replace "WebView2's EBWebView folder on Windows; WebKitGTK's data directory on Linux" with "WebView2's EBWebView folder on Windows; Qt Web Engine's profile directory on Linux".
- [ ] 3.8 In §"Linking", update the vcpkg-vs-system-pkg-config description to read "Linux: no vcpkg. The CMake build calls `find_package(Qt6 6.4 REQUIRED COMPONENTS WebEngineWidgets Widgets)` and links against system Qt6 libraries. The static-triplet policy doesn't apply on Linux; we link shared system libraries, which is the norm for DC plugins."
- [ ] 3.9 In the Decision 3 (scheme registration) rationale, replace `webkit_web_view_load_html(html, content_uri)` with `QWebEngineView::setHtml(html, base_url)` and the `http://assets.example/<type>/loader.html` base-URI convention with `ev://assets.example/<type>/loader.html` (matching the implementation).
- [ ] 3.10 In §"[Risk] WebKitGTK's `http` scheme registration conflicts with normal web navigation", update the title and prose to refer to "Qt Web Engine's `ev` scheme" — same risk applies (custom scheme handling for `ev` vs Chromium treating `http`/`https` as web), still mitigated by the NavigateToString rewrite.
- [ ] 3.11 In §"[Risk] Visual rendering divergences", update "WebKitGTK and WebView2" → "Qt Web Engine and WebView2".
- [ ] 3.12 In §"Q2: Wayland-tested binary", update "we use `gtk_container_add` (no X11-specific reparenting)" to describe the Qt Web Engine approach (own container + QVBoxLayout) and the Ctrl+Q Wayland limitation now known.
- [ ] 3.13 Add a new "Known limitation" subsection documenting the Ctrl+Q Wayland jump root cause (`QWebEngineView`'s compositor surface attaching to the embedded QMainWindow's separate wl_surface because DC's `TQtMainWindow.ChangeParent` preserves `Qt::Window`), the `qtpdfview_qt` cross-check, the plugin-side mitigations attempted, and the `QT_QPA_PLATFORM=xcb doublecmd` workaround.

## 4. Update the in-progress change's tasks.md

- [ ] 4.1 Update §"1. Spike — confirm WebKitGTK scheme semantics before committing the design" title to "Spike — confirm Qt Web Engine URI-scheme registration before committing the design" and replace the "WebKitGTK" / `ev` references in the bullet body to match the actual spike (which confirmed Qt Web Engine also blocks `http` and led to the same `ev://` fallback).
- [ ] 4.2 Update §"4.2" to describe `EdgeLister_Linux.cpp` in terms of the actual implementation (own QWidget container + QVBoxLayout + setFocusProxy; `gs_Views` keyed by the container; `Navigator::Open` direct on the main thread, no WM_COPYDATA), not the original GTK-3-`GtkWidget*` description.
- [ ] 4.3 Update §"4.4" to describe `EdgeViewer/CMakeLists.txt` in terms of `find_package(Qt6 6.4 REQUIRED COMPONENTS WebEngineWidgets Widgets)` + `add_library(MODULE ...)` + the GNU ld version script, not `pkg_check_modules(WEBKIT ...)` + `set(CMAKE_CXX_VISIBILITY_PRESET hidden)`.
- [ ] 4.4 Update §"4.5" install-rule description to read "Configure install rules in CMake so `EdgeViewer.wlx64` is laid out next to `assets/` and `edgeviewer.ini` directly in the plugin dir (no `Resources/` wrapper), matching the Windows package layout (`BuildMakeSetup.bat` flattens `Resources\` contents beside the DLL)."
- [ ] 4.5 Update §"4.6" to read "Update `EdgeLister.cpp` so the Linux side has no `#ifdef _WIN32` branch — the Linux side constructs `QtWebEngineBackend` directly in `EdgeLister_Linux.cpp` and `WebViewFactory.cpp` is Windows-only." (The `WebView/WebViewFactory.cpp` is fully `#ifdef _WIN32`-guarded now; the Linux constructor is in `EdgeLister_Linux.cpp`.)
- [ ] 4.6 Update §"Note on scheme" to refer to "Qt Web Engine" instead of "WebKitGTK" and to reference the `ev://` rewrite path as it actually exists in `QtWebEngineBackend::NavigateToString`.
- [ ] 4.7 Update §"6. Verify Linux build and Double Commander load":
  - §6.1 — update the prerequisites from `libwebkit2gtk-4.1-dev`, `gtk3-dev` to `qt6-base-dev`, `qt6-webengine-dev` (Debian/Ubuntu) or `qt6-qtbase-devel`, `qt6-qtwebengine-devel` (Fedora/Arch).
  - §6.2 — update install path to `~/.local/share/doublecmd/plugins/edgeviewer/`.
  - §6.4 — update to "Confirm dark mode: toggle Double Commander's dark mode (or the system palette) and confirm the `CSSDark` ini values are selected for at least Markdown and AsciiDoc." (drop the GTK-3 theme reference).
  - §6.5 — update to "Confirm the directory viewer on Linux shows static `folder.png`/`file.png` icons only (no shell-thumbnail generation path triggered)."
- [ ] 4.8 Update §"7. Update developer docs":
  - §7.1 — update to mention both the Qt Web Engine port and the `QT_QPA_PLATFORM=xcb` Ctrl+Q workaround note.
  - §7.2 — update from "Linux uses CMake + system WebKitGTK (no vcpkg)" to "Linux uses CMake + system Qt6 (no vcpkg)".

## 5. Update the in-progress change's specs/linux-runtime/spec.md

- [ ] 5.1 §Requirement: Linux build artifact — replace `libwebkit2gtk-4.1` and `gtk3` with `Qt6WebEngineWidgets` and `Qt6Widgets` (via `find_package(Qt6 6.4 REQUIRED COMPONENTS WebEngineWidgets Widgets)`); replace "shipped alongside `Resources/`" with "shipped directly into the plugin dir alongside `assets/` and `edgeviewer.ini`".
- [ ] 5.2 §Scenario: Building on Linux — update the prerequisite list (Debian/Ubuntu vs Fedora/Arch package names).
- [ ] 5.3 §Requirement: Cross-platform file-type rendering — update Scenario "Markdown renders the same on both platforms" to read "Linux (Qt Web Engine)" instead of "Linux (WebKitGTK)".
- [ ] 5.4 §Requirement: Virtual host mapping for asset and local resources — replace `webkit_web_context_register_uri_scheme` with `QWebEngineUrlScheme::registerScheme` + `QWebEngineProfile::installUrlSchemeHandler` (the actual implementation). Replace "WebKitGTK scheme handler" with "Qt Web Engine scheme handler" in both Asset and Local scenarios.
- [ ] 5.5 §Requirement: WebView configuration on Linux — replace "the directory WebKitGTK uses for its profile data" with "the directory Qt Web Engine uses for its profile data" in both the requirement prose and §Scenario: Linux build honors UserDir. Replace the §Scenario: Linux build ignores Chromium-specific keys prose to refer to the Qt Web Engine side ("the plugin does not pass any switch to Qt Web Engine and rendering works normally").
- [ ] 5.6 §Requirement: HTML charset override unavailable on both platforms — update the parenthetical "(Windows WebView2 + Linux WebKitGTK)" to "(Windows WebView2 + Linux Qt Web Engine)" in both the requirement prose and the Known-limitation note.
- [ ] 5.7 §Requirement: Sticky per-processor zoom not honored on Linux — replace "WebKitGTK's built-in Ctrl+scroll / Ctrl+0 / Ctrl+plus / Ctrl+minus behavior" with "Qt Web Engine's built-in Ctrl+scroll / Ctrl+0 / Ctrl+plus / Ctrl+minus behavior".
- [ ] 5.8 §Requirement: Accelerator-key relaying not implemented on Linux — replace "WebKitGTK and Double Commander's own focus management" with "Qt Web Engine and Double Commander's own focus management".

## 6. Verify

- [ ] 6.1 `openspec validate docs-linux-backend-qt --strict` — must pass.
- [ ] 6.2 `git diff openspec/changes/port-to-double-commander-linux/` — verify the spec, proposal, design, and tasks files no longer mention `WebKitGTK` / `libwebkit2gtk-4.1` / `gtk3` / `WebKitBackend` / `GtkWidget*` (except in historical "note" callouts).
- [ ] 6.3 `git diff AGENTS.md Readme.md` — verify the Wayland Ctrl+Q note and the Chromium subprocess overhead note are both present (done in `3970960`).
- [ ] 6.4 (Optional) `cmake --build build` clean and harness end-to-end test still passes (no code changes, so build should be a no-op).

## 8. Archive

- [ ] 8.1 `openspec archive docs-linux-backend-qt` once §6 verification passes.
- [ ] 8.2 Commit the in-place edits to `port-to-double-commander-linux/specs/linux-runtime/spec.md`, `proposal.md`, `design.md`, `tasks.md` (one commit titled "docs: reflect Qt Web Engine binding in linux-runtime change").
- [ ] 8.3 `openspec validate port-to-double-commander-linux --strict` — verify the in-progress change's artifacts still validate after the edits.
- [ ] 8.4 (Later, out of scope of this change) archive `port-to-double-commander-linux` once its `tasks.md` is fully satisfied.