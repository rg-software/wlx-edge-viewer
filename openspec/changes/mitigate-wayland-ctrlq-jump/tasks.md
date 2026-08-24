> **SUPERSEDED** — the Ctrl+Q native-Wayland jump was re-investigated with
> surface-level evidence, a fix shipped (Branch C: software-rendering
> workaround, documentation-only), and the mechanism confirmed by trace +
> KWin cross-check. See `openspec/changes/revisit-wayland-ctrlq-jump/`.

# mitigate-wayland-ctrlq-jump — FINAL INVESTIGATION RECORD

## Outcome

**Closed without shipping.** Three plugin-side mitigation attempts were
made on the user's KDE / Wayland / DC 1.2.8 / Qt 6.11 / LCL-Qt6
machine; none resolved the Ctrl+Q "lister at screen center, DC main
window jumps" symptom, and the final timing-based attempt introduced
a navigation crash. The escape is rooted in Qt Web Engine's
compositor surface attaching to DC's embedded form's `wl_surface`
(instead of the panel's), determined upstream of anything reachable
from a plugin. The recommended workaround — `QT_QPA_PLATFORM=xcb
doublecmd` — remains authoritative and is already documented in
`Readme.md` and `AGENTS.md`.

Commits in this change (all kept for the historical trail):

- `8a3102c` — first heuristic correction: try stripping `Qt::Window`
  on the form rather than the central widget. *No-op on this DC build
  because LCL wraps the embedded form as `QAbstractScrollArea`
  without `Qt::Window` — confirmed by the debug-logged widget chain
  (chain depth 13 on Ctrl+Q, no widget in the chain carries
  `Qt::Window`).*
- `c04adb1` — defer `Navigator::Open` to `QShowEvent`. *Hook fires
  correctly per the log; jump still occurs because the Chromium
  compositor surface initializes when the widget is shown, not when
  `setHtml` is called.*
- `9bf2b0c` — defer `container->show()` AND `Navigator::Open` to the
  same `QShowEvent`. *Hook fires correctly per the log; jump still
  occurs for the same upstream-Qt-Web-Engine reason.*
- `88d8b25` — added lifecycle logging to observe the hook firing
  (instrumentation); jump unchanged in subsequent retest.
- `0cf2b6b` — *revert*: navigation-within-Ctrl+Q crashed
  (`EThreadError`) because `OpenIn`'s `cancel()`-then-synchronously-
  fire-`Navigator::Open` raced with the in-flight Chromium worker
  cleanup from the previous `setHtml`. Reverted to the pre-investigation
  `container->show()` + inline `Navigator::Open`, which is
  navigation-stable (F3 + Ctrl+Q + Ctrl-Y inside Ctrl+Q all work,
  Ctrl+Q still jumps on native Wayland, XWayland workaround stays).

Plugin code state after revert:

- `EdgeLister::Create` builds the container, sets up `QVBoxLayout`,
  parents the `QWebEngineView` into it, calls `container->show()`,
  stores the handle in `gs_Views`, and calls `Navigator::Open(file)`
  inline. Byte-equivalent to commit `b3fd0a1` before this
  investigation.
- An optional widget-tree debug block (parent + form + flags +
  `parent->window()` + full `parentWidget()` chain) is gated behind
  `EDGEVIEWER_LINUX_DEBUG` (set via `-DEDGEVIEWER_LINUX_DEBUG_LOGGING=ON`
  CMake option). Useful for any future Ctrl+Q investigation.

## Future direction (kept on the record, not implemented)

The user's "pre-create an invisible browser per session, List* only
shows existing" idea survives as §3.0. It is the only known lever
that could dissolve the first-creation-only timing problem rather than
run around it; doing it requires module-level state (a process-global
shared_ptr to a long-lived `QtWebEngineBackend`), race-safe first-
create / last-destroy lifecycle, and — critically — letting the user
preview a different file in a second lister without UI contention
(today each `ListLoadW` owns its own backend). Out of scope for this
change; revisit as a separate dedicated change if the user re-prioritises.
