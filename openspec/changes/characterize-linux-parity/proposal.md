## Why

The `port-to-double-commander-linux` change shipped a working Linux lister for the core text loaders and images, but several Windows-only behaviors were deliberately deferred (per `Readme.md` §Future work and `openspec/changes/port-to-double-commander-linux/proposal.md` §Removed/Future-work). We need a living, per-feature checklist that:

1. Catalogs every Windows behavior that Linux does or could do.
2. Records the current Linux status per feature (works / should work / planned / not planned).
3. Captures the test approach — automated where the shared logic permits it, manual with explicit DC steps where it doesn't.
4. Becomes the source from which we spawn fix-up changes when a feature is found broken or a user-visible regression needs tracking.

The list grows iteratively as new functionality is questioned.

## What Changes

- **New**: capability `linux-parity` — a single living spec that tracks Linux vs Windows feature parity, status per feature, and the test approach. Each row is a small enough unit to spawn a follow-up change when needed.
- **New**: a "test approach" column per row that distinguishes automated (cross-platform logic already covered by `EdgeViewer.Tests` or a Qt-side harness) from manual (DC + native Wayland/X11 verification with explicit steps).
- **No source-tree changes** in this change itself — this is a tracking/characterization change. Fixes live in follow-up changes (e.g., `add-linux-dynamic-dir-thumbs` would add to `specs/directory-view/spec.md`).

## Capabilities

### New Capabilities

- `linux-parity`: umbrella capability listing Windows features, Linux status per feature, and the test approach (automated vs manual). Rows reference the affected processor / C++ source / static asset and the Windows-side capability they correspond to. When a row needs implementation, it spawns its own OpenSpec change that adds a delta spec to the relevant existing capability.

### Modified Capabilities

None at the requirement level — no Windows behavior changes here. Each fix-up change spawned from a row may amend its target capability.

## Impact

- **Docs**: one new spec file under `openspec/changes/characterize-linux-parity/specs/linux-parity/spec.md` and this `proposal.md`.
- **Build**: none.
- **Dependencies**: none.
- **Systems**: no runtime behavior change. Users see no difference.
