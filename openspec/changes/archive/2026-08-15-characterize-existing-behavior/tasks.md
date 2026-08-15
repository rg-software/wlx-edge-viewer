## 1. Per-file-type spec files

- [x] 1.1 Write `specs/markdown/spec.md` (4 requirements: detection, rendering via loader template, CSS theme, virtual host mapping)
- [x] 1.2 Write `specs/asciidoc/spec.md` (4 requirements: detection, rendering via loader template, CSS theme with NO CSSDark quirk, virtual host mapping)
- [x] 1.3 Write `specs/rst/spec.md` (4 requirements: detection, rendering via loader template, CSS theme with CSSDark, virtual host mapping)
- [x] 1.4 Write `specs/html/spec.md` (8 requirements: detection, default Navigate to local.example, encoding override to html.example, BOM detection, meta charset detection, CSS injection, CSS theme, virtual host mapping)
- [x] 1.5 Write `specs/mhtml/spec.md` (3 requirements: detection, rendering via mhtml2html loader, virtual host mapping)
- [x] 1.6 Write `specs/url-files/spec.md` (5 requirements: detection, URL= parsing, local file:/// dispatch, external URL Navigate, virtual host mapping)
- [x] 1.7 Write `specs/images/spec.md` (5 requirements: detection, rendering via thumbnail-viewer loader, FitToScreen mode, CSS theme, virtual host mapping)
- [x] 1.8 Write `specs/directory-view/spec.md` (11 requirements: detection, listing generation/sort, dynamic shell thumbnails, image file display, other file display, hidden files, CSS theme, view config options, virtual host mapping, right-click menu, UNC limitation)
- [x] 1.9 Write `specs/other-fallback/spec.md` (4 requirements: detection, Navigate to local.example, PDF via built-in viewer, virtual host mapping)

## 2. Cross-cutting infrastructure spec files

- [x] 2.1 Write `specs/wlx-contract/spec.md` (10 requirements: export set, file loading, file navigation, window closing, detect string generation, search dispatch, print dispatch, ANSI wrapper parity, no-op exports, 32/64-bit export parity)
- [x] 2.2 Write `specs/virtual-host-mapping/spec.md` (4 requirements: assets.example mapping, local.example mapping, every processor maps both, mapped folders bypass request interception)
- [x] 2.3 Write `specs/plugin-config/spec.md` (9 requirements: ini location, lazy loading, [Extensions], [Chromium] keys, per-type CSS sections, [Directory] keys, ForcedHtmlExt, UserDir default, 32/64-bit parity)
- [x] 2.4 Write `specs/dark-mode/spec.md` (5 requirements: flag source, CSS selection per processor with AsciiDoc exception, web engine color scheme, global per-file flag, 32/64-bit parity)
- [x] 2.5 Write `specs/zoom-control/spec.md` (6 requirements: zoom hotkeys, discrete step table, KeepZoom persistence, per-processor-type tracking, no parent relay, 32/64-bit parity)
- [x] 2.6 Write `specs/text-search/spec.md` (5 requirements: dispatch chain, parameter flags, find-first behavior, ANSI wrapper parity, 32/64-bit parity)
- [x] 2.7 Write `specs/print/spec.md` (4 requirements: dispatch chain, TC params ignored, ANSI wrapper parity, 32/64-bit parity)
- [x] 2.8 Write `specs/accelerator-keys/spec.md` (6 requirements: browser hotkeys disabled, key relay to parent, Q close, 1-8 tab switch, zoom hotkeys not relayed, 32/64-bit parity)
- [x] 2.9 Write `specs/temp-file-management/spec.md` (8 requirements: symlink resolution, UNC temp-copy, ForcedHtmlExt temp-copy, temp file generation, temp cleanup, EBWebView cleanup, path prefix stripping, 32/64-bit parity)
- [x] 2.10 Write `specs/popup-context-menu/spec.md` (5 requirements: invocation from directory viewer, shell menu content, command execution, COM/PIDL lifetime, 32/64-bit parity)

## 3. Design and tasks artifacts

- [x] 3.1 Write `design.md` (capability decomposition rationale: per-file-type + cross-cutting, no code changes, spec-only documentation)
- [x] 3.2 Write `tasks.md` (this file)

## 4. Validate and archive

- [ ] 4.1 Run `openspec validate characterize-existing-behavior --strict` and resolve any remaining issues (all requirements SHALL contain SHALL or MUST).
- [ ] 4.2 Verify no source code was modified: `git diff master -- EdgeViewer/ Resources/ vcpkg.json EdgeViewer.sln EdgeViewer.vcxproj BuildMakeSetup.bat` SHALL show no changes outside `openspec/`.
- [ ] 4.3 Verify the Windows build still produces identical binaries: build Release|x64 and Release|Win32 via `BuildMakeSetup.bat` (or `vcvarsall.bat x86 && msbuild ... /p:UseEnv=true` then repeat for x64) and confirm both DLLs are byte-identical or behaviorally identical to the master build (the spec files under `openspec/` MUST NOT affect compilation).
- [ ] 4.4 Archive the change with `openspec archive characterize-existing-behavior` to promote all 18 delta specs to `openspec/specs/<capability>/spec.md`.