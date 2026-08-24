## Context

The Linux backend (`EdgeLister_Linux.cpp`, `QtWebEngineBackend.cpp`) already creates its own `QWidget` container parented to DC's viewer-form widget and embeds the `QWebEngineView` into that container via a `QVBoxLayout`. That approach was modeled on `j2969719/doublecmd-plugins/wlx/qtpdfview_qt` and was an attempt to keep the plugin's geometry under plugin control, but it did not fix the Ctrl+Q Wayland jump because it never touched the `Qt::Window` flag on the form DC hands us. See `proposal.md` - Why for the symptom and root-cause analysis; this design covers only the "how."

The relevant widget hierarchy under Ctrl+Q on native Wayland is:

```
DC main window (QMainWindow, true top-level, Qt::Window)
└─ quick-view panel (QWidget, child)
   └─ viewer form (QMainWindow, reparented here, STILL Qt::Window)  ← ParentWin
      └─ our container (QWidget, child)  ← what plugin creates & returns
         └─ QWebEngineView (child; creates wl_subsurface)
```

The escape is `ParentWin` itself becoming a separate `wl_surface` because it retains `Qt::Window` while having a parent widget. Our container is a child of the escaped form, so it rides on the wrong surface. The untried lever is `Qt::Window` on `ParentWin`.

The Wayland `xdg-shell` protocol forbids clients from absolutely positioning top-level surfaces — the compositor decides. Therefore "just `move()` the form to the right coordinates" is not a viable plugin-side fix on native Wayland. The viable fix is to eliminate the escaped surface so there is nothing for the compositor to reposition.

All changes are in C++ (`EdgeViewer/EdgeLister_Linux.cpp`, and only if the fallback is needed, `EdgeViewer/WebView/QtWebEngineBackend.cpp`). No `Resources/assets/` static-asset changes. No new dependencies — the fix uses only Qt6 `QWidget`/`QWindow` API already linked via `find_package(Qt6 ... COMPONENTS WebEngineWidgets Widgets)`. **`vcpkg.json` is not touched** (it is Windows-only; this change is Linux-only and uses system Qt6 via CMake/pkg-config).

## Goals / Non-Goals

**Goals:**
- Eliminate the Ctrl+Q quick-view window escape and DC main-window jump on native Wayland by stripping `Qt::Window` from the reparented viewer form before creating the plugin container.
- Keep F3 (standalone lister) behavior byte-identical to today.
- Keep the Windows build byte-identical (the change is `#ifndef _WIN32`-scoped to the Linux-only source file).
- Provide a documented fallback (`setTransientParent`) that is selected only if the primary path regresses DC widget internals, so the change is not blocked on a single unverified assumption.
- Make the Ctrl+Q-vs-F3 detection reliable and cheap (no event loops, no heuristics that depend on widget captions or object names).

**Non-Goals:**
- Fixing the XWayland repaint churn the user reported, if it turns out to be a separate root cause (DC's LCL geometry-sync reacting to reparenting independent of `Qt::Window`). If the primary fix happens to reduce it, that is a bonus, not a goal.
- Pursuing the upstream fixes (DC's `TQtMainWindow.ChangeParent` clearing `Qt::Window`, or Qt Wayland adding `Qt::WA_DontCreateSeparateWlSurface`). Those remain the recommended long-term fixes and are tracked at github.com/doublecmd/doublecmd; this change is a plugin-side mitigation until they land.
- Re-introducing any of the deferred features in `Readme.md` "Future work" table rows 1–8, 10.
- Adding a runtime ini toggle for the mitigation. The spec requires the chosen path to be deterministic per build, not a runtime toggle, to keep behavior predictable for upstream bug reports.

## Decisions

### Decision 1: Detect Ctrl+Q by `parentWidget() != nullptr && (flags & Qt::Window)`

**Choice:** In `EdgeLister::Create` (`EdgeLister_Linux.cpp`), compute `bool isQuickView = (parent->parentWidget() != nullptr) && (parent->windowFlags() & Qt::Window);`. Strip `Qt::Window` only when `isQuickView` is true.

**Rationale:** F3 gives the plugin a genuine top-level `QMainWindow` — no `parentWidget()`. Ctrl+Q gives a form that DC reparented into the quick-view panel but on which `TQtMainWindow.ChangeParent` kept `Qt::Window`. The conjunction of "has a parent widget" and "is still a window" is exactly the escaped-form condition. It does not depend on widget captions, object names, or DC internals beyond the flag that the root-cause analysis already identified.

**Alternatives considered:**
- *Walk to `parent->window()` and compare to `parent`.* Under Ctrl+Q, `parent->window()` would return the escaped form itself (because `Qt::Window` makes it a window), not DC's main window, so `parent == parent->window()` would also be true under F3. Not discriminating.
- *Check `qApp->platformName() == "wayland"`.* Tempting, but the fix is harmless under XWayland (X11 child-window geometry already keeps a reparented `Qt::Window` form inside its parent's window tree), so gating on platform name would withhold a benign fix from XWayland users and add a platform-name dependency we do not need.
- *Inspect `parent->windowHandle()->type()`.* Requires `windowHandle()` to be non-null, which it may not be before `show()`. The flag check is cheaper and sufficient.

### Decision 2: Strip `Qt::Window` then re-`show()` the parent

**Choice:** For the Ctrl+Q case, before creating the container:
```cpp
parent->setWindowFlags(parent->windowFlags() & ~Qt::Window);
parent->show();  // setWindowFlags hides the widget; re-show
```

**Rationale:** `QWidget::setWindowFlags` documents that the widget is hidden and its platform window is re-created when flags change. Re-showing restores visibility. After this, the form is a regular child widget: on Wayland it shares its parent's `wl_surface` (no new top-level surface is created), and the `QWebEngineView`'s compositor subsurface attaches to the panel's surface. This is the minimal change that directly removes the root cause.

**Alternatives considered:**
- *Defer the flag change with a `QTimer::singleShot(0, ...)`.* Avoids re-creating the platform window synchronously inside `ListLoadW`, but introduces an async window during which DC could resize or focus the form while it still has `Qt::Window`. Synchronous is simpler and matches the "fix it before we build our widget on top" ordering.
- *Install an event filter that re-strips `Qt::Window` if DC re-applies it later.* Not in the initial implementation. If verification shows DC re-applying the flag on focus/resize, a follow-up task adds the filter; the design does not preclude it.

### Decision 3: Fallback via `QWindow::setTransientParent`, selected at build time

**Choice:** If verification on a real Wayland session shows stripping `Qt::Window` breaks DC's menubar/toolbar/statusbar on the viewer form, implement a fallback that does not modify the parent's flags:
```cpp
if (auto* gp = parent->parentWidget()) {
    if (auto* mainWin = gp->window()) {
        if (parent->windowHandle() && mainWin->windowHandle())
            parent->windowHandle()->setTransientParent(mainWin->windowHandle());
    }
}
```
The chosen path is fixed per build (a compile-time `#define` or a single source-level constant), not a runtime ini key, per the spec's determinism requirement.

**Rationale:** `setTransientParent` tells the compositor "this surface is a child of that one" for positioning purposes. The compositor will then typically center the escaped form over DC's main window instead of at screen center — a partial improvement that avoids the worst of the jump without touching DC's widget flags. It is compositor-dependent (not all compositors honor transient positioning identically), which is why it is the fallback, not the primary.

**Alternatives considered:**
- *Runtime ini toggle `[Wayland] FixMode=strip|transient`.* Rejected: the spec requires deterministic per-build behavior so that upstream bug reports describe one behavior, not a user-configurable matrix.
- *Always do both (strip AND set transient parent).* Pointless if stripping works (there is no separate surface to be transient). If stripping works, `setTransientParent` is a no-op on a child widget; if stripping fails, we are in fallback territory and the flag change is what failed. Combining them adds complexity without benefit.

### Decision 4: Debug logging first, gated by existing log facility

**Choice:** Before implementing the flag strip, add a debug-log block at the top of `EdgeLister::Create` that dumps `parent->windowFlags()`, `parent->geometry()`, `parent->parentWidget()` (pointer + its `metaObject()->className()`), the `parentWidget()` chain up to `window()`, and `parent->windowHandle()` (and its `setTransientParent` if any). Gate it behind the existing `Log.h` facility so it is emitted only when the plugin's debug logging is enabled.

**Rationale:** The widget-hierarchy model in the Readme is inferred, not logged. Confirming it with real output on the user's Linux machine before trusting the heuristic is cheap insurance and gives us the evidence to write a correct upstream bug report (the under-explained "main window jumps" leg from the earlier analysis). Reusing `Log.h` keeps logging consistent with the rest of the plugin and avoids a new debug knob.

**Alternatives considered:**
- *`qDebug()` directly.* Bypasses the plugin's log routing; harder to correlate with the rest of the plugin's output.
- *Skip logging, go straight to the fix.* Faster but blind; if the heuristic mismatches on the user's DC build, we would ship a broken fix and have to debug it remotely.

## Risks / Trade-offs

- **[Risk] `setWindowFlags` re-creates the platform window and briefly hides the form → flicker on first Ctrl+Q.** → Mitigation: the re-show is synchronous and immediate; the flicker is one frame at most and only on the open path, not on `ListLoadNextW`. Acceptable; document in `Readme.md`.
- **[Risk] DC re-applies `Qt::Window` on a later event (focus/resize), re-escaping the form.** → Mitigation: the initial implementation does not handle this. Verification step explicitly checks for it by switching focus and resizing the panel after Ctrl+Q. If observed, a follow-up task installs an event filter that re-strips the flag; the design permits this extension without rework.
- **[Risk] DC's `QMainWindow` menubar/toolbar/statusbar rely on `Qt::Window` and disappear after stripping.** → Mitigation: the viewer form in quick-view mode typically has no menubar, but this is unverified. The fallback (Decision 3) exists precisely for this case and is selected at build time if verification shows the regression. The spec's fallback requirement makes this a guarded bet, not a blind one.
- **[Risk] The heuristic matches a non-quick-view case and strips `Qt::Window` on a form that genuinely needs it.** → Mitigation: the conjunction `parentWidget() != nullptr && (flags & Qt::Window)` is exactly the DC bug condition; a genuine top-level (F3) has no `parentWidget()`. The only way the heuristic misfires is if DC parents a real top-level into another window while intending it to stay top-level, which is the same bug by another name.
- **[Risk] The XWayland repaint churn persists because it is a separate root cause.** → Mitigation: out of scope (Non-Goal). If it persists, it stays documented as today; if it disappears, we update the docs to note the side benefit.
- **[Trade-off] Modifying a host widget's window flags from a plugin is mildly invasive** and is the kind of thing that makes upstream maintainers frown. → Mitigation: the change is scoped to the Linux-only source file, gated by a precise heuristic, and accompanied by an upstream bug report at github.com/doublecmd/doublecmd so the real fix lands in DC and this mitigation can eventually be removed.

## Migration Plan

This is a plugin-side mitigation with no data migration and no config change. Deployment is: build the Linux `.wlx64`, replace the installed plugin, restart DC. Rollback is: revert to the previous `.wlx64`. No `edgeviewer.ini` keys are added, removed, or renamed. No Windows artifact changes.

If the primary path ships and a later DC version re-introduces a regression because DC changed how `TQtMainWindow.ChangeParent` handles `Qt::Window`, the fallback (Decision 3) is already designed and can be switched to in a single-source-line change, with no spec impact (the spec covers both paths).

## Open Questions

- **Does DC re-apply `Qt::Window` after we strip it?** Deferrable: answerable during the Linux verification step without changing the specs, the approach, or the task breakdown. If "yes," a follow-up task adds an event filter; the design already permits it.
- **Does the XWayland repaint churn share this root cause?** Deferrable: observable during verification; the answer changes only documentation prose, not the code approach.
- **Which compositors honor `setTransientParent` positioning for the fallback?** Deferrable: only relevant if the fallback is selected, and the fallback's behavior is already specified as "partial improvement, compositor-dependent." No spec or task change hinges on the per-compositor answer.
