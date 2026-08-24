## Purpose

Defines the embedding contract for the plugin lister under Double Commander's Ctrl+Q quick view and F3 standalone window on native Wayland, and the evidence contract that gates which mitigation ships. Replaces the falsified `ChangeParent`/`Qt::Window` root-cause record with instrumented facts and makes surface-level identification of the escaping `wl_surface` a verifiable deliverable before any functional change is accepted.

Affected layer: the Linux embedding code (`EdgeViewer/EdgeLister_Linux.cpp`, `EdgeViewer/WebView/QtWebEngineBackend.cpp`). No processor under `EdgeViewer/Processors/` and nothing under `Resources/assets/` is affected - the symptom reproduces identically across Markdown, HTML, image, and directory views because it lives in the embedding layer. Platform scope is Linux x86_64 only (the port targets 64-bit Linux); there is no 32/64 divergence to specify. Windows builds are covered by a non-regression scenario below.

## ADDED Requirements

### Requirement: Escaping-surface identification precedes any functional fix

The investigation SHALL produce a recorded identification of the surface that escapes DC's panel tree on the first Ctrl+Q open of a session, before any functional mitigation from this change is accepted into the plugin source. The identification SHALL name the surface's Wayland role (`xdg_toplevel`, `wl_subsurface`, or other), the owning component (the Qt widget class or Chromium subsystem that created it), and its parent linkage, captured on the user's KDE/Wayland machine during a reproduction of the defect.

#### Scenario: Protocol trace isolates the rogue surface
- **WHEN** a first-of-session Ctrl+Q open is captured with `WAYLAND_DEBUG=1` and correlated with `QT_LOGGING_RULES="qt.qpa.wayland*=true"` widget-to-surface output
- **THEN** the trace yields exactly one newly created top-level-role surface (or promoted subsurface) attributable to the lister flow that is absent from a non-lister baseline trace, and the record names the widget or Chromium component whose creation call produced it

#### Scenario: KWin cross-check corroborates the trace
- **WHEN** KWin's `queryWindowInfo` is invoked while the stray window is visible
- **THEN** the reported PID and resource class match the surface identified in the protocol trace record, and the two observations are stored together as the evidence pack

#### Scenario: Widget-tree dump revalidates the falsification
- **WHEN** the plugin is built with `-DEDGEVIEWER_LINUX_DEBUG_LOGGING=ON` and a first Ctrl+Q open is logged
- **THEN** the dump shows no widget in the parent chain carrying the `Qt::Window` flag, reconfirming on the current DC/Qt build that the previously documented root cause does not apply

### Requirement: Evidence-selected mitigation embeds the lister on native Wayland

IF the identification shows a lever reachable from the plugin (native-widget boundary attributes on the plugin container, or transient-parent linkage of the escaped surface to DC's main window), THEN the plugin SHALL ship exactly one such mitigation behind a compile-time constant, and with it enabled on native Wayland: the first Ctrl+Q quick-view open of a session SHALL render the file content inside the quick-view panel, and Double Commander's main window SHALL NOT reposition when the quick view opens.

#### Scenario: First Ctrl+Q open stays embedded
- **WHEN** a Markdown file is opened via Ctrl+Q as the first lister open of a DC session on native Wayland, with the mitigation compiled in
- **THEN** the rendered content appears inside the quick-view panel and DC's main window remains at its prior position

#### Scenario: Subsequent opens behave identically
- **WHEN** further files are opened via Ctrl+Q after closing the first quick view
- **THEN** every open embeds like the first - no open regresses to the screen-center placement

#### Scenario: Mitigation is processor-independent
- **WHEN** the first Ctrl+Q open is repeated with an image file and with a directory
- **THEN** all embed inside the panel like Markdown does, confirming the fix lives in the embedding layer rather than any single processor path

### Requirement: F3 standalone lister is unaffected

The F3 standalone lister window SHALL continue to behave as before the mitigation on native Wayland: a genuine top-level window whose position and rendering are unchanged relative to the pre-change build.

#### Scenario: F3 open unchanged
- **WHEN** the same Markdown file is opened via F3 on the same native Wayland session
- **THEN** the standalone lister appears as a normal top-level window, renders correctly, and exhibits no placement or focus anomaly attributable to the mitigation

### Requirement: Navigation stability is preserved under any shipped state

Regardless of which branch ships (including documentation-only outcomes), WLX lifecycle operations inside an open Ctrl+Q quick view SHALL remain stable: navigating to another file via the panel (DC calls `ListLoadNextW`) re-renders in place without thread-affinity errors, and closing the quick view releases plugin resources cleanly. The implementation SHALL NOT reintroduce the cancel-then-fire race that produced `EThreadError` during the prior investigation, and SHALL NOT construct web-engine surfaces at plugin initialization against unmapped windows.

#### Scenario: In-place navigation inside an open quick view
- **WHEN** the user navigates to another file (Ctrl+Y or cursor movement) while a Ctrl+Q quick view is open
- **THEN** the new file renders inside the same embedded view with no crash and no `EThreadError`

#### Scenario: Quick view close is clean
- **WHEN** the quick view is closed (DC calls `ListCloseWindow`)
- **THEN** the plugin releases the view's resources and the panel returns to DC's control without error

### Requirement: Documentation states only instrumented facts

The public documentation SHALL describe the Ctrl+Q native-Wayland limitation using the instrumented findings: that no widget in the chain carries `Qt::Window`, which surface escapes and why (per the evidence pack), which mitigation shipped (or why none did), and the current status of the `QT_QPA_PLATFORM=xcb` workaround (still required, or demoted to fallback for compositors where the mitigation is insufficient). The falsified `TQtMainWindow.ChangeParent` narrative SHALL be removed from `Readme.md`, `AGENTS.md`, and the `EdgeLister_Linux.cpp` header comment.

#### Scenario: Reader finds the true mechanism
- **WHEN** a user reads the "Ctrl+Q quick-view window jumps under native Wayland" section of `Readme.md`
- **THEN** the stated root cause matches the evidence pack, the shipped mitigation (if any) is named, and the XWayland recommendation reflects actual post-fix status

### Requirement: Windows build remains untouched

All source changes of this change SHALL be confined to Linux-only files. The Windows Release builds SHALL succeed for both platforms with no new warnings or errors, proving no cross-platform leakage.

#### Scenario: Dual-platform release build verification
- **WHEN** Release is built for Win32 (`vcvarsall.bat x86` + `msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=Win32 /p:UseEnv=true`) and x64 (`vcvarsall.bat x64` + x64 platform)
- **THEN** both builds succeed producing `EdgeViewer-Win32.dll` and `EdgeViewer-x64.dll`, with `git diff` confirming no Windows source file (`EdgeLister_Win.cpp`, `WebView2Backend.*`, `WebViewFactory.*`, `DllMain.cpp`, `Platform_Win.cpp`, `DirProcessor_Win.cpp`, `EdgeViewer.vcxproj`, `vcpkg.json`) was modified
