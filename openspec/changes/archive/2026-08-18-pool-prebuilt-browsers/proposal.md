# pool-prebuilt-browsers

## Why

On native Wayland, opening a file via Ctrl+Q (Double Commander's
quick view) makes the plugin's `QWebEngineView` float as an
independent `wl_surface` near the screen center and DC's main window
"jumps" to match it. F3 (standalone lister) is unaffected. The root
cause is that `QWebEngineView`'s Chromium compositor surface is
acquired on the first widget-show against an unrealized parent
`wl_surface` (DC has not yet called `FViewer.Show` when
`ListLoadW` runs); empirically "only the first creation of a
window is problematic" — subsequent opens, on the same DC session,
reuse the established surface tree and embed cleanly. Three plugin-side
mitigation attempts (strip `Qt::Window`, defer `Navigator::Open`,
defer `container->show()` together) did not solve the jump because
the compositor-surface acquisition happens inside Qt Web Engine and
is upstream of anything reachable from a plugin.

This change dissolves the timing problem at its source by
**pre-creating** the `QWebEngineView` once at plugin load, so its
compositor surface is acquired before the user ever issues a
`ListLoadW`. A small pool of pre-built invisible views is kept on
hand so that even when several listers are simultaneously open
(F3 + Ctrl+Q, or repeated F3 opens without closing the previous
one), the next `ListLoadW` does not create a fresh
`QtWebEngineBackend` and re-trigger the timing class. The
`QT_QPA_PLATFORM=xcb doublecmd` workaround documented in
`Readme.md` (Ctrl+Q quick-view jumps under native Wayland) is no
longer required for the Ctrl+Q path; F3 was already unaffected.

## What Changes

- New module-level ownership of the `QtWebEngineBackend` /
  `QWebEngineView` instances: the lifetime moves from
  per-`Create`-construction to a process-wide pool managed by
  `EdgeViewer/WebView/BrowserPool.{h,cpp}` (new file).
- New behavior contract: every `ListLoadW` call now *claims* a
  pre-built view from the pool (reparent + show it in the lister's
  container, fire `Navigator::Open(file)`); every `ListCloseWindow`
  call now *returns* the view to the pool (hide, reparent back to the
  pool's stash widget, free the page). The pool always keeps one
  hot spare available: build the first spare at plugin load, and each
  time a spare is *claimed* by `ListLoadW`, schedule an async build
  of a new spare.
- Behavior change for end users: native-Wayland DC users no longer
  see the Ctrl+Q "lister at screen center / DC main window jumps"
  symptom on **any** Ctrl+Q open (including the first one in a
  session). F3, multi-lister (Ctrl+Q while F3 is open, or vice
  versa), navigation inside a lister, and dark mode all work
  unchanged. On Linux, the plugin's behavior on X11 / XWayland is
  unaffected (was already working).
- Implementation shape:

  - `BrowserPool::Initialize()` (called from `DllMain`
    `DLL_PROCESS_ATTACH` for the Linux build) constructs the first
    spare view on a hidden stable parent widget and shows it once so
    its `wl_surface` is acquired.
  - `BrowserPool::Acquire(size_hint)` returns a
    `{Container, View, Backend}` triplet. It takes a spare from
    the pool or blocks (briefly) until one is built. It reparents
    the view to the new container parented under `parentWindow`
    (DC's container), fires `Navigator::Open(file)`, and schedules a
    `QTimer::singleShot(0, ...)` to build a new spare off the hot
    path so the next `Acquire` is instant.
  - `BrowserPool::Release(container)` reparents the view back to the
    hidden stable parent and hides it; clears the loaded page via
    `setUrl(QUrl("about:blank"))` so the next `Acquire` does not
    show stale content. The view itself stays in the pool as the
    next candidate spare.
  - `BrowserPool::Shutdown()` (from `DLL_PROCESS_DETACH`) releases
    every cached view and destroys the hidden stash widget.
- Resource cost: ~one additional `QWebEngineBackend` instance
  (~200–400 MB RSS for the zygote / GPU / renderer processes) is
  resident whenever no lister is open; the spare replaces itself, so
  only `N_active + 1` views are alive at any moment.
- No Windows-side changes. The pool is Linux-only
  (`#ifdef`-guarded in the new file). Windows keeps its existing
  per-`Create` constructor path in `EdgeLister_Linux.cpp`'s analogue
  is not touched.
- `DdlMain` `DLL_PROCESS_ATTACH` is updated to call
  `BrowserPool::Initialize()`; `DLL_PROCESS_DETACH` to call
  `BrowserPool::Shutdown()`. The Linux-only branch of
  `DllMain.cpp`'s `#else` block is updated.

## Capabilities

### New Capabilities

- `lister-window-pool`: observable behavior of how many
  `QWebEngineView` instances exist over a Double Commander session
  and how concurrent listers (F3 plus Ctrl+Q, repeated F3 opens
  without closing the previous one) interact with them.

### Modified Capabilities

- `wlx-contract`: the linux branch of `ListLoadW`,
  `ListLoadNextW`, and `ListCloseWindow` change from
  "create-once/destroy-once-per-lister" to "claim-and-return-from-pool".
  No Windows change. The 12 exported symbols / signatures are
  unchanged; only the Linux-side implementation differs.
- `linux-runtime`: the
  `openspec/changes/port-to-double-commander-linux/specs/linux-runtime/spec.md`
  delta's "Requirement: Linux build artifact" wording changes to
  reflect the new one-shot initialization call at plugin load and
  the always-resident spare cost. Other requirements unchanged.

## Impact

- Affected code (Linux only):
  - New `EdgeViewer/WebView/BrowserPool.{h,cpp}` — pool
    implementation, hidden stash widget, async-spare scheduler.
  - `EdgeViewer/EdgeLister_Linux.cpp` — `Create` claims from the
    pool instead of constructing a backend; `OpenIn` /
    `Close` continue to call the existing backend through the
    container handle.
  - `EdgeViewer/DllMain.cpp` — the Linux-only `#else` branch of
    `ListLoadW`/`ListCloseWindow` calls into the pool; the
    `DLL_PROCESS_ATTACH` / `DLL_PROCESS_DETACH` blocks call
    `BrowserPool::Initialize()` / `BrowserPool::Shutdown()`.
  - `EdgeViewer/WebView/QtWebEngineBackend.{h,cpp}` — `Close()`
    is repurposed: pool does NOT call it on Release (the view is
    reused), but it still gets called on Shutdown for full cleanup.
- Affected docs: `Readme.md` §"Ctrl+Q quick-view window jumps
  under native Wayland" updates from "XWayland workaround" to
  "fixed by pre-creating the browser pool; XWayland no longer
  required for Ctrl+Q on Wayland, only for the F3 standalone-lister
  case on exotic compositors." `AGENTS.md` §"Known limitations" row
  updated similarly.
- No new dependencies. Pure Qt6 / Qt Web Engine APIs already linked.
  No Windows source file touched.
- No breaking ABI change. The 12 WLX exports are unchanged; only
  Linux internal lifecycle changes.
