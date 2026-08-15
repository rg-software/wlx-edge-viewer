## Context

See `proposal.md` for motivation. The project has ~17 functional areas; only `eml` has a spec (`openspec/specs/eml/spec.md`). This change captures the rest as baseline capability specs with no code changes — pure documentation derived from reading `EdgeViewer/*.cpp`, `EdgeViewer/Processors/*.cpp`, `WebView2.cpp`, `EdgeLister.cpp`, `Globals.cpp`, and `Resources/edgeviewer.ini`.

## Goals / Non-Goals

**Goals:**

- Baseline specs for all functional areas so the upcoming `port-to-double-commander-linux` change can delta against them.
- Each spec describes *current observed behavior* — not idealized, not aspirational.
- Specs follow the same format as `openspec/specs/eml/spec.md` (Purpose + Requirements + Scenarios).
- Each scenario is a potential manual test case (the project has no test framework today).

**Non-Goals:**

- No code changes, no build changes, no new dependencies.
- No tests yet — unit/characterization tests require the `IWebView` abstraction the port introduces.
- No spec for `eml` — it already has one (`openspec/specs/eml/spec.md`).
- No redesign or behavior change — this is pure characterization.

## Decisions

### Decision 1: Capability decomposition — per-file-type + cross-cutting

Capabilities are split into two groups:

- **Per-file-type** (9 capabilities: `markdown`, `asciidoc`, `rst`, `html`, `mhtml`, `url-files`, `images`, `directory-view`, `other-fallback`). Each mirrors one processor class in `EdgeViewer/Processors/`. This follows the existing `eml` spec's pattern and gives each file type an independent spec.
- **Cross-cutting** (9 capabilities: `wlx-contract`, `virtual-host-mapping`, `plugin-config`, `dark-mode`, `zoom-control`, `text-search`, `print`, `accelerator-keys`, `temp-file-management`, `popup-context-menu`). These are behaviors shared across multiple processors or belonging to the plugin infrastructure layer.

**Alternative:** one giant spec per broad area (e.g., `rendering`, `infrastructure`). Rejected: the port change needs per-capability delta targets; a single monolithic spec would force the port to modify a large file for each behavioral change, making review harder.

**Note:** the count is 18 new capabilities (not 9+9=18; the cross-cutting list has 10 items but `popup-context-menu` is collapsed into the directory-view-adjacent group, giving 9 cross-cutting + 9 per-file-type = 18). See `proposal.md` for the exact list.

### Decision 2: Specs describe behavior, not implementation

Following the OpenSpec instruction: specs capture *observable behavior*, not class names or library choices. E.g., the `markdown` spec says "the loader HTML template is filled with the file's base URL, CSS filename, and filename, then loaded as a string into the web view" — it does not say "MdProcessor calls NavigateToString with a regex-replaced loader.html." This keeps specs portable across the port's `IWebView` refactor.

Where implementation is essential to understand behavior (e.g., the `DetectEncoding` path dispatches between `local.example` and `html.example` domains, which is observable in which resources are loaded), the spec mentions the *observable* aspect (which domain is used) without naming the C++ class.

### Decision 3: No vcpkg.json or dependency changes

This change is pure OpenSpec documentation. No C++, no static assets (`Resources/assets/`), no `vcpkg.json` changes — meeting the design rules trivially.

### Decision 4: Spec files live under the change directory until archived

All 18 new spec files are written to `openspec/changes/characterize-existing-behavior/specs/<capability>/spec.md` as `## ADDED Requirements`. Upon `openspec archive`, they are promoted to `openspec/specs/<capability>/spec.md`.

## Risks / Trade-offs

**[Risk] Specs may drift from actual behavior** if the code changes between characterization and port. Low risk because these are baseline specs and the port branch is parked; master isn't expected to change significantly.
→ Mitigation: Review specs against source during archive.

**[Risk] Behavior is inferred from reading source, not from black-box testing.** There's no test suite to verify spec claims empirically.
→ Mitigation: Each spec includes scenarios that can be manually validated by loading the plugin in TC with `Examples/` files. The port's verify step re-checks these.

**[Trade-off] 18 spec files is a large surface.** But each is small (1-4 requirements typically) and focused. The alternative — fewer, larger specs — would reduce delta precision for the port.