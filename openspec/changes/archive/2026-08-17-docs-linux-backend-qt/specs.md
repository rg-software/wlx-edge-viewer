# docs-linux-backend-qt — specs

This change has `skip_specs: true` set in `.openspec.yaml`.

**Why no spec delta is being created here:**

- The `linux-runtime` capability does not yet exist as a top-level spec at `openspec/specs/linux-runtime/spec.md`. It exists only as an `## ADDED Requirements` delta inside the in-progress change `openspec/changes/port-to-double-commander-linux/specs/linux-runtime/spec.md`. Modifying that delta from a separate change (`docs-linux-backend-qt`) is structurally awkward — openspec expects capability deltas to apply to top-level (archived) capabilities, not to open-change deltas.
- The behavior the `linux-runtime` delta spec describes has not changed. The plugin still builds a shared object named `EdgeViewer.wlx64` that exports the same 12 WLX symbols, renders the same file types through the same loader HTML templates, and honors the same `edgeviewer.ini` keys. The only thing that has changed since the delta was written is which web-engine library it links against — and that is a library-name / reference correction, not a behavior change.

**What is being updated instead:**

The in-progress change's delta spec at
`openspec/changes/port-to-double-commander-linux/specs/linux-runtime/spec.md`
is being edited directly to replace the `libwebkit2gtk-4.1` + `gtk3`
mentions with `Qt6WebEngineWidgets` + `Qt6Widgets`, and to replace the
"WebKitGTK" references in the requirements with "Qt Web Engine"
equivalents. The semantics of every requirement remain unchanged —
they are now satisfied by the Qt Web Engine implementation that has
shipped in commit `738c3d1` instead of the WebKitGTK implementation
that was originally specified.

The corresponding updates to the in-progress change's
`proposal.md`, `design.md`, and `tasks.md` are likewise library-name
corrections, not behavior changes, and are tracked under this
change's `design.md` and `tasks.md` artifacts rather than as spec
deltas.