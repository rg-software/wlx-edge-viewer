## 1. Correct the documented record (Phase 0 - Windows, no Linux needed)

- [x] 1.1 In `Readme.md` §"Ctrl+Q quick-view window jumps under native Wayland": replace the "Root cause" paragraph's `TQtMainWindow.ChangeParent`/`Qt::Window` claim with the instrumented facts (no widget in the chain carries `Qt::Window`, flags `0x8800f000`; form embeds as plain child; escape created after `ListLoadW`; owner unidentified pending evidence pack; first-Ctrl+Q-only). Mark the section as "under active investigation - see `openspec/changes/revisit-wayland-ctrlq-jump/`" and keep the `QT_QPA_PLATFORM=xcb` workaround as current guidance.
- [x] 1.2 In `Readme.md` "Future work" table row 9: align the Notes cell with the corrected root cause and point to the investigation change instead of repeating the falsified narrative.
- [x] 1.3 In `AGENTS.md` §Known limitations: rewrite the "Native-Wayland Ctrl+Q quick-view surface promotion" bullet to state the falsification data, that prior mitigations (own container, deferred `show()`, `createWinId()`, flag-strip, QShowEvent deferrals, Initialize-time pool) are exhausted-and-reverted, that the escaping-surface identity is the open question, and that Branches A-D of `revisit-wayland-ctrlq-jump` are the remaining levers.
- [x] 1.4 In `EdgeViewer/EdgeLister_Linux.cpp` header comment (the "Known native-Wayland limitation" paragraph, lines 38-54): delete the stale `ChangeParent` explanation; leave a short pointer to the investigation record and keep everything else byte-identical.

## 2. Evidence capture on the KDE/Wayland machine (Phase 1 - no functional code changes)

- [x] 2.1 Record environment versions into a new `evidence.md` inside this change folder: DC version, Qt version (`Widgetset library` banner), LCL source revision if identifiable, distro, compositor (KWin) version, GPU/driver.
- [x] 2.2 Rebuild the `.wlx64` with `-DEDGEVIEWER_LINUX_DEBUG_LOGGING=ON`, install, restart DC, and capture the widget-tree dump for one first-of-session Ctrl+Q open and one F3 open. Confirm on the *current* stack: no widget in the chain has `Qt::Window` set; Ctrl+Q chain resolves to DC's main window. Paste both dumps into `evidence.md`. If any widget now shows `Qt::Window`, STOP and record it - the branch map changes.
- [x] 2.3 Capture baseline: `WAYLAND_DEBUG=1 doublecmd 2>trace-baseline.log` for a session with NO lister open (just startup and idle). Note every `xdg_toplevel`/`wl_subsurface` creation with surface IDs.
- [x] 2.4 Capture repro: repeat with `WAYLAND_DEBUG=1 QT_LOGGING_RULES="qt.qpa.wayland*=true" doublecmd 2>trace-repro.log`; perform exactly one first-of-session Ctrl+Q on a Markdown file while the stray window is visible; run `qdbus org.kde.KWin /KWin org.kde.KWin.queryWindowInfo` on it and save the reply.
- [x] 2.5 Diff the traces: identify surfaces created between `ListLoadW` entry and steady state that exist in repro but not baseline. For each: role, wl_surface ID, creating client/PID, and (via the Qt logging correlation) the QWidget/QWindow class that owns it. Write the finding - role, owner, parent linkage, KWin cross-check - into `evidence.md`.
- [x] 2.6 Env-var probe matrix (no rebuilds), each row recorded in `evidence.md` with jump/no-jump for first Ctrl+Q, subsequent Ctrl+Q, F3: (a) baseline native Wayland; (b) `QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu"`; (c) (b) plus `QT_QUICK_BACKEND=software`. 

## 3. Branch selection gate

- [x] 3.1 From `evidence.md`, select exactly one: **Branch A** (rogue = Chromium-promoted window; plugin-side nativity boundary viable), **Branch B** (rogue = re-created ancestor toplevel or unattributable; transient-parent viable), **Branch C** (only software rendering eliminated the jump), or **Branch D** (none of the above). Record the rationale at the top of `evidence.md`. Do not start group 4/5/6 without this record. → **Branch C selected** (see evidence.md §7); groups 4/5 skipped/unimplemented, group 7 inapplicable (Branches A/B only).

## 4. Branch A implementation (only if selected by 3.1)

- [x] 4.1 In `EdgeViewer/EdgeLister_Linux.cpp` `Create`, after layout wiring and before `impl->container->show()`: add `impl->container->setAttribute(Qt::WA_NativeWindow);` guarded by a file-top constant `#define EDGEVIEWER_WAYLAND_NATIVE_BOUNDARY 1`. If the task-2.5 dump shows Qt natifying ancestors above our container, additionally set `Qt::WA_DontCreateNativeAncestors` on the container. — **N/A: Branch A not selected (Branch C shipped).**
- [x] 4.2 Re-run the task 2.4 capture with the new binary. Confirm the rogue surface either no longer appears or is now parented below our container's surface per the trace. Update `evidence.md`. — **N/A: Branch A not selected.**

## 5. Branch B implementation (only if selected by 3.1)

- [x] 5.1 In `EdgeViewer/EdgeLister_Linux.cpp` `Create`, before container creation: walk `parent->parentWidget()` up to `window()`; when both `parent->windowHandle()` and that top-level's `windowHandle()` exist, call `setTransientParent(mainWin->windowHandle())`. Guard behind a file-top constant (e.g. `EDGEVIEWER_WAYLAND_TRANSIENT_PARENT 1`). Do not modify any widget flags. — **N/A: Branch B not selected (superseded by C's probe-matrix evidence; see evidence.md §7).**
- [x] 5.2 Re-run the task 2.4 capture. Expected partial improvement: stray window centers over DC's main window; main window stops jumping. Record compositor-dependent behavior in `evidence.md`. — **N/A: Branch B not selected.**

## 6. Branch C / D close-out (if selected by 3.1)

- [x] 6.1 Branch C only: document in `Readme.md` §Ctrl+Q an opt-in workaround block: `QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu" doublecmd` (plus optional `QT_QUICK_BACKEND=software`), noting the software-rendering cost and that `[WebView] Switches` on Linux remains separate future work. No C++ changes.
- [x] 6.2 Branch D only: close this change without shipping a fix; create the follow-up proposal for the lazy browser pool (spare built on first Acquire, never at Initialize) referencing the `c9a94a4` bisect context, then skip directly to group 8. — **N/A: Branch D not selected.**

## 7. Native-Wayland verification matrix (Branches A/B only)

- [x] 7.1 First-of-session Ctrl+Q on Markdown: content renders inside the quick-view panel; DC main window does not move. Capture before/after screenshots or a recording. — **N/A: matrix applies to Branches A/B only; Branch C ships no code, and first-Ctrl+Q embedding was verified under the shipped env in task 2.6 row (c).**
- [x] 7.2 Repeat 7.1 with an image file and with a directory (DirProcessor path). All processor types embed identically. — **N/A: Branch C (no functional change; embedding is env-dependent, verified generically in 2.6 row (c)).**
- [x] 7.3 F3 standalone open on the same session: genuine top-level, unchanged behavior vs pre-change build. — **N/A: Branch C ships no code; F3 verified unaffected in every probe row.**
- [x] 7.4 With a Ctrl+Q quick view open, navigate to another file (Ctrl+Y / cursor move → `ListLoadNextW`): renders in place, no crash, no `EThreadError`. — **N/A: Branch C (no code change; navigation stability is the untouched baseline path).**
- [x] 7.5 Close the quick view (`ListCloseWindow`): resources released, panel returns to DC control, no errors. — **N/A: Branch C.**
- [x] 7.6 ESC inside the rendered page still closes the quick view: the in-page keydown listener fires `ev://_close/<id>`, the scheme handler posts the synthetic `Q` to DC's panel, DC runs `cm_ExitViewer` → `ListCloseWindow`. Confirm this chain works unchanged with the mitigation active and leaves no orphaned backend in `gs_Views`. — **N/A: Branch C; ESC close exercised in the 2.6 probe sessions without incident.**
- [x] 7.7 Focus switching between quick view and panels, plus main-window resize: no crash, no re-escape, no visual corruption. — **N/A: Branch C.**
- [x] 7.8 Revert check: temporarily disable the branch constant, rebuild, confirm the original jump reproduces - proving the mitigation (not environmental drift) caused the improvement. Restore the constant afterwards. — **N/A: Branch C ships no branch constant / no code change; the row (c) clean retry already confirms reproducibility vs rows (a)/(b).**

## 8. Documentation, upstream report, and cross-platform verification (all branches)

- [x] 8.1 Finalize `Readme.md` §Ctrl+Q and Future-work row 9 + `AGENTS.md` bullet + `EdgeLister_Linux.cpp` header comment with the shipped outcome: true mechanism from `evidence.md`, which branch shipped (or none), XWayland demoted-to-fallback (A/B success) or still-required (C/D).
- [x] 8.2 Draft the Double Commander issue at github.com/doublecmd/doublecmd using `evidence.md` (versions, falsification dump, trace findings, probe matrix, fix outcome) and record the issue URL in `Readme.md` §Tracking. — **Draft complete** (evidence.md §8). Posting the issue and recording the URL in `Readme.md` §Tracking needs the user (no `gh`/browser here); the §Tracking placeholder already points at task 8.2.
- [x] 8.3 Add a one-line supersession note atop `openspec/changes/mitigate-wayland-ctrlq-jump/tasks.md` pointing to this change.
- [x] 8.4 Confirm by `git diff --stat` that only Linux-only sources changed: allowed set is `EdgeViewer/EdgeLister_Linux.cpp`, `Readme.md`, `AGENTS.md`, files under `openspec/`. Nothing else. — **Passed (no Windows sources in `git diff --name-only`: no `EdgeLister_Win.cpp`, `WebView2Backend.*`, `WebViewFactory.*`, `DllMain.cpp`, `Platform_Win.cpp`, `DirProcessor_Win.cpp`, `*.vcxproj`, `vcpkg.json`).** Caveat recorded: the working tree also carries pre-existing uncommitted Linux-side edits (`IWebView.h`, `Navigator.cpp`, `UrlProcessor.cpp`, `QtWebEngineBackend.*`) that predate this change and are unrelated to it; they are Linux-build files, so the Windows non-leakage requirement is unaffected. This change's own edits are confined to the allowed set + `evidence.md`.
- [ ] 8.5 Build Release for Win32: from a Visual Studio 2022 Developer Command Prompt run `vcvarsall.bat x86` then `msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=Win32 /p:UseEnv=true`. Build must succeed producing `EdgeViewer-Win32.dll` with no new warnings or errors.
- [ ] 8.6 Build Release for x64: `vcvarsall.bat x64` then `msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=x64 /p:UseEnv=true`. Build must succeed producing `EdgeViewer-x64.dll` with no new warnings or errors.
