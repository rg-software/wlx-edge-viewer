# docs-linux-backend-qt — design

## Context

This change is documentation-only. The Qt Web Engine backend was
shipped in commit `738c3d1` and refined in `3970960` (container
widget embedding); the docs (`AGENTS.md`, `Readme.md`, and the
in-progress change's `proposal.md` / `design.md` / `tasks.md` /
`specs/linux-runtime/spec.md`) still describe the original
WebKitGTK + GTK 3 binding that was specified before the implementation
work began. The implementation switched to Qt Web Engine mid-way
because Double Commander's Qt6 build only accepts `QWidget*` as
the WLX lister parent (see proposal.md § "Detailed reasoning").

## Goals / Non-Goals

**Goals:**

- Bring the in-progress change's `proposal.md`, `design.md`,
  `tasks.md`, and `specs/linux-runtime/spec.md` into alignment
  with the as-shipped Qt Web Engine implementation.
- Capture the rationale for choosing Qt Web Engine, the rationale
  for keeping the `ev://` custom URI scheme, and the empirical
  finding about the Ctrl+Q quick-view Wayland jump, in the docs
  (so future readers understand why the implementation differs
  from the originally-written prose).
- Bring the top-level `AGENTS.md` and `Readme.md` into alignment.
  Partially done in `738c3d1` (initial `WebKitGTK` → `Qt Web Engine`
  substitution) and fully done in `3970960` (refined root-cause
  analysis for the Ctrl+Q Wayland jump, plus the Chromium subprocess
  overhead note).

**Non-Goals:**

- No code changes. The behavior is fixed; only the prose describing
  it changes.
- No new file types, no new ini keys, no new exporter functions.
- No change to the `QWebEngineView` compositor surface behavior
  (that is a Qt / Qt Web Engine upstream concern, documented for
  future maintainers but not actionable from this plugin).
- No fix for the first-`ListLoadW`-Chromium-subprocess spawn cost
  (inherent to `QWebEngineView`, documented as a known property of
  the chosen backend, not a defect).

## Decisions

### Decision 1: Treat this as a docs-only change with `skip_specs: true`

The behavior of the plugin does not change. The library the
implementation links against changes from WebKitGTK 4.1 + GTK 3
to Qt 6 WebEngine, but every requirement in the in-progress change's
`specs/linux-runtime/spec.md` continues to be satisfied (a WLX
plugin named `EdgeViewer.wlx64` that exports the 12 WLX symbols,
renders Markdown / AsciiDoc / RST / HTML / MHT / EML / URL / Images /
Other through shared `Resources/assets/<type>/loader.html`
templates, and honors the same `edgeviewer.ini` keys). The spec
language has to change because it now refers to a different
binding, but the behavior it specifies is identical. `skip_specs:
true` is the correct opt-out.

### Decision 2: Update the in-progress change's delta spec in place

The `linux-runtime` capability exists only as a delta spec under
`openspec/changes/port-to-double-commander-linux/specs/linux-runtime/spec.md`
(it is not yet a top-level capability at `openspec/specs/`). The
delta spec's `## ADDED Requirements` are being edited directly to
substitute "Qt 6 Web Engine" for "WebKitGTK" in each
requirement's prose. The semantics are unchanged; the binding name
is updated to match what shipped. Editing the delta spec is
structurally less clean than opening a follow-up delta from a
separate change (openspec expects capability deltas to apply to
top-level capabilities, not to open-change deltas), but it is
correct and avoids holding two open changes against the same
artifact. Alternative: opening a top-level
`openspec/specs/linux-runtime/spec.md` and moving the delta
content into it, then having `docs-linux-backend-qt` open a delta
against that top-level spec. That is more work and adds a top-level
spec whose only purpose is to be amended again immediately.

### Decision 3: Reference the qtpdfview_qt comparison in both AGENTS.md and Readme.md

The Ctrl+Q Wayland jump root-cause analysis is non-obvious and
benefits from a concrete counter-example. The
`j2969719/doublecmd-plugins/wlx/qtpdfview_qt` plugin embeds a
`QPdfView` (no compositor surface) and does not exhibit the jump
in the same environment. Pointing readers at that plugin makes
the root cause (`QWebEngineView`'s compositor surface attachment)
discoverable rather than something they have to rediscover from
first principles. The reference is included in both
`AGENTS.md`'s "Known limitations" section and `Readme.md`'s
"Ctrl+Q quick-view window jumps under native Wayland" section.

### Decision 4: Document the per-`ListLoadW` Chromium subprocess cost as a known property, not a defect

`QWebEngineView` is backed by Chromium; each instance spawns
zygote + GPU + renderer processes. This is the cost of supporting
the loaders' JS/CSS stack (marked.js, highlight.js,
asciidoctor.js, mermaid, mathjax). The cost is paid at first
`ListLoadW` of a session and is amortized by Chromium reusing
the profile's processes across subsequent loads. Documenting
this in the user-facing `Readme.md` (and in `AGENTS.md`'s "Known
limitations") sets expectations: this is not a bug, and there
is no in-plugin fix without dropping the JS/CSS rendering
stack.

## Risks / Trade-offs

- **Risk**: a future reader might think the docs are inconsistent
  if they look at `port-to-double-commander-linux/specs/linux-runtime/spec.md`
  and notice the binding has been retroactively changed. **Mitigation**:
  the spec at that path is a delta spec for an open change; the
  delta spec will be archived alongside the change. Future readers
  looking at the archived spec should also look at the change's
  history (and at commit `738c3d1`) to see what was actually shipped.
  The proposal and design of `docs-linux-backend-qt` capture the
  full rationale.

- **Trade-off**: the change intentionally does not attempt to fix
  the Ctrl+Q Wayland jump from inside the plugin. Offscreen render
  + `QPainter` blit would lose the scrolling / clicking / zooming
  that the loaders need; switching to `webengine-minimal` would
  lose the JS/CSS rendering stack. The recommended workaround
  (`QT_QPA_PLATFORM=xcb doublecmd`) is the pragmatic conclusion.
  Documenting this limitation in the top-level `Readme.md` and
  `AGENTS.md` (instead of hiding it) means users will find it
  before being surprised.

- **Risk**: leaving the in-progress `port-to-double-commander-linux`
  change open while editing its delta spec from a separate change
  creates a small "two open changes touch the same artifact"
  hazard. **Mitigation**: the delta spec edits are the only
  in-place changes; everything else is in this change's own
  artifacts. The in-progress change can still be archived
  normally once its `tasks.md` (which describes the actual
  shipped Qt Web Engine binding) is finalized.