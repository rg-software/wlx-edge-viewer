# pool-prebuilt-browsers — CLOSED OUT (no shipping implementation)

## Outcome

**Implementation abandoned; change archived as a record of the
investigation.** The pre-built-browser pool was implemented and
reached a state where:
- the C++ compiles cleanly with clang++ 19 + Qt 6 dev packages;
- the unit-level `Acquire` flow runs through end-to-end (verified by
  instrumentation in the offscreen harness — every step of the
  reparent / layout / show / navigate sequence logged its entry and
  exit);
- the *plugin binary* (`EdgeViewer.wlx64`) loads, and `ListLoadW`
  returns a non-null lister handle (the harness's `printf("ListLoadW
  returned %p\n", listWin)` fires);
- BUT: `app.exec()` segfaults before any timer callback fires. The
  crash is reproducible against the offscreen platform-plugin harness
  and is not present when the same code is reverted to the per-`Create`
  construction (no pool, no stash widget, no spare pool — the
  pre-investigation `EdgeLister_Linux.cpp` from commit `0cf2c6b`).

The crash correlates with the BrowserPool introducing a stash
top-level widget and creating a second `QtWebEngineBackend` (the
async-spare build). Two failure modes are likely:

1. `new QWidget()` without a parent creates a top-level widget. On
   offscreen this can interact badly with Qt's event-dispatcher
   bookkeeping when the platform integration has no display server
   and the widget never has its `show()` called in a way that
   matches Qt's expectations.
2. `QtWebEngineBackend`'s constructor starts a fresh Chromium GPU
   / zygote subprocess. Two `QWebEngineView` instances on the same
   `QWebEngineProfile::defaultProfile()` may race or otherwise be
   unsupported on offscreen.

Neither is fixable from the plugin without redesign (e.g. make the
spare-view's parent something that already exists on offscreen, or
serialise spare builds). On the real KDE/Wayland desktop, either or
both may work fine — the offscreen harness isn't a faithful proxy
for it (which is itself a useful finding: the offscreen harness
should not be used to validate QtWebEngineView pool behaviour; use a
real DC session for that).

## Conclusion

**The change does not ship.** The Ctrl+Q Wayland jump on
`port-to-double-commander-linux` stays documented as the `QT_QPA_
PLATFORM=xcb doublecmd` workaround (no change). The pre-create-pool
idea is recorded here as a future-direction attempt that
implementation work could not validate end-to-end in the available
test environment; revisit as a separate dedicated change if a user
has both a real Wayland DC session and time to debug Qt's
top-level-widget + multiple-QWebEngineView lifecycle before merging.

## Records

- Implementation artefacts (kept for historical reference, do **not**
  ship to the repo as runtime code):
  - `EdgeViewer/WebView/BrowserPool.{h,cpp}` — implemented and
    verified to compile + load. Removed from `CMakeLists.txt` source
    list to avoid shipping.
  - `EdgeViewer/EdgeLister_Linux.cpp` — `Create` and `OpenIn` were
    refactored to call the pool; reverted to the per-`Create`
    construction.
  - `EdgeViewer/DllMain.cpp` — included `WebView/BrowserPool.h` for
    `Initialize`/`Shutdown` from `DLL_PROCESS_ATTACH`/`DETACH`;
    reverted to its prior form.
- Planning artefacts (kept in `openspec/changes/pool-prebuilt-browsers/`):
  proposal.md, specs/lister-window-pool/spec.md, design.md,
  tasks.md (this file).
