## Context

The Ctrl+Q native-Wayland jump survived three plugin-side mitigations (closed in `openspec/changes/mitigate-wayland-ctrlq-jump/tasks.md`), and the instrumentation run during that investigation falsified the documented root cause: on the target build (CachyOS / KDE Wayland / DC 1.2.8 / Qt 6.11 / LCL-Qt6) *no* widget in the chain DC hands to `ListLoadW` carries `Qt::Window` (flags `0x8800f000`, Window-type mask `0x1ff` = 0); the viewer form embeds as a plain child (`QAbstractScrollArea`-wrapped) and `parent->window()` resolves to DC's main window. The escaping surface is therefore created *after* `ListLoadW` - most plausibly by Chromium's compositor/QQuickWidget internals when the view is first shown against the freshly reparented, not-yet-realized LCL tree - but its identity has never been observed at runtime. Observed facts that constrain any design: the jump is **first-Ctrl+Q-of-session only**; subsequent opens embed cleanly; F3 is unaffected; the timing-deferral experiments (defer `Navigator::Open` and/or `container->show()` to the parent's `QShowEvent`) fired correctly per logs yet did not help, so ordering alone does not fix it; `j2969719/doublecmd-plugins/wlx/qtpdfview_qt`, which uses the identical container pattern with a raster-painting `QPdfView`, is immune.

Code state today: `EdgeViewer/EdgeLister_Linux.cpp` is back on the navigation-stable inline path (`container->show()` + `Navigator::Open` inside `Create`), retains the optional `EDGEVIEWER_LINUX_DEBUG` widget-tree dump (`-DEDGEVIEWER_LINUX_DEBUG_LOGGING=ON`), and its header comment still teaches the falsified `ChangeParent` story. All changes in this change belong in C++ under `EdgeViewer/` (one Linux-only file plus documentation) - **nothing** under `Resources/assets/`; no dependency is involved, so **`vcpkg.json` is untouched**.

## Goals / Non-Goals

**Goals:**
- Identify the escaping surface by role, owner, and parent linkage on real hardware before writing any functional code.
- Ship at most one evidence-selected mitigation; record why the others were rejected.
- Correct the public record (`Readme.md`, `AGENTS.md`, source header comment) to match instrumented reality.
- Preserve navigation stability (no `EThreadError`) and the F3 path exactly as they are today.
- Produce the upstream Double Commander issue text from the collected evidence.

**Non-Goals:**
- Fixing DC or LCL upstream - this change only mitigates plugin-side and files the report.
- Re-introducing `[WebView] Switches` engine flags on Linux as a shipped feature (separate future change if Branch C wins).
- The pre-created browser pool as an in-change fallback (Branch D is a spin-off decision only).
- Any behavior change on Windows, any processor or asset change, any ini key change.

## Decisions

### Decision 1: Evidence gates implementation; exactly one branch ships
No functional commit lands until the Phase-1 evidence names the rogue surface. The four branches are mutually exclusive and selected by what the surface turns out to be:
- **Branch A** - rogue surface is Chromium's own promoted window/compositor surface: stop the promotion at our boundary by making the plugin container the nearest native widget (`Qt::WA_NativeWindow` on the container, `Qt::WA_DontCreateNativeAncestors` where needed), so Chromium finds a native parent *below* DC's widgets.
- **Branch B** - rogue surface is a re-created ancestor top-level: execute the designed-but-never-run `QWindow::setTransientParent(mainWin->windowHandle())` fallback (design Decision 3 of `mitigate-wayland-ctrlq-jump`, verbatim).
- **Branch C** - only software rendering kills the jump (`QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu"` probe): ship documentation of the env-var workaround; do not silently re-add engine switches (AGENTS.md forbids ad-hoc reintroduction; `[WebView] Switches` on Linux stays separate future work).
- **Branch D** - all above fail: close without shipping; spin off the lazy browser pool (spare built on first *Acquire*, not Initialize - the `c9a94a4` bisect step never completed) as its own change.
*Alternative considered:* implement several branches behind an ini toggle. Rejected: determinism requirement from the prior spec (upstream reports must describe one behavior) and the prior change's explicit rejection of runtime toggles.

### Decision 2: Diagnose via existing tooling, zero plugin-code changes in Phase 1
Phase 1 uses only: (a) the already-present `EDGEVIEWER_LINUX_DEBUG` tree dump re-run on the current build to revalidate the falsification; (b) `WAYLAND_DEBUG=1 doublecmd 2>trace.log` filtered around the first `xdg_toplevel`/`wl_subsurface` creations correlated with `QT_LOGGING_RULES="qt.qpa.wayland*=true"` (maps QWindows to wl_surfaces); (c) KWin `org.kde.KWin.queryWindowInfo` over D-Bus while the stray window is visible (PID, resource class). 
*Alternatives considered:* adding permanent runtime logging of Chromium internals - rejected as invasive and unnecessary when the protocol trace already answers "who created the surface"; using `xev`-style X tools - N/A on native Wayland.

### Decision 3: Branch A mechanics (if selected)
In `EdgeLister::Create`, after building the container and before `show()`: `impl->container->setAttribute(Qt::WA_NativeWindow);`. Rationale: Chromium's delegated-compositing path requests a native window handle; when the ancestor chain has no consistent native window at that moment, Qt creates one whose parentage resolves wrong (the promotion we observe). Giving our own plain container nativity first gives Chromium a correct, plugin-owned native parent without touching DC's widgets. `WA_DontCreateNativeAncestors` is applied only if the dump shows Qt natifying ancestors beyond our container. Selected behind `#define EDGEVIEWER_WAYLAND_NATIVE_BOUNDARY 1` (compile-time constant, single point of flip per the determinism rule).
*Alternative:* `createWinId()` on the parent - tried during the earlier investigation era, did not resolve; also touches DC widgets, unlike our own container.

### Decision 4: Branch B mechanics (if selected)
Unchanged from the prior design's fallback: walk `parent->parentWidget()` up to `window()` (DC's real main window) and call `parent->windowHandle()->setTransientParent(mainWin->windowHandle())` when both handles exist. Outcome is partial by design (compositor centers the escaped form over the main window instead of screen center; main window stops jumping) - acceptable if the evidence shows the escapee cannot be eliminated plugin-side.

### Decision 5: Documentation corrections are part of the same change
Three sites carry the falsified narrative and are corrected together so the record cannot diverge again: `Readme.md` §"Ctrl+Q quick-view window jumps under native Wayland" + Future-work row 9; `AGENTS.md` §Known-limitations bullet; `EdgeLister_Linux.cpp` lines ~22-36 header comment. New text states: instrumented facts, the evidence-pack location, which branch shipped, and XWayland's demoted status (fallback rather than requirement) - or unchanged status if nothing shipped.
*Alternative:* separate docs-only change first. Rejected: user chose one change; splitting invites the docs drifting further while diagnosis runs.

### Decision 6: Guardrails encoded in review, not code
From the closed investigation: (1) no cancel-then-synchronously-fire patterns around `Navigator::Open` in `OpenIn` (caused `EThreadError` racing Chromium worker cleanup); (2) nothing web-engine-related constructed during plugin Initialize against unmapped windows (caused F3 blank); (3) the inline `container->show()` + `Navigator::Open` sequence remains the untouched fallback whenever an experiment fails. These are enforced by the task list requiring each experiment to revert cleanly to the baseline path before the next attempt.

A fourth invariant from the current tree: the ESC-close bridge (`ev://_close/<id>` scheme handler posting a synthetic `Q` to DC's panel) and the deliberate no-delete close semantics of Linux `ListCloseWindow` (`DllMain.cpp` calls backend `Close()` + erases `gs_Views`, leaving container destruction to Qt ownership under DC's form) are load-bearing. Mitigations on Branches A/B must not touch either path; the verification matrix exercises ESC explicitly.

## Risks / Trade-offs

- **[Risk] The trace cannot attribute the surface** (Chromium subprocesses create their own surfaces; PID differs). → Mitigation: correlate creation order with `qt.qpa.wayland*` logging in-process; KWin query gives the window's PID even across processes; if attribution stays ambiguous, Branch B (transient-parent) is chosen as the lowest-risk lever since it works regardless of who owns the escapee.
- **[Risk] Branch A makes things worse** (a native child subsurface could itself be misplaced by some compositors). → Mitigation: A is compile-time-gated and verified against the full matrix (first/subsequent Ctrl+Q, F3, Ctrl+Y-in-Ctrl+Q, close) before acceptance; reverting is deleting the attribute line.
- **[Risk] DC updates change the picture** (new DC/LCL may alter embedding). → Mitigation: task 1 revalidates the falsification data on the current build first; evidence pack records exact versions.
- **[Risk] Software rendering (Branch C) costs GPU acceleration for all users just to fix quick view.** → Mitigation: C ships as *documentation* only, framed as opt-in env var for affected users; no default change.
- **[Trade-off] One more investigation round on a problem twice attacked.** Justified because every prior attempt guessed at the mechanism; this is the first pass that observes the failing component directly.

## Migration Plan

1. Land doc corrections + evidence pack (Phase 0-1 artifacts) - deployable immediately, no behavior change.
2. If A or B ships: rebuild `.wlx64`, replace installed plugin, restart DC; verify matrix; then update docs with shipped-branch status and file the upstream issue.
3. Rollback: revert the `.wlx64` to the previous binary (single-file deployment; no config or data migration). If nothing ships, rollback is a no-op and XWayland guidance stands.

## Open Questions

- Does the current KDE/Wayland machine still run DC 1.2.8 / Qt 6.11, or has the stack moved? Deferrable: answered by task group 1's version capture; changes only the evidence pack contents.
- Which KWin/compositor version, for the upstream report? Deferrable: captured alongside the trace.
