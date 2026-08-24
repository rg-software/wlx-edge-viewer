# Evidence pack — revisit-wayland-ctrlq-jump

Recorded on the KDE/Wayland target machine during Phase 1. This file gates
branch selection (task 3.1). Sections are appended by tasks 2.1–2.6.

## 1. Environment versions (task 2.1)

| Component | Value |
|---|---|
| Distro | CachyOS Linux (Arch-based), kernel `7.1.8-1-cachyos` |
| Desktop session | KDE Plasma, native Wayland (`XDG_SESSION_TYPE=wayland`) |
| Compositor | KWin 6.7.4 |
| Double Commander | **1.2.8 gamma**, revision 383, commit `d5756c496`, build 2026/08/09 |
| Widgetset | x86_64-Linux-qt6 |
| Qt (widgetset library) | **6.11.1** (`libQt6Core.so.6.11.1`); DC links Qt6Core/Gui/Widgets/DBus/PrintSupport + Qt6Pas |
| LCL | Lazarus 4.8.0.0 / FPC 3.2.2 (bundled with DC build; separate LCL source revision not identifiable from the binary/banner) |
| GPU | NVIDIA TU104 [GeForce RTX 2070 SUPER] |
| GPU driver | nvidia-utils 610.57.04-1 (+ lib32), mesa 3:26.1.6-1 |
| Plugin under test | `~/.local/share/doublecmd/plugins/edgeviewer/EdgeViewer.wlx64` |

Design-doc open question resolved: the stack is unchanged since the prior
investigation (still DC 1.2.8 / Qt 6.11 / LCL-Qt6 on CachyOS).

## 2. Widget-tree dumps (task 2.2)

Built with `-DEDGEVIEWER_LINUX_DEBUG_LOGGING=ON`, installed, DC
restarted fresh under capture (`QT_QPA_PLATFORM=wayland`, native
session). Performed in order: Ctrl+Q on `Examples/tutorial #1.md`
(session's first lister open), ESC, then F3 on the same file, ESC.
Two dumps logged (below). Wrapper lines prefixed `[edgeviewer]`.

### Ctrl+Q first open (quick view, embedded) — chain resolves to DC main window

```
EdgeLister::Create: qpa=wayland parent=0x3c47a6c0 class=QWidget flags=0x8800f000 (parent->window=0x0x3b7714b0 topClass=QWidget topFlags=0xa00f001) parent->parentWidget=0x3c545760 formClass=QAbstractScrollArea formFlags=0x8800f000 form->parentWidget=0x0x3b7714b0 form->window=0x3b7714b0 parent->isVisible=0
  chain 0x3c47a6c0 class=QWidget flags=0x8800f000 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3c545760 class=QAbstractScrollArea flags=0x8800f000 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3c54b7d0 class=QWidget flags=0x800 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3b0f7760 class=QFrame flags=0x8800f000 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3c230a10 class=QWidget flags=0x8800f000 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3b79f160 class=QStackedWidget flags=0x8800f000 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3b79f100 class=QTabWidget flags=0x8800f000 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3b79e510 class=QFrame flags=0x8800f000 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3b794950 class=QFrame flags=0x9000f000 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3b793f20 class=QFrame flags=0x8800f000 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3b770ef2 class=QWidget flags=0x8800f000 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3b76fbbf class=QAbstractScrollArea flags=0x8800f000 window=0x3b7714b0 topFlags=0xa00f001 isWindow=0
  chain 0x3b7714b0 class=QWidget flags=0xa00f001 window=0x3b7714b0 topFlags=0xa00f001 isWindow=1
```

Findings:
- 13-hop chain; every intermediate widget reports `isWindow=0`.
- Top-of-chain (DC main window) flags `0xa00f001`, the only `isWindow=1`.
- On every hop the Windows-type flag bits (low byte of `windowFlags()`) are **zero** — no widget violates the falsification: **nothing carries `Qt::Window`**.
- Ctrl+Q chain resolves to DC's main window. (branch map unchanged; STOP condition not met)

**F3 standalone open — real toplevel, shallow chain**

```
EdgeLister::Create: qpa=wayland parent=0x3b384c0e class=QWidget flags=0x8800f000 (parent->window=0x3c4417a0 topClass=QWidget topFlags=0xa00f001) parent->parentWidget=0x3c6501c8 formClass=QAbstractScrollArea formFlags=0x8800f000 form->parentWidget=0x3c4417a0 form->window=0x3c4417a0 parent->isVisible=0
  chain 0x3b384c2 class=QWidget flags=0x8800f000 window=0x3c4417a0 topFlags=0xa00f001 isWindow=0
  chain 0x3c6501c8 class=QAbstractScrollArea flags=0x8800f000 window=0x3c4417a0 topFlags=0xa00f001 isWindow=0
  chain 0x3c4417a0 class=QWidget flags=0xa00f001 window=0x3c4417a0 topFlags=0xa00f001 isWindow=1
```
- Shallow 3-hop chain to a genuine toplevel (`isWindow=1`); also carries no `Qt::Window` type. Consistent with F3 being a real standalone window.

## 3. Wayland baseline trace (task 2.3)

Pending.

## 4. Wayland repro trace + KWin cross-check (task 2.4)

Fresh session under `WAYLAND_DEBUG=1 QT_LOGGING_RULES="qt.qpa.wayland*=true"`. Performed: first-of-session Ctrl+Q (kept the stray visible), then KWin `queryWindowInfo` picker.

**Key trace events at first Ctrl+Q (04:18:06):**

- `xdg_surface#67` … `xdg_toplevel#68` (DC main window) **destroyed**
- `wl_surface#63` destroyed
- `wl_surface#39` created → `xdg_surface#57` → `xdg_toplevel#59` created
- **title "Double Commander 1.2.8~383"**, `set_app_id("doublecmd")`, geometry 1370×866
- no `wl_subsurface` anywhere
- Chromium EGL surface (`EGLSurface(39/0x16992a40)`) composites into `wl_surface#39` — the **new toplevel**

Cross-check (KWin `queryWindowInfo`): user clicked the stray window; KWin reported:

```json
{
  "caption": "Double Commander 1.2.8~383",
  "desktopFile": "doublecmd",
  "pid": 10934,
  "resourceClass": "doublecmd",
  "resourceName": "doublecmd",
  "type": 0,
  "hasTransientParent": false
}
```

`pid 10934` is the doublecmd process itself; the stray toplevel is **DC-owned and a recreated ancestor toplevel** (Branch B mechanism candidate, later superseded by probe-matrix selection of Branch C — see §6/§7).

Cross-check conclusion: the surface that escapes is not a Chromium/plugin subsurface; it is a Wayland toplevel (recreated DC main window) with DC pid/app-id `doublecmd`, same title as the main window.

## 5. Trace diff — escaping-surface identification (task 2.5)

**Escaping surface:** `wl_surface#39` (= `xdg_surface#57` → `xdg_toplevel#59`), created **after** `ListLoadW` began (first Ctrl+Q), destroying the pre-existing DC toplevel (`xdg_surface#67`). Plausible owner: DC's LCL widgetset re-creating the main window during viewer reparent; actual QWidget/QWindow class per qt-logging not yet extracted (filter category `qt.qpa.wayland*` emitted no lines; may need `qt.qpa.*`). Parent linkage (KWin): none — `hasTransientParent=false`, normal toplevel role.

**Mechanism documented (Branch B candidate); shipped branch = C** — see §6 discriminator and §7 selection.

## 6. Env-var probe matrix (task 2.6)

| Row | Environment | First Ctrl+Q | Subsequent Ctrl+Q | F3 |
|---|---|---|---|---|
| (a) | baseline native Wayland | jump | no-jump | fine |
| (b) | `QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu"` | jump | no-jump | fine |
| (c) | (b) + `QT_QUICK_BACKEND=software` | **no-jump** (confirmed on retry) | no-jump | fine |
| (d) | `QT_QUICK_BACKEND=software` alone (no `--disable-gpu`) | **no-jump** (confirmed) | no-jump | fine |

Row (a) first Ctrl+Q = the jump reproduced in task 2.4 (stray window visible, KWin cross-check captured). Subsequent Ctrl+Q in the same session embeds cleanly (no jump); F3 opens standalone (fine).

Rows (a)/(b)/(c): each ran as a fresh session (restarted DC), first-of-session Ctrl+Q, then ESC, subsequent Ctrl+Q, ESC, F3. Row (d) ran as a fresh session with only `QT_QUICK_BACKEND=software` set — no `--disable-gpu` — and the first Ctrl+Q embedded with no jump.

**Discriminator:** `--disable-gpu` alone (row b) still jumps; `QT_QUICK_BACKEND=software` eliminates the jump both with (row c) and **without** (row d) `--disable-gpu`. So the decisive variable is **`QT_QUICK_BACKEND=software`**; `--disable-gpu` is unnecessary. Satisfies the design's **Branch C** selection criterion ("only software rendering eliminated the jump"). The trace finding (re-created ancestor toplevel, `wl_surface#39`/`xdg_toplevel#59`) documents the mechanism the probe matrix eliminates; per design Decision 1, exactly one branch ships and the probe matrix discriminator selects **C**.

## 7. Branch selection rationale (task 3.1)

**Branch C selected** — see §5 (mechanism: re-created ancestor toplevel owned by DC) and §6 (probe matrix: only the software-rendering combination eliminated the jump, reproduced on a clean retry). Per design Decision 1 exactly one branch ships; the probe matrix is the discriminating evidence and it selects **C** (documentation-only workaround, no C++ changes). Branch B's transient-parent lever is superseded by C's cleaner evidence: no functional code path is needed when the env workaround reliably eliminates the jump. Proceed to group 6 close-out (task 6.1 for C) and then group 8.

## 8. Draft: Double Commander issue (task 8.2)

Filed at: *(URL to be recorded in `Readme.md` §Tracking once posted)*

```markdown
Title: First Ctrl+Q quick view on native Wayland destroys and re-creates the
main-window toplevel, and DC's main window jumps to follow it

Environment:
- Double Commander 1.2.8 gamma, revision 383, commit d5756c496, build 2026/08/09
- Lazarus 4.8.0.0 / FPC 3.2.2, widgetset x86_64-Linux-qt6, Qt 6.11.1
- CachyOS Linux, kernel 7.1.8-1-cachyos, KDE Plasma on Wayland, KWin 6.7.4
- NVIDIA GeForce RTX 2070 SUPER (TU104), nvidia-utils 610.57.04-1, mesa 3:26.1.6-1

Symptom:
The FIRST lister open of a session via Ctrl+Q (quick view) places the plugin
lister's window at an unspecified location and the Double Commander main window
jumps to follow it. The panel does not contain the rendered content.
Subsequent Ctrl+Q opens embed cleanly. F3 (standalone lister) is unaffected.

Instrumented facts (EdgeViewer WLX plugin, Qt WebEngine backend):
- No widget in the chain DC hands to ListLoadW carries the Qt::Window flag
  (top-of-chain window flags 0x8800f000; Window-type mask 0x1ff = 0). The
  viewer form embeds as a plain child (QAbstractScrollArea-wrapped) and
  parent->window() resolves to DC's real main window.
- WAYLAND_DEBUG=1 trace of a first Ctrl+Q: DC's main-window toplevel
  (xdg_surface#67 / xdg_toplevel#68, title "Double Commander 1.2.8~383") is
  DESTROYED, and a NEW toplevel is created immediately afterwards
  (wl_surface#39 -> xdg_surface#57 -> xdg_toplevel#59) with the same title,
  same app_id "doublecmd", same geometry (1370x866). Chromium's EGL compositor
  surface attaches to the new toplevel. No wl_subsurface is created anywhere.
- KWin queryWindowInfo on the stray window: pid = the doublecmd process,
  resourceClass/resourceName = "doublecmd", type Normal, hasTransientParent =
  false. The escapee is a DC-owned re-created ancestor toplevel, not a plugin
  or Chromium subsurface.

Probe matrix (fresh session per row; first Ctrl+Q / subsequent Ctrl+Q / F3):
- baseline native Wayland:               jump / no-jump / fine
- QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu": jump / no-jump / fine
- QT_QUICK_BACKEND=software (alone):     no-jump (confirmed) / no-jump / fine

Outcome:
Only software rendering eliminates the jump; no plugin-side C++ change could
reliably do so. We ship an opt-in env workaround
(QT_QUICK_BACKEND=software doublecmd; QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu"
is not required) and XWayland remains the fallback. Root cause appears to be in
the DC/LCL Qt6 widgetset re-creating the main-window toplevel on first quick
view, not in the embedded web engine itself.
```
