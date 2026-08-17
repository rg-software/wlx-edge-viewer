# docs-linux-backend-qt

## Why

The Linux backend was originally specified and implemented against
WebKitGTK 4.1 + GTK 3. During the implementation the binding was
switched to Qt 6 WebEngine (QtWebEngineBackend) because Double
Commander's Qt6 build ships its own Qt toolkit and accepts a
`QWidget*` as the lister parent — a `GtkWidget*` cannot be embedded
into a Qt widget tree, and a `QWebEngineView` cannot be embedded
into a GTK widget tree. The committed code (`738c3d1` on
`port-to-double-commander-linux`) is now Qt Web Engine. Several
documents (`AGENTS.md`, `Readme.md`, the
`port-to-double-commander-linux` openspec change) still describe the
WebKitGTK binding and need to be updated to match what was actually
shipped, with a written rationale for future readers.

Two additional findings emerged during empirical testing and are
recorded alongside the doc sync:

1. **The custom `ev://` scheme is still required.** Qt Web Engine
   (Chromium) does not allow registering `http` as a custom URI
   scheme (Chromium reserves it for actual web traffic); the same
   reason that motivated `ev://` under WebKitGTK 2.38+ applies to
   Qt Web Engine. The `http://` → `ev://` rewrite in
   `QtWebEngineBackend::NavigateToString` stays unchanged.

2. **The Ctrl+Q quick-view jump under native Wayland is rooted in
   the `QWebEngineView` compositor surface, not just the embedded
   `QMainWindow`'s `Qt::Window` flag.** Confirmed by compiling
   [`j2969719/doublecmd-plugins/wlx/qtpdfview_qt`](https://github.com/j2969719/doublecmd-plugins/tree/master/plugins/wlx/qtpdfview_qt)
   (which embeds a plain `QPdfView`, no compositor surface) and
   observing it does NOT exhibit the Ctrl+Q jump in the same DC
   environment. Plugin-side mitigations (return own container,
   deferred `show()`, `createWinId()`) do not resolve the
   compositor-surface promotion; `QT_QPA_PLATFORM=xcb` is the
   recommended workaround. The
   `doublecmd/plugins/wlx/kate/defects.md` notes ("Core Problem"
   section) document the same Wayland subsurface promotion concern
   from another plugin author's perspective.

## What Changes

- **Modified**: `openspec/changes/port-to-double-commander-linux/specs/linux-runtime/spec.md` — the Linux runtime delta spec. Replaces WebKitGTK/gtk3 references with Qt 6 / Qt Web Engine equivalents; updates the build-prerequisites list (`libwebkit2gtk-4.1` + `gtk3` → `Qt6WebEngineWidgets` + `Qt6Widgets`); revises the scheme-registration rationale (WebKitGTK 2.38+ rejection → Chromium reserves `http`); notes that the `ev://` rewrite stays.
- **Modified**: `openspec/changes/port-to-double-commander-linux/proposal.md` — update the `libwebkit2gtk-4.1` mention; replace the future-work rows that describe WebKitGTK behavior (zoom, accelerator-keys, dark-mode) with Qt Web Engine equivalents; replace the cross-engine reference in §Removed (HTML charset override).
- **Modified**: `openspec/changes/port-to-double-commander-linux/design.md` — replace the WebKitGTK-specific rationale sections (scheme registration, CORS, `WEBKIT_DISABLE_DMABUF_RENDERER`) with the Qt Web Engine equivalents (`QWebEngineUrlScheme`, `QWebEngineProfile` defaults, the `ev://` URL scheme callback); update Decision 11 (Linux exports) to reflect the GNU ld version script (already implemented in commit `5361f19`); add a note about the `QWebEngineView` compositor surface and the Wayland Ctrl+Q limitation.
- **Modified**: `openspec/changes/port-to-double-commander-linux/tasks.md` — replace the WebKitGTK-specific task lines with Qt Web Engine equivalents.
- **Modified**: `AGENTS.md` — already partially updated in `738c3d1`; this change adds the rationale prose around the Qt choice (briefly restated here) and refines the Ctrl+Q Wayland note to point at the `QWebEngineView` compositor surface as the root cause (already done in `3970960`).
- **Modified**: `Readme.md` — same updates as `AGENTS.md`, plus the "process overhead" subsection documenting the per-`ListLoadW` Chromium subprocess spawn cost (already done in `3970960`).

No code changes. No requirement changes beyond updating the
already-known-wrong binding/library references in the docs.

## Capabilities

### Modified Capabilities

- `linux-runtime` — the requirements in this delta spec now refer to Qt 6 / Qt Web Engine. No observable behavior change (file types, rendering model, ini keys, asset paths, pre-fetch are all identical). The deltas needed are purely documentation corrections: the `SHALL link against libwebkit2gtk-4.1` lines become `SHALL link against Qt6WebEngineWidgets via Qt6`, the `WebKitGTK scheme handler` lines become the `ev://` scheme callback, etc. The implementation already satisfies the corrected requirements.

## Impact

- Affected docs only. No source changes.
- Affected files:
  - `openspec/changes/port-to-double-commander-linux/specs/linux-runtime/spec.md`
  - `openspec/changes/port-to-double-commander-linux/proposal.md`
  - `openspec/changes/port-to-double-commander-linux/design.md`
  - `openspec/changes/port-to-double-commander-linux/tasks.md`
  - `AGENTS.md` (already updated in `3970960`)
  - `Readme.md` (already updated in `3970960`)
- No new dependencies. No new capabilities.
- Pre-existing observations worth recording (already in AGENTS.md / Readme.md after `3970960`):
  - **Ctrl+Q quick-view window jumps under native Wayland** — root cause is `QWebEngineView`'s compositor surface attaching to the embedded `QMainWindow`'s separate `wl_surface` (created because DC's `TQtMainWindow.ChangeParent` preserves `Qt::Window` on the embedded form). Empirically confirmed against `qtpdfview_qt` (uses `QPdfView`, no compositor surface, not affected). Plugin-side mitigations attempted do not resolve the underlying promotion. Workaround: `QT_QPA_PLATFORM=xcb doublecmd`. Track at github.com/doublecmd/doublecmd.
  - **First `ListLoadW` of a session spawns Chromium subprocesses (zygote + GPU + renderer)** — inherent to `QWebEngineView` (Chromium-backed). Subsequent loads in the same session are faster because Chromium reuses the profile's processes. `QT_WEBENGINE_DISABLE_SANDBOX=1` and `--single-process` reduce fork overhead at the cost of stability. Switching to a lighter web engine (e.g. Qt WebEngine's `webengine-minimal`) would lose the JS/CSS rendering stack (marked.js, highlight.js, mermaid, mathjax). Document for users, not a defect.

## Detailed reasoning

### Why Qt 6 Web Engine and not WebKitGTK

Double Commander 1.2+ ships three official widgetset builds: GTK2, Qt5, Qt6. The WLX lister contract passes a native widget handle as the parent of the plugin's view:

- The GTK2 build passes a `GtkWidget*`. A `QWebEngineView` cannot be embedded in a GTK widget tree — Qt and GTK have separate GMainLoop / signal systems and the widget trees are entirely foreign to each other.
- The Qt5 / Qt6 builds pass a `QWidget*`. A `WebKitGTK` widget (`WebKitWebView`) also cannot be embedded in a Qt widget tree.

Cross-widgetset embedding at the binary level (passing each other's opaque pointers) is not workable. So the plugin must bind to whatever widgetset the host is built with. WebKitGTK 4.1 + GTK 3 would have worked only for the GTK build of DC, which the user has explicitly excluded by running `doublecmd` (Qt6 build, see `/proc/$(pgrep -x doublecmd)/environ` and the build banner "Widgetset library: Qt 6.11.1"). Qt 6 Web Engine is the only web-engine binding that targets the host's Qt widget tree directly. The Qt6 Web Engine + Qt6 Widgets choice is forced by the host; not a stylistic preference.

### Why `ev://` is still needed

Qt Web Engine (Chromium) does not allow registering `http` as a custom URI scheme — Chromium reserves the `http` and `https` schemes for actual web traffic and explicitly rejects global scheme-handler installation for them. This is the same reason that motivated `ev://` under WebKitGTK 2.38+. The `http://` → `ev://` rewrite in `QtWebEngineBackend::NavigateToString` therefore stays in place; it is invisible to loaders that keep their `http://assets.example/...` references.

### Why the Ctrl+Q quick-view jump under native Wayland is not fixable from the plugin

Empirical investigation (commit `3970960` plus the qtpdfview_qt cross-check) established that:

1. **Structural fact**: `QWebEngineView` creates an internal Chromium compositor `QWindow`. On Wayland, Qt WebEngine attaches that compositor `QWindow` as a `wl_subsurface` of the nearest `wl_surface` in the widget tree's ancestor chain.
2. **DC structural fact**: LCL Qt6's `TQtMainWindow.ChangeParent` (`lcl/interfaces/qt6/qtwidgets.pas:7459-7484`) preserves the `Qt::Window` flag on a `QMainWindow` even when the form is parented to the quick-view panel. On Wayland, a `QWidget` with `Qt::Window` becomes its own top-level `wl_surface`, separate from the parent's `wl_surface`.
3. **Combined effect under Ctrl+Q** (quick view): the embedded viewer form has its own `wl_surface`. The QWebEngineView's compositor `wl_subsurface` attaches to that form's `wl_surface`, not to DC's main `wl_surface`. The compositor positions both independently — DC's main window "jumps" to align with the form's window. Under F3 (standalone lister), the form is a genuine top-level (no parent promotion), so the QWebEngineView attaches directly to DC's main `wl_surface` and the jump doesn't occur.
4. **Cross-check with `qtpdfview_qt`**: the plugin at `github.com/j2969719/doublecmd-plugins/tree/master/plugins/wlx/qtpdfview_qt` embeds a `QPdfView` — a plain Qt widget that paints via `QPainter` into its own widget window, with no separate compositor surface. It exhibits no Ctrl+Q jump in the same environment. This isolates `QWebEngineView`'s compositor surface as the differing factor.
5. **Plugin-side mitigations attempted (all ineffective)**: returning our own `QWidget*` container as the plugin handle (so DC's `QWidget_move/resize` operates on our container, not on DC's widget), deferring the `QWebEngineView`'s `show()` via a `QEvent::Show` filter, calling `parent->createWinId()` upfront. None of these change which `wl_surface` the QWebEngineView's compositor attaches to — that is determined by Qt WebEngine and Qt Wayland internally.

### Where the fix could land

- **Qt Wayland platform plugin** (`qtbase/src/plugins/platforms/wayland/`): how it handles parented widgets that have `Qt::Window`. Two realistic improvements: (a) when a child widget has both `Qt::Window` AND an explicit `setParent()` to a widget inside a different top-level, automatically strip `Qt::Window`; or (b) provide a per-window opt-out flag (e.g. `Qt::WA_DontCreateSeparateWlSurface`) so the widget can keep its `Qt::Window` for menubar/toolbar/statusbar inheritance while sharing the parent's `wl_surface`. Option (b) is more targeted and backward-compatible.
- **Qt WebEngine** (`qtwebengine/`): how `QWebEngineView` creates and parents its internal compositor window. A theoretical API to specify the parent surface would help, but Qt WebEngine's design is built around the compositor-surface model and is unlikely to change.
- **DC widgetset** (Lazarus Qt6 `TQtMainWindow.ChangeParent`): clear `Qt::Window` when reparenting (preserving only the relevant menubar/toolbar/statusbar flags). This is the lower-effort fix but requires re-verifying every LCL Qt6 form's window-flag behavior.

The fix is non-trivial from inside the plugin alone: offscreen render + `QPainter` blit would lose the scrolling / clicking / zooming that the loaders need, and switching to `webengine-minimal` would lose the JS/CSS rendering stack (marked.js, highlight.js, mermaid, mathjax). The recommended workaround is therefore `QT_QPA_PLATFORM=xcb doublecmd` until Qt's Wayland surface handling matures.