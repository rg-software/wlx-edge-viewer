## Purpose

Defines the observable embedding behavior of the EdgeViewer lister window when Double Commander opens a file via Ctrl+Q (quick view) on a native Wayland session: the lister surface stays inside the quick-view panel and DC's main window does not reposition, achieved by the plugin stripping the `Qt::Window` flag from the reparented viewer form before creating its container widget.

## ADDED Requirements

### Requirement: Quick-view lister embeds inside the panel on native Wayland

When Double Commander invokes `ListLoadW` on a native Wayland session (Qt platform plugin `wayland`) to open a file via Ctrl+Q (quick view), the parent widget Double Commander supplies is a `QMainWindow` that has been reparented into the quick-view panel but retains the `Qt::Window` flag (per DC's `TQtMainWindow.ChangeParent`). The plugin SHALL detect this case — parent widget has both a non-null `parentWidget()` AND the `Qt::Window` flag set — and SHALL strip `Qt::Window` from that parent widget before creating its container widget, then re-show the parent (because `setWindowFlags` hides the widget). After stripping, the parent form becomes a regular child widget sharing the quick-view panel's `wl_surface` instead of becoming its own top-level `wl_surface`, so the `QWebEngineView`'s Chromium compositor subsurface attaches to the panel's surface and no independently-positionable top-level surface is created for the compositor to misplace. The rendered document SHALL appear inside the quick-view panel, not at screen center, and Double Commander's main window SHALL NOT reposition to follow the lister.

This requirement applies only to the Linux build (`EdgeViewer.wlx64`) on native Wayland. The Windows build (32- and 64-bit) is unaffected because it does not enter this code path. The Linux build ships only as 64-bit (`x86_64`) per the `linux-runtime` capability, so no 32-bit Linux behavior is defined here.

No file-type processor or `Resources/assets/` path is involved in this behavior; it is purely a property of the lister window's embedding into the host's quick-view panel, independent of which processor renders the file.

#### Scenario: Ctrl+Q opens a Markdown file on native Wayland

- **WHEN** Double Commander opens `readme.md` via Ctrl+Q (quick view) on a native Wayland session and the parent widget DC passes to `ListLoadW` has a non-null `parentWidget()` and the `Qt::Window` flag
- **THEN** the plugin strips `Qt::Window` from the parent widget, re-shows it, creates its container as a child of the parent widget, and the rendered Markdown appears inside the quick-view panel with DC's main window remaining in its original position

#### Scenario: Ctrl+Q opens an image file on native Wayland

- **WHEN** Double Commander opens `photo.png` via Ctrl+Q on a native Wayland session
- **THEN** the image renders inside the quick-view panel and DC's main window does not jump, identical to the Markdown case, because the fix is in the embedding layer and independent of the processor

#### Scenario: Ctrl+Q on XWayland remains functional

- **WHEN** Double Commander runs under XWayland (`QT_QPA_PLATFORM=xcb`) and the user opens a file via Ctrl+Q
- **THEN** the lister embeds and renders inside the quick-view panel as before; the `Qt::Window`-stripping path is either a no-op (the heuristic may also match under XWayland, where stripping is harmless because X11 child-window geometry already keeps the form inside its parent) or skipped, and in neither case does the fix introduce a regression

### Requirement: Standalone lister (F3) is unaffected on native Wayland

When Double Commander invokes `ListLoadW` to open a file via F3 (standalone lister window) on a native Wayland session, the parent widget DC supplies is a genuine top-level `QMainWindow` with no `parentWidget()`. The plugin SHALL NOT strip `Qt::Window` in this case, because the form is a legitimate top-level window and removing the flag would break its top-level behavior. The standalone lister SHALL continue to render and behave exactly as before this change.

#### Scenario: F3 standalone lister on native Wayland

- **WHEN** Double Commander opens a file via F3 on a native Wayland session and the parent widget has no `parentWidget()`
- **THEN** the plugin leaves the parent widget's window flags untouched, creates its container as a child, and the standalone lister window renders and positions exactly as before this change

### Requirement: Lifecycle operations work after the embedding fix on native Wayland

After the plugin strips `Qt::Window` from the reparented viewer form on the Ctrl+Q path, all subsequent WLX lifecycle operations on that lister instance SHALL behave correctly: navigating to another file via `ListLoadNextW` SHALL re-render the new file inside the same panel-embedded view; closing the quick-view via `ListCloseWindow` SHALL release the view's resources and leave the quick-view panel in a state DC can reuse; and focus transitions between the quick-view panel and DC's panels SHALL not crash or leave the rendered content in an inconsistent state.

#### Scenario: Navigate to another file via Ctrl+Q quick view

- **WHEN** a Markdown file is open in a Ctrl+Q quick view on native Wayland and Double Commander calls `ListLoadNextW` with another Markdown file
- **THEN** the new file renders inside the same panel-embedded view without the lister escaping the panel or DC's main window repositioning

#### Scenario: Close the quick view on native Wayland

- **WHEN** a file is open in a Ctrl+Q quick view on native Wayland and Double Commander closes the quick view (calling `ListCloseWindow`)
- **THEN** the plugin releases the view's resources and the quick-view panel returns to DC's control without error

### Requirement: Fallback when stripping Qt::Window regresses DC widget behavior

If verification on a real Wayland session finds that stripping `Qt::Window` from DC's viewer form breaks DC's menubar/toolbar/statusbar handling on that form (or any other DC-internal behavior), the plugin SHALL provide a fallback path that does not modify the parent widget's window flags: instead it SHALL set the parent form's `QWindow::setTransientParent` to DC's real main window's `windowHandle()` (located by walking the `parentWidget()` chain to the nearest `window()`). This fallback is a partial improvement — the compositor will position the escaped form relative to DC's main window (typically centered over it) rather than at screen center — and SHALL be selected only when the primary `Qt::Window`-stripping path is demonstrably unsafe. The chosen path (primary or fallback) SHALL be deterministic per build, not a runtime toggle, to keep the behavior predictable for users and for upstream bug reports.

#### Scenario: Primary path breaks DC menubar and fallback is selected

- **WHEN** verification shows stripping `Qt::Window` removes DC's viewer-form menubar or toolbar on the quick-view panel
- **THEN** the plugin uses the `setTransientParent` fallback: the lister form still escapes the panel as a separate top-level surface, but the compositor positions it over DC's main window instead of at screen center, and DC's main window does not jump to screen center

#### Scenario: Primary path is safe and is selected

- **WHEN** verification shows stripping `Qt::Window` does not regress any DC widget behavior on the quick-view panel
- **THEN** the plugin uses the primary `Qt::Window`-stripping path and the fallback is not exercised
