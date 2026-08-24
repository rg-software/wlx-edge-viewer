## ADDED Requirements

### Requirement: ESC closes the lister on Linux

On Linux, when the user presses the ESC key while the embedded web view
has focus, the plugin SHALL close the lister (destroy the embedded
`QWebEngineView`, erase the `gs_Views` entry) and return focus to the
file panel. The mechanism is an in-page JS `keydown` listener injected
by `QtWebEngineBackend`'s constructor via
`AddScriptToExecuteOnDocumentCreated`; the listener triggers a request
to `ev://_close/<id>` (via `new Image().src = url`, since Chromium's JS
`fetch()` API has a server-side allowlist that rejects custom schemes
even when registered via `QWebEngineUrlScheme::registerScheme`) which
the global `EvSchemeHandler` routes back to the registered container
`QWidget*` and posts a **synthetic `Q` keypress** to the container's
parent widget (DC's viewer panel). DC's hotkey handler processes the
`Q` identically to a physical press, invoking `cm_ExitViewer` which
closes the lister through DC's normal machinery (calling
`ListCloseWindow` on this plugin as it does for any other lister close).

- Windows behavior is unchanged: Total Commander's panel-level keymap
  posts `WM_CLOSE` to the lister HWND when ESC is pressed; the lister's
  `WNDPROC` (`EdgeViewer/EdgeLister_Win.cpp`) falls through to
  `DefWindowProc`.
- The Linux JS bridge is parented to the embedded `QWebEngineView`'s
  `QWebEngineScriptCollection`; the per-instance `<id>` token is
  allocated by `AllocateContainerId()` and unregistered by the
  container's `QObject::destroyed` signal via `UnregisterContainer(id)`.
- The JS listener SHALL ignore events with `e.defaultPrevented` true,
  so loaders that consume ESC themselves (none today) are not affected.
- **Why synthetic Q and not calling ListCloseWindow directly**:
  `hide()`/`close()` on the container from within the ev:// scheme
  handler either exits DC entirely (synchronous) or has no effect
  (deferred via QTimer). `QCloseEvent` posted to the parent widget is
  ignored. The synthetic `Q` follows the exact same dispatch path DC
  uses for its built-in `Q` shortcut, which is known to work reliably.
- **Why a JS bridge and not a `QObject::eventFilter`**: a
  `QObject::eventFilter` installed on `QWebEngineView` does NOT see
  key events — Chromium intercepts keyboard input at a level below
  Qt's event system (confirmed by instrumentation: the filter logged
  38 events of varying types over a session but none were
  `QEvent::KeyPress` (type 6) or `QEvent::KeyRelease` (type 7)).
- **No interference with image fullscreen**: the image viewer's
  fullscreen toggle uses `F` (its own keydown listener), not ESC.

#### Scenario: ESC closes an open lister

- **WHEN** the user opens any file in the lister pane (F3) and presses
  ESC while focus is in the rendered view
- **THEN** the lister pane is dismissed and the file panel regains focus

#### Scenario: ESC in image fullscreen closes the lister

- **WHEN** the user has pressed `F` to enter the image-viewer's
  fullscreen class and then presses ESC
- **THEN** the lister closes (the loader's fullscreen toggle uses
  `F`, so ESC is not consumed by the loader; the bridge dispatches
  the close request)
