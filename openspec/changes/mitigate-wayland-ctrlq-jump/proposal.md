## Why

On native Wayland, opening a file via Ctrl+Q (Double Commander's quick view) makes the plugin's lister window escape the quick-view panel and float as an independent top-level surface near the screen center, and DC's main window jumps to follow it. F3 (standalone lister) is unaffected. The documented root cause (`Readme.md` §"Ctrl+Q quick-view window jumps under native Wayland") is that DC's `TQtMainWindow.ChangeParent` preserves `Qt::Window` on the viewer form it parents into the quick-view panel, so on Wayland that form becomes its own `wl_surface`; `QWebEngineView`'s Chromium compositor subsurface then attaches to the escaped form's surface instead of DC's main surface, and the compositor positions both independently. The current documentation concludes this is "not fixable from the plugin," but that conclusion was drawn after testing only three narrow mitigations (own container, deferred `show()`, `createWinId()` on parent) — none of which touch the `Qt::Window` flag on the parent DC hands us. That flag is the untried lever, and the Wayland `xdg-shell` protocol's ban on absolute client positioning of top-level surfaces means the only viable plugin-side fix is to eliminate the escaped surface rather than to reposition it.

## What Changes

- In `EdgeLister::Create` (`EdgeViewer/EdgeLister_Linux.cpp`), detect the Ctrl+Q case heuristically: the parent widget DC passes has both a `parentWidget()` (it was reparented into the quick-view panel) AND the `Qt::Window` flag (DC's `TQtMainWindow.ChangeParent` kept it). The F3 case has no `parentWidget()` (genuine top-level) and is left untouched.
- For the Ctrl+Q case, strip `Qt::Window` from the parent widget before creating the container, then re-`show()` it (`setWindowFlags` hides the widget). This makes the form a regular child widget sharing the quick-view panel's `wl_surface` instead of getting its own, so the `QWebEngineView`'s compositor subsurface attaches to the panel's surface and no independent top-level exists for the compositor to reposition.
- Add debug logging at `EdgeLister::Create` entry that dumps the parent's `windowFlags()`, `geometry()`, the `parentWidget()` chain, and `windowHandle()` state, gated by an existing log flag / build mode, so the heuristic can be verified on a real Wayland session before trusting it.
- Provide a fallback path (set `QWindow::setTransientParent` to DC's real main window's `windowHandle`) if stripping `Qt::Window` is found to break DC's menubar/toolbar/statusbar handling on the viewer form. The fallback is a partial improvement (compositor centers the form over DC's main window instead of at screen center) and is selected only if the primary path regresses.
- **No Windows changes.** The Linux-only code path (`EdgeLister_Linux.cpp`) is `#ifndef _WIN32`-scoped; the Windows build is byte-identical.
- Update `Readme.md` §"Ctrl+Q quick-view window jumps under native Wayland" and `AGENTS.md` §"Known limitations / future work" to reflect the mitigation: downgrade from "not fixable from the plugin" to "mitigated by stripping `Qt::Window` on the reparented form (Ctrl+Q case), with `setTransientParent` fallback," and note any residual caveats discovered during verification.
- Update `Readme.md` "Future work" table row 9 accordingly.

## Capabilities

### New Capabilities
- `wayland-quickview-embedding`: Observable behavior of the lister window when embedded into Double Commander's quick-view panel (Ctrl+Q) on native Wayland — specifically that the lister surface stays inside the panel and DC's main window does not jump, achieved by stripping `Qt::Window` from the reparented viewer form before creating the plugin container.

### Modified Capabilities
<!-- None. The jumping behavior was a documented known limitation, not a spec'd
     requirement, so there is no existing requirement to modify. The
     linux-runtime capability introduced by the in-progress
     port-to-double-commander-linux change is not yet in openspec/specs/, so it
     cannot be referenced as a modified capability here. -->

## Impact

- **Affected code**: `EdgeViewer/EdgeLister_Linux.cpp` (primary change in `EdgeLister::Create`); possibly `EdgeViewer/WebView/QtWebEngineBackend.cpp` if the fallback needs a hook to reach DC's main `windowHandle`. No Windows source files touched.
- **Affected docs**: `Readme.md` (§"Ctrl+Q quick-view window jumps under native Wayland", "Future work" table row 9), `AGENTS.md` (§"Known limitations / future work" — the native-Wayland Ctrl+Q entry).
- **No new dependencies.** Pure Qt6 widget API (`QWidget::setWindowFlags`, `QWidget::show`, `QWidget::windowHandle`, `QWindow::setTransientParent`). No vcpkg.json change; no CMakeLists change.
- **Risks**: (a) `setWindowFlags` hides and re-creates the platform window — brief flicker on first Ctrl+Q; (b) DC may re-apply `Qt::Window` on later events (focus/resize), requiring an event filter to re-strip (fragile, to be evaluated during verification); (c) DC's `QMainWindow` menubar/toolbar/statusbar may rely on `Qt::Window` — the viewer form typically has no menubar in quick-view mode, but this must be confirmed; (d) the XWayland repaint churn the user reported may or may not share this root cause — if it does, the primary fix likely reduces it; if it is DC's LCL geometry-sync reacting to reparenting independent of `Qt::Window`, the churn may persist.
- **Verification**: build Release for Win32 and x64 (confirm no Windows breakage from a Linux-only `#ifndef _WIN32` change); on a Linux native-Wayland session, smoke-test Ctrl+Q with several file types (Markdown, HTML, image, directory), F3 standalone, `ListLoadNextW` file switching, and quick-view close — confirm no jump, content renders inside the panel, DC main window stays put.
