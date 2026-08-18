# pool-prebuilt-browsers — design

## Context

The Linux build (`LCLQT6`) currently constructs a fresh
`QtWebEngineBackend` (and its `QWebEngineView`) for every
`ListLoadW` call (`EdgeLister_Linux.cpp::Create`, line 79–86).
On native Wayland, the first widget-show of the embedded
`QWebEngineView` triggers Chromium to allocate a compositor
`wl_subsurface`. When the parent form is still unmapped (DC
calls `ListLoadW` before `FViewer.Show`), the compositor lands
on the wrong `wl_surface` and becomes an independent top-level
that the compositor positions at screen center; DC's main
window jumps to match. Three previous mitigations
(`mitigate-wayland-ctrlq-jump`, commits `8a3102c` /
`c04adb1` / `9bf2b0c`) tried to defer container-show and
`Navigator::Open` to the parent's first `QShowEvent`; none
prevented the jump and the timing variant introduced a
navigation crash (commits `0cf2c6b` / `bd1e4a7` reverted to
the pre-investigation state with the Ctrl+Q jump still present,
documented as `QT_QPA_PLATFORM=xcb` workaround).

This change does not defer the timing — it eliminates the first
time the timing happens. By keeping at least one `QWebEngineView`
resident from plugin load (and rebuilding the spare synchronously
the first time, then asynchronously on each subsequent claim),
the Chromium compositor-surface acquisition happens once, during
DC startup, when the user is not yet watching the screen. Every
subsequent `ListLoadW` reuses an existing view; navigation just
calls `QWebEngineView::setUrl(...)` on the already-attached view.

The pool shape — `N_active + 1 spare` at all times, with the
spare kept hidden on a process-wide `QWidget*` parent — matches
the user's exact framing: "pre-create one window immediately upon
plugin load, create a spare as soon as that window is claimed
and shown, so we have one additional hidden window at our
disposal at any time."

## Goals / Non-Goals

**Goals:**

- Eliminate the Ctrl+Q Wayland jump by removing the
  "first widget-show against unmapped parent" timing window.
- Preserve all existing per-lister handle / contract behavior
  (`QWidget*` returned to DC, `Navigator::Open` semantics,
  `EdgeLister::Create`/`OpenIn`/`Close` semantics).
- Make concurrent listers (F3 plus Ctrl+Q, repeated F3) work
  cleanly — each listWin gets a distinct view from the pool.
- Avoid regressions in any listers' lifecycle (Navigation, Close,
  search, print, navigation between listers).
- Stay within the Linux-only build; Windows build is untouched.

**Non-Goals:**

- Minimizing idle Chromium footprint (one spare is always
  resident; that's the chosen trade-off).
- Detecting "no spare ready, need synchronous build" without
  blocking — bursty ListLoadW may briefly wait if the queue is
  drained; that's acceptable per the spec's "bursty" requirement
  (each may briefly block, none fail).
- Pre-loading multiple spares (one is what the user described;
  the constant stays at 1).
- Folding the Backlog `DdlMain` `Linux`-only branch into a
  Windows-comparable hook point — Windows is structurally
  different (no `QObject` event loop, no Chromium subprocess).
- Adopting the `single shared_ptr<QtWebEngineBackend>` model
  unchanged from architecture — that's
  `openspec/changes/port-to-double-commander-linux/specs/linux-runtime/spec.md`
  and not being modified here.

## Decisions

### Decision 1: `BrowserPool` is module-level state — one instance per process

`BrowserPool` is a static-class with the pool, the hidden stash
widget, and the spare-build scheduling. There is exactly one
`BrowserPool` per process; `Initialize` is idempotent (a
re-initialize after `Shutdown` rebuilds the first spare).
The Linux `DllMain` calls `Initialize` from `DLL_PROCESS_ATTACH`
and `Shutdown` from `DLL_PROCESS_DETACH`. Windows does not call
these — Windows keeps the existing per-lister `WebView2Backend`
construction in `EdgeLister_Win.cpp`.

**Rationale:** the user's framing is "pre-create one window
immediately upon plugin load," which is naturally a
singleton. There is no value in a per-thread pool because all
WLX callbacks arrive on the main Qt thread per Spike 2 / Design
Decision 7.

**Alternatives considered:**

- *Per-thread pool.* Rejected: there is no per-thread use; WLX
  callbacks are sequential on one thread.
- *Lazy first-spare at first `ListLoadW`.* Rejected: the very
  first Ctrl+Q of the session would still hit the original
  timing window; pre-creating at plugin load kills that
  window.
- *One BrowserPool per caller's Qt thread affinity.* Same as
  per-process given the actual threading.

### Decision 2: First spare is built synchronously during `Initialize`

`Initialize` calls into a private synchronous `buildSpare()`
which constructs the first `QtWebEngineBackend`, its
`QWebEngineView`, and the hidden stash widget, then `show()`-s
the stash widget once to acquire its `wl_subsurface`.
Subsequent spares are scheduled asynchronously via
`QTimer::singleShot(0, [...])` on the Qt main thread so they
never block a `ListLoadW` call.

**Rationale:** making the first spare synchronous removes the
"first creation" timing class entirely — DC blocks on plugin
load for ~1–3 s while Chromium warms, then never has to wait
again. Asynchronous first spare leaves the user-visible Ctrl+Q
open as the very first creation, which is exactly the symptom
we are trying to eliminate.

**Alternatives considered:**

- *Async first spare.* Rejected (see above).
- *First spare built in `DLL_PROCESS_ATTACH` via detached
  thread.* Rejected: would require thread-safety primitives
  (mutex) just to check completion; the Qt thread has to
  construct `QWidget`s anyway.

### Decision 3: Hidden stash widget is a `QWidget` parented to the QApplication's first top-level window

The hidden stash widget is `new QWidget(<some-toplevel>)`,
constructed once during `Initialize`, set as parent of all
spare views, and `show()`-ed exactly once. When a view is
"Claim"ed, its `QWidget::setParent(listerContainer)` reparents
it; the compositor surface stays attached to the same chain
(Qt Web Engine's view keeps its compositor surface across
reparenting on Wayland, per Qt 6 documentation). When a view is
"Release"d, `setParent(stashWidget)` reparents back; the view is
hidden and `setUrl(about:blank)` clears the page.

**Rationale:** `QWidget`'s `parentWidget` propagation is the
cleanest way to move a `QWebEngineView` between containers in
Qt without re-creating it. Re-creating a `QWebEngineView` is
exactly what caused the original timing problem and we are trying
to avoid it.

**Alternatives considered:**

- *Hide the view entirely between listers (parent stays
  per-lister).* Rejected: this is exactly the per-lister
  constructor we are replacing.
- *Use a top-level popup for the stash.* Rejected: a top-level
  popup would itself be a `wl_surface` — the point is to share
  DC's main window's surface tree, not to create yet another.

### Decision 4: `EdgeLister::Create`, `OpenIn`, `Close` rewire to the pool

Current shape (`Create` line 79–86) constructs a fresh
`QtWebEngineBackend` and saves it in `gs_Views[container] =
backend`. New shape:

- `Create(parentWindow, fileToLoad, processor)` calls
  `BrowserPool::Acquire(parentWindow, fileToLoad)`, which
  returns a `(Container, View, Backend)` triplet. The container
  is the value stored in `gs_Views` (existing contract). The
  backend's `Open(fileToLoad)` is run synchronously inside the
  Acquisition. The spare is rescheduled before `Acquire` returns
  so the next call finds a fresh spare.

  `gs_Views` semantics is unchanged: each container still maps to
  its backend (the backend is shared across containers but the
  map gives `OpenIn` and `Close` access via the container handle).

- `OpenIn(listWin, fileToLoad)` looks up `gs_Views[listWin]` to
  find the shared backend (or, more cheaply, walks the container
  to find its `QWebEngineView` and calls `QWebEngineView::setUrl`
  directly via the backend shared_ptr). `listWin` is whichever
  the lister carries today — Linux-only code, no Windows
  change.

- `BrowserPool::Release(container)` is called from
  `DllMain.cpp`'s Linux `ListCloseWindow` instead of the current
  `backend->Close()` + `gs_Views.erase()`. It reparents the view
  back to the stash widget, hides it, clears the page via
  `setUrl(about:blank)`, and reschedules a build-spare if needed.

### Decision 5: `QtWebEngineBackend::Close()` keeps its current meaning

`QtWebEngineBackend::Close()` continues to set the view's
parent to nullptr and call `deleteLater`. It is called only
from `BrowserPool::Shutdown()` (which iterates the pool and
closes every cached view); it is NOT called from
`BrowserPool::Release()`. That preserves the "release = cheap
return to pool, no Destroy" property the spec specifies, while
"shutdown = full cleanup of all Chromium subprocesses".

## Risks / Trade-offs

- **[Risk] Idle memory cost.** Even when the user never opens a
  lister, the Chromium zygote + GPU + renderer subprocesses stay
  resident (~200–400 MB RSS on typical Linux desktop).
  → **Accept** — this is the trade the user explicitly chose
  ("pre-create one window immediately upon plugin load").

- **[Risk] `DLL_PROCESS_ATTACH` blocks ~1–3 s for the first
  spare.** Plugin load adds latency to DC startup.
  → **Accept** — better than the user pressing Ctrl+Q and seeing
  a jump.

- **[Risk] Burst of synchronous spare builds if the user opens
  several listers back-to-back before the async spare is ready.**
  → **Mitigation:** `Acquire` falls back to a synchronous build
  when the spare list is empty. The fallback is exercised only
  when the spare scheduler has not yet caught up; under normal
  usage the user-perceptible latency is one Acquire (a few ms
  on top of `setHtml`). Worst case is just a slower Acquire, not
  a crash (spec: §"No reentrancy or starvation under bursty
  ListLoadW").

- **[Risk] View's compositor surface could fail to re-attach on
  reparent.** Qt 6 documentation says `QWebEngineView` is
  reparentable and the compositor surface follows the parent
  wl_surface hierarchy, but this is Qt-version-specific and not
  exhaustively tested on KDE Wayland. If the surface does NOT
  follow the reparent, the F3-second-lister case or Ctrl+Q
  consecutive cases would still see the escape.
  → **Mitigation:** the spec's "Acquirereparents" requirement is
  verifiable in the harness (`harnessAcquireReparents` —
  acquire, navigate, render-check). If verification shows the
  reparent does not track the surface, this change's scope
  expands: visible state of the view has to be preserved
  across listers (no hide, no reparent) — which would require the
  pool to own the container hierarchy too, not just the view.
  This risk is acknowledged and the harness covers it.

- **[Trade-off] `Close()` on the Windows path is unchanged but
  the Linux path now has two close points** — `Close()` on
  `QtWebEngineBackend` destroys the view (called by Shutdown);
  `BrowserPool::Release()` does NOT destroy the view. The dual
  path is simple enough (`if (Shutdown) backend->Close(); else
  pool.Release(...);`).

## Migration Plan

This change replaces the per-`Create` constructor in
`EdgeLister_Linux.cpp` with a pool claim. There is no migration
of existing user data; only an internal plugin-side lifecycle
change. Users see no functional difference other than the Ctrl+Q
Wayland jump being absent. The internal pool implementation
breaks no DC-facing contract.

Rollback strategy: revert the commit. The pre-investigation
behavior (Ctrl+Q jumps on native Wayland, doc'd as the
`QT_QPA_PLATFORM=xcb` workaround) returns. Windows is never
affected in either direction.

## Open Questions

- **Q1:** Should the very first `ListLoadW` after `Initialize`
  block on a synchronous build if the spare somehow did not
  finish in time, or fall back to building inline? Current
  Decision: synchronous fallback is the simpler answer and it
  matches the spec's "bursty" requirement — answerable during
  verification.
- **Q2:** How long should `BrowserPool::Initialize` wait for the
  first Chromium subprocess to be fully up before returning? The
  spec doesn't pin a number; current intent is "synchronously
  build, then return." If Chromium is slow, DC startup is slow.
  Defer to verification + measurement.
- **Q3:** Does the user's KDE/Wayland test confirm the
  Ctrl+Q jump is gone after this change? Final verification on
  the user's machine is gating; if F3-second-lister or
  Ctrl+Q-second-lister surfaces a new jump because reparent
  doesn't track, Decision "Risk: compositor reparent" triggers
  the expanded scope.
