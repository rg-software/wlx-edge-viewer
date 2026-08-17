## 1. Instrument the widget hierarchy

- [ ] 1.1 In `EdgeViewer/EdgeLister_Linux.cpp` `EdgeLister::Create`, at the very top (after the `parent` null check), add a debug-log block via the existing `Log.h` facility that emits: `parent` pointer, `parent->metaObject()->className()`, `parent->windowFlags()` (as a raw `Qt::WindowFlags` value), `parent->geometry()`, `parent->parentWidget()` (pointer + `metaObject()->className()`, or "null"), the full `parentWidget()` chain walked up to `parent->window()` (pointer + class name at each hop), and `parent->windowHandle()` (pointer + its `setTransientParent` if non-null). Gate the block behind the plugin's existing debug-log flag so it is silent in normal use.
- [ ] 1.2 Build the Linux `.wlx64` with debug logging enabled and run DC on a native Wayland session. Capture the log output for one F3 open and one Ctrl+Q open of the same Markdown file. Confirm: under F3, `parent->parentWidget()` is null and `parent == parent->window()`; under Ctrl+Q, `parent->parentWidget()` is non-null, `parent` has `Qt::Window` set, and `parent->window()` is `parent` itself (i.e. the form is an escaped top-level, not DC's main window). This validates the heuristic in design Decision 1 before trusting it.

## 2. Implement the primary fix (strip `Qt::Window` on the Ctrl+Q path)

- [ ] 2.1 In `EdgeViewer/EdgeLister_Linux.cpp` `EdgeLister::Create`, after the debug-log block and before `new QWidget(parent)`, compute `const bool isQuickView = (parent->parentWidget() != nullptr) && (parent->windowFlags() & Qt::Window);`.
- [ ] 2.2 When `isQuickView` is true, call `parent->setWindowFlags(parent->windowFlags() & ~Qt::Window);` followed by `parent->show();` (because `setWindowFlags` hides the widget). Keep the existing container-creation logic (`new QWidget(parent)` + `QVBoxLayout` + `addWidget(view)` + `setFocusProxy` + `container->show()`) unchanged after this point.
- [ ] 2.3 When `isQuickView` is false (F3 case), make no change to the parent's flags — the existing code path runs untouched. Confirm by reading the diff that the F3 branch is byte-identical to before.
- [ ] 2.4 Update the file's top-of-file comment block (currently lines 15–36) to describe the new `Qt::Window`-stripping step and why it is gated on `isQuickView`, replacing the stale claim that returning our own container "avoids creating the QWebEngineView's compositor surface directly under DC's widget" (which the root-cause analysis showed does not actually avoid the jump).

## 3. Verify the primary fix on native Wayland

- [ ] 3.1 Build the Linux `.wlx64` and load it in DC on a native Wayland session. Open a Markdown file via Ctrl+Q. Confirm: the rendered content appears inside the quick-view panel (not at screen center), and DC's main window does NOT reposition. Capture a screen recording or before/after screenshots.
- [ ] 3.2 Repeat 3.1 with an image file (`photo.png` or similar) and a directory (to exercise the `DirProcessor`). Confirm the fix is processor-independent (it is in the embedding layer).
- [ ] 3.3 Open a file via F3 (standalone lister) on the same native Wayland session. Confirm: the standalone lister window appears and behaves exactly as before this change (genuine top-level, no flag stripping).
- [ ] 3.4 With a Ctrl+Q quick view open, have DC call `ListLoadNextW` (navigate to another file in the panel). Confirm: the new file renders inside the same panel-embedded view without the lister escaping or DC's main window jumping.
- [ ] 3.5 Close the Ctrl+Q quick view (DC calls `ListCloseWindow`). Confirm: the plugin releases the view's resources and the quick-view panel returns to DC's control without error.
- [ ] 3.6 After Ctrl+Q open, switch focus between the quick-view panel and DC's file panels, and resize the DC main window. Confirm: no crash, no re-escape of the lister (if DC re-applies `Qt::Window` and the form re-escapes, record this for the event-filter follow-up in design Decision 2).
- [ ] 3.7 Check DC's viewer form for menubar/toolbar/statusbar presence after the flag strip. If any DC widget is missing or broken, record it — this triggers the fallback in task group 4 instead of shipping the primary path.

## 4. Fallback path (only if 3.7 shows a regression)

- [ ] 4.1 If task 3.7 shows stripping `Qt::Window` breaks DC's menubar/toolbar/statusbar or any other DC widget on the viewer form, revert the `setWindowFlags` call in task 2.2 and instead implement the `setTransientParent` fallback: walk `parent->parentWidget()` up to the nearest `window()` (DC's real main window), and if both `parent->windowHandle()` and `mainWin->windowHandle()` are non-null, call `parent->windowHandle()->setTransientParent(mainWin->windowHandle());`. Do not modify the parent's flags in this path.
- [ ] 4.2 Re-run tasks 3.1–3.6 with the fallback. Confirm: the lister form may still escape the panel as a separate top-level surface, but the compositor positions it over DC's main window (not at screen center), and DC's main window does not jump to screen center. Record which compositor was used (GNOME/Mutter, KWin, wlroots-based, etc.) since transient-parent positioning is compositor-dependent.
- [ ] 4.3 Make the chosen path (primary strip vs. fallback transient) a single source-level constant or compile-time `#define` in `EdgeLister_Linux.cpp` so the build ships exactly one behavior, per the spec's determinism requirement. No runtime ini toggle.

## 5. Verify the Windows build is unaffected

- [ ] 5.1 Confirm by diff inspection that no Windows source file (`EdgeLister_Win.cpp`, `WebView2Backend.*`, `WebViewFactory.*`, `DllMain.cpp`, `Platform_Win.cpp`, `DirProcessor_Win.cpp`, `EdgeViewer.vcxproj`, `vcpkg.json`) was modified by this change. The entire change is in `EdgeLister_Linux.cpp` (and, only if the fallback is needed, `QtWebEngineBackend.cpp`), both of which are Linux-only.
- [ ] 5.2 Build Release for Win32: from a Visual Studio 2022 Developer Command Prompt, run `vcvarsall.bat x86` then `msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=Win32 /p:UseEnv=true`. Confirm the build succeeds and produces `EdgeViewer-Win32.dll` with no new warnings or errors.
- [ ] 5.3 Build Release for x64: run `vcvarsall.bat x64` then `msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=x64 /p:UseEnv=true`. Confirm the build succeeds and produces `EdgeViewer-x64.dll` with no new warnings or errors.

## 6. Update documentation

- [ ] 6.1 In `Readme.md` §"Ctrl+Q quick-view window jumps under native Wayland": rewrite the "Root cause" paragraph's last sentence ("Plugin side mitigations attempted ... do not resolve the underlying compositor-surface promotion") to reflect that stripping `Qt::Window` from the reparented viewer form (the Ctrl+Q case) DOES resolve it, and describe the heuristic and the `setTransientParent` fallback. Keep the upstream-fix recommendation (DC `TQtMainWindow.ChangeParent` / Qt Wayland) as the long-term path.
- [ ] 6.2 In `Readme.md` "Future work" table row 9: update the "Notes" column to state the mitigation is implemented (primary or fallback, whichever shipped) and point to the §"Ctrl+Q quick-view window jumps under native Wayland" subsection for details, rather than only recommending XWayland.
- [ ] 6.3 In `AGENTS.md` §"Known limitations / future work": update the "Native-Wayland Ctrl+Q quick-view surface promotion" bullet to reflect the implemented mitigation, note which path (strip vs. transient) shipped, and keep the XWayland recommendation as a fallback for users on compositors where the mitigation is insufficient.
- [ ] 6.4 If the XWayland repaint churn (task 3.6 observation) is reduced or eliminated by the primary fix, note that side benefit in `Readme.md` §"Ctrl+Q quick-view window jumps under native Wayland". If it persists unchanged, leave the existing XWayland notes as-is.
- [ ] 6.5 Run `git diff Readme.md AGENTS.md EdgeViewer/EdgeLister_Linux.cpp` and review the full diff for accuracy and tone before considering the change done.
