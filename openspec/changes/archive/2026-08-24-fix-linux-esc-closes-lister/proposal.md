## Why

The `port-to-double-commander-linux` change shipped a working Linux
lister for the core text loaders and images, but the WLX contract's
ESC-closes-lister behavior was not wired: Total Commander's panel-level
keymap posts `WM_CLOSE` to the lister HWND when ESC is pressed (the
lister's `WNDPROC` then destroys the window in `DefWindowProc`),
while Double Commander's Qt6 panel does not close the lister on ESC
and `EdgeLister_Linux.cpp` installed no key handling. Result: pressing
ESC in any lister is a no-op.

## What Changes

- **Modified** (`EdgeViewer/WebView/QtWebEngineBackend.{h,cpp}` +
  `EdgeViewer/EdgeLister_Linux.cpp`): wire a simple in-page JS → host
  bridge that closes the lister on ESC. The mechanism:

  1. `AllocateContainerId()` allocates a per-instance token before
     backend construction; the backend's constructor embeds the token
     into a JS snippet registered via `AddScriptToExecuteOnDocumentCreated`.
  2. The JS installs a `keydown` listener on the window. On `Escape`
     (and only when `defaultPrevented` is false), it issues
     `new Image().src = 'ev://_close/<id>'`.
  3. `EvSchemeHandler::requestStarted` recognizes the `_close` host,
     parses the id, looks up the lister container `QWidget*` in a
     process-wide `id → QWidget*` map, **erases the entry first to
     prevent dispatch races**, then posts a **synthetic `Q` keypress**
     (KeyRelease) to the container's parent widget (DC's viewer panel).
  4. DC's hotkey handler processes the `Q` identically to a physical
     press, invoking `cm_ExitViewer` → lister close → DC calls
     `ListCloseWindow` on this plugin as it does for any other lister
     close.  The plugin does not destroy the container itself; that
     would race DC's close logic and trigger its parent-widget
     destruction hooks (in some Qt6 widgetsets this exits the
     application).
  5. The container's `QObject::destroyed` signal drives
     `UnregisterContainer(id)`, so the registry never holds a
     dangling pointer even if the lister is closed by an external
     path.

   Why synthetic Q and not calling ListCloseWindow directly:
   `hide()`/`close()` on the container from within the ev:// scheme
   handler either exits DC entirely (synchronous) or has no effect
   (deferred via QTimer). `QCloseEvent` posted to the parent widget
   is ignored. The synthetic `Q` follows the exact same dispatch
   path DC uses for its built-in `Q` shortcut, which is known to
   work reliably.

   Why a JS bridge and not a `QObject::eventFilter`: a
   `QObject::eventFilter` installed on the embedded `QWebEngineView`
   does NOT see key events — Chromium intercepts keyboard input at a
   level below Qt's event system on `QWebEngineView` (confirmed by
   instrumentation during this work: the filter logged 38 events of
   varying types over a session but none were `QEvent::KeyPress`
   (type 6) or `QEvent::KeyRelease` (type 7)). The in-page `keydown`
   listener is the only reliable interception point.

   Why `new Image().src = url` and not `fetch(url)`: Chromium's JS
   `fetch()` API has a server-side allowlist (`URL.supportedSchemes`)
   that doesn't pick up schemes registered via
   `QWebEngineUrlScheme::registerScheme`, even with the `CorsEnabled`
   flag. `fetch()` therefore rejects `ev://_close/<id>` with
   `URL scheme "ev" is not supported.` `<img>` requests do not have
   that restriction: any URL set as `src` triggers a navigation
   request that the global scheme handler picks up.

## Capabilities

### Modified Capabilities

- `wlx-contract`: extend the existing WLX entry-point behavior
  description to note that on Linux, `ESC` is delivered to the lister
  via a JS bridge (in-page `keydown` listener + `ev://_close/<id>`
  request routed to a synthetic `Q` keypress on DC's viewer panel)
  rather than via the host's panel keymap (Windows: TC's keymap posts
  `WM_CLOSE`). The wire-level contract — pressing ESC closes the
  lister — is unchanged on both platforms.

## Impact

- **Code**: ~40 LOC added across `QtWebEngineBackend.{h,cpp}` (id
  registry, scheme-handler dispatch, JS injection) and
  `EdgeLister_Linux.cpp` (allocate id, register container, wire
  destroyed-signal for auto-unregister, pass id to backend).
- **Build**: no CMake change; uses already-linked `<atomic>` and
  existing Qt headers.
- **Dependencies**: none.
- **Readme**: the existing "Known limitations" bullet describing the
  JS bridge remains accurate.
- **Systems**:
  - Linux users in any lister pane: ESC now closes the lister, matching
    Windows behavior. One press fully closes the lister (container
    + embedded WebView).
  - Linux users in image-viewer fullscreen: ESC closes the lister
    (the loader's fullscreen toggle uses `F`, so no behavior collision).
  - Windows users: no change.
