## Why

Running Double Commander under XWayland (`QT_QPA_PLATFORM=xcb`) is currently required because, on native Wayland, the first Ctrl+Q quick-view open makes the plugin lister appear at screen center and DC's main window jump to follow it. The root cause recorded in `Readme.md`, `AGENTS.md`, and the `EdgeLister_Linux.cpp` header comment ("DC's `TQtMainWindow.ChangeParent` retains `Qt::Window`") was **falsified by instrumentation** during the closed `mitigate-wayland-ctrlq-jump` investigation: no widget in the chain DC hands us carries `Qt::Window` (flags `0x8800f000`, Window-type mask zero). The identity of the actually-escaping `wl_surface` has never been established, and at least four plausible levers remain untried (native-widget boundary attributes, the designed-but-never-executed `setTransientParent` fallback, Chromium GPU/software rendering probes, the lazily-built browser pool). The crashes observed during that investigation were artifacts of the experiment hooks themselves (a cancel-then-fire race producing `EThreadError`; an Initialize-time pre-created spare rendering F3 blank), not inherent to embedding. This change replaces speculation with surface-level evidence, corrects the documentation record, and attempts exactly one evidence-selected fix.

## What Changes

- **Documentation correction**: rewrite the falsified `ChangeParent`/`Qt::Window` root-cause narrative in `Readme.md` §"Ctrl+Q quick-view window jumps under native Wayland" (plus Future-work row 9), `AGENTS.md` §Known-limitations, and the `EdgeLister_Linux.cpp` header comment, replacing it with the instrumented facts (no `Qt::Window` anywhere in the parent chain; form embeds as plain child; escape created later; owner unknown; first-Ctrl+Q-of-session only).
- **Diagnosis phase (no functional code changes)**: a documented, repeatable procedure on the real KDE/Wayland machine that identifies the escaping surface by name: rebuild with the existing `-DEDGEVIEWER_LINUX_DEBUG_LOGGING=ON` widget-tree dump, capture a `WAYLAND_DEBUG=1` protocol trace correlated with `QT_LOGGING_RULES="qt.qpa.wayland*=true"` around a first Ctrl+Q, cross-check with KWin `queryWindowInfo`, and run a zero-code env-var probe matrix (`QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu"`, optional `QT_QUICK_BACKEND=software`).
- **Fix phase (gated by the evidence; exactly one branch ships)**: Branch A - make our container the native boundary (`Qt::WA_NativeWindow` / `WA_DontCreateNativeAncestors`) so Chromium's surface attaches below DC's widgets; Branch B - execute the designed-but-untested `QWindow::setTransientParent` fallback; Branch C - if only software rendering eliminates the jump, document the env-var recommendation without re-introducing engine switches (registering `[WebView] Switches` on Linux remains separate future work per AGENTS.md); Branch D - if all fail, spin off the lazily-built browser pool (spare built on first Acquire, not Initialize) as its own future change.
- **Guardrails carried forward** (from the investigation record): no synchronous cancel-then-fire in `OpenIn`; nothing pre-created against unmapped surfaces at Initialize; the inline `container->show()` + `Navigator::Open` path stays the fallback at every step.
- **Upstream escalation**: file the previously-promised-but-never-filed Double Commander issue using the collected evidence pack.
- Supersedes the closed-without-shipping investigation record in `openspec/changes/mitigate-wayland-ctrlq-jump/tasks.md`.

## Capabilities

### New Capabilities
- `wayland-ctrlq-embedding`: Observable embedding behavior of the plugin lister under Double Commander's Ctrl+Q quick view and F3 standalone lister on native Wayland - which surface hosts the rendered content, that DC's main window does not reposition, that first and subsequent opens behave identically once a fix ships, and the evidence-recording contract (which artifact identifies the escaping surface) that gates each fix branch.

### Modified Capabilities
<!-- None. The Ctrl+Q jump was a documented known limitation, not a spec'd
     requirement; no existing capability's requirements change. -->

## Impact

- **Affected code (fix branches A/B only)**: `EdgeViewer/EdgeLister_Linux.cpp` - header comment correction plus, for the shipping branch, a small attribute/transient-parent block in `EdgeLister::Create` behind a compile-time constant. Diagnosis phase touches no C++.
- **Affected docs**: `Readme.md` (§"Ctrl+Q quick-view window jumps under native Wayland", Future-work row 9), `AGENTS.md` (§Known limitations bullet), `openspec/changes/mitigate-wayland-ctrlq-jump/tasks.md` cross-reference note.
- **Platform scope**: Linux-only behavior; Windows sources are untouched. Verification still builds Release Win32 + x64 to prove no Windows breakage.
- **No new dependencies.** Pure Qt6 widget API already linked (`find_package(Qt6 ... COMPONENTS WebEngineWidgets Widgets)`); `vcpkg.json` untouched; no `Resources/assets/` changes; no `edgeviewer.ini` keys added, removed, or renamed.
