## Purpose

Defines how the Linux build of the EdgeViewer plugin owns
`QtWebEngineBackend` instances over the lifetime of a Double
Commander session, so that the `ListLoadW` / `ListLoadNextW` /
`ListCloseWindow` callbacks are always satisfied with a previously-
shelled Chromium-backed view rather than spinning one up on demand.
The pool exists specifically to prevent the native-Wayland
Ctrl+Q "lister at screen center / DC main window jumps" symptom
(which is caused by Chromium acquiring its compositor surface on the
first widget-show against an unrealized parent `wl_surface`).

This capability applies to the Linux build only. The Windows build
keeps its per-lister `WebView2Backend` allocation model; this
spec applies only when `LCLQT6` is the active widgetset.

## ADDED Requirements

### Requirement: One warm spare QtWebEngineView at all times

By the time the first `ListLoadW` of a DC session returns, the plugin
SHALL already have at least one `QtWebEngineBackend` (and its
`QWebEngineView`) parented to a long-lived hidden stable widget and
shown at least once, so that the compositor's native `wl_subsurface`
has been acquired and attached against DC's main-window surface tree.
The plugin SHALL maintain this spare under all subsequent states
(open lister count zero, one, two or more). When a `ListLoadW`
claim takes the only spare, the plugin SHALL schedule an asynchronous
build of a new spare so that the next `ListLoadW` finds a fresh spare
ready.

#### Scenario: First Ctrl+Q of a fresh DC session

- **WHEN** the user has just started Double Commander, takes no lister
  actions, then activates Ctrl+Q on `Examples/foo.md`
- **THEN** the plugin serves the request from the warm spare built
  during `DllMain` initialization; the lister renders inside the
  quick-view panel; Double Commander's main window stays at its
  original coordinates; no "lister at screen center, DC main window
  jumps" symptom is observed.

#### Scenario: F3 immediately after Ctrl+Q (concurrent listers)

- **WHEN** a Ctrl+Q quick view is already open showing file A, and
  the user triggers F3 on a different file B (DC closes the quick
  view before opening F3, or DC keeps both visible — either flow is
  permitted)
- **THEN** the second `ListLoadW` finds another spare
  (the pool schedules a replacement after the first claim); the F3
  lister renders correctly without Chromium compositor-surface
  acquisition warnings.

#### Scenario: Five rapid F3 opens and closes

- **WHEN** the user opens F3, closes it, opens F3, closes it — five
  times in quick succession
- **THEN** every F3 lister renders correctly; the pool never has
  fewer than one spare ready; no warning or error is emitted on the
  stderr about running out of spare capacity.

### Requirement: Acquire transfers a view; Release returns it; no per-claim allocation

A `ListLoadW` call SHALL `Acquire` a `(Container, View, Backend)`
triplet from the pool. The Acquire transfer SHALL be cheap: the
`QWebEngineView`'s parent SHALL be re-parented to the new
`QWidget*` container (which lives under DC's `ParentWin`), the
view SHALL be `show()`-ed if it is currently hidden, and the
loaded HTML from any prior lister SHALL be cleared via
`QWebEngineView::setUrl(QUrl("about:blank"))` before
`Navigator::Open(newfile)` runs (so the user never sees stale
content from a prior lister).

A `ListCloseWindow` call SHALL `Release` the triplet: the view's
parent SHALL be re-parented back to the pool's hidden stash widget,
the view SHALL be hidden, and the loaded HTML SHALL be cleared to
`about:blank`. The plugin SHALL NOT call `QtWebEngineBackend::Close`
on Release (Close is reserved for `BrowserPool::Shutdown`); the
view's Chromium subprocess stays alive across Releases.

#### Scenario: Acquire reparents and shows, Release re-parents and hides

- **WHEN** the plugin's pool acquires a view for a new lister and
  later releases it
- **THEN** after Acquire, the view's `QWidget::parentWidget()`
  returns the new lister container and `isVisible()` returns true;
  after Release, the view's `parentWidget()` returns the pool's
  hidden stash widget and `isVisible()` returns false; the view's
  `QWebEngineView::url()` returns the same `about:blank` URL after
  Release as it did before Acquire was first called.

#### Scenario: Acquire runs Navigator::Open before return

- **WHEN** `ListLoadW(fileToLoad)` is called and Acquire happens
- **THEN** by the time the plugin returns the lister handle to DC,
  the view has finished `Navigator::Open(fileToLoad)` and the
  `QWebEngineView::url()` reflects `ev://assets.example/...` plus the
  file content (verified the same way as today — DOM `title`,
  `document.getElementById('content')`, etc. resolve via the harness).

#### Scenario: Release does not destroy the backend

- **WHEN** `ListCloseWindow` runs after Release
- **THEN** the underlying `QtWebEngineBackend` and its `QWebEngineView`
  are still alive (not destroyed), the Chromium GPU / zygote
  subprocesses remain resident, and a subsequent `Acquire` claims
  this same view (with a fresh `setUrl(about:blank)` already
  applied).

### Requirement: Concurrent listers each get their own view

When `ListLoadW` is called while a previously-opened lister is still
visible (e.g. Ctrl+Q quick view open and F3 standalone opened
without the user closing the first), the plugin SHALL hand out a
different view from the pool, not the same view. Each `listWin`
handle returned by `ListLoadW` SHALL correspond to a distinct
`QtWebEngineView` instance during the lister's lifetime. The plugin
SHALL NOT show stale content from a different lister in any active
lister.

#### Scenario: Two F3 listers visible simultaneously (different files)

- **WHEN** the user has F3 open showing file A, then triggers F3
  on file B (DC closes one and opens the other, or shows two
  simultaneously)
- **THEN** the second F3 lister shows file B's content; if it ever
  shows both views side by side (e.g. next call to F3 before the
  first window is dismissed in some DC layouts), each lister's
  rendered content matches its own file.

### Requirement: Pool lives across the plugin's lifetime; shuts down on detach

`BrowserPool::Initialize()` SHALL be called from `DllMain`
`DLL_PROCESS_ATTACH` on Linux. It SHALL build the first spare
synchronously (the first `QWebEngineBackend` plus its
`QWebEngineView` plus the hidden stash widget plus the first call
to `show()` to acquire the compositor surface). Subsequent spares
are scheduled asynchronously via `QTimer::singleShot(0, [...])`
on the Qt main thread so they never block a `ListLoadW` call.

`BrowserPool::Shutdown()` SHALL be called from `DllMain`
`DLL_PROCESS_DETACH` on Linux. It SHALL
- stop scheduling new spares;
- `QtWebEngineBackend::Close()` every cached view (so the
  Chromium subprocesses terminate cleanly);
- destroy the hidden stash widget;
- leave the pool empty so a subsequent `Initialize()` can rebuild
  it (relevant when DC loads the plugin again).

#### Scenario: Shutdown leaves no Chromium processes around

- **WHEN** `DllMain DLL_PROCESS_DETACH` is invoked while the pool has
  one spare and zero active listers
- **THEN** after `Shutdown()` returns, `ps -ef | grep -i chrom` no
  longer lists any QtWebEngineProcess subprocess owned by the
  plugin's PID, and a subsequent `Initialize()` rebuilds exactly one
  spare.

### Requirement: No reentrancy or starvation under bursty ListLoadW

When several `ListLoadW` calls arrive in quick succession (e.g.
user scripted `--no-splash` or DC fires batch loads on startup), the
pool SHALL service them all without crashing, deadlocking, or
emitting Qt Web Engine warnings about "running out of spare
capacity". Each call may briefly block on a synchronous spare build
only if no spare is currently ready (defined as: pool's spare list
is empty AND a previous async spare build has not yet completed).

#### Scenario: Burst of three ListLoadW within 100 ms

- **WHEN** `ListLoadW` is called three times within 100 ms
  (back-to-back), with no `ListCloseWindow` in between
- **THEN** all three return non-null lister handles; the first finds a
  warm spare; the second finds the spare the first claim scheduled;
  the third briefly waits (or another spare is built synchronously)
  and returns; no crash, no warning, all three listers render their
  files correctly when shown.

No file-type processor or `Resources/assets/` path is involved
in this behavior; it is purely a property of how the plugin owns
`QtWebEngineBackend` instances and hands their `QWebEngineView`s
into the lister containers.
