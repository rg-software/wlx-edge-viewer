# Tasks

- [x] 1.1 Add `IsNetworkPath()` to `EdgeViewer/Platform.h` and implement it in `Platform_Win.cpp` (UNC `\\server\share\...` and `\\?\UNC\...` detection, robust against extended-length local `\\?\C:\...`) and `Platform_Linux.cpp` (always false).
- [x] 1.2 Remove the UNC → temp-copy branch from `Platform_Win.cpp::GetPhysicalPath`; return the plain `\\server\share\...` path for UNC files.
- [x] 1.3 Keep `GenTempFile`/`gs_tempFiles`/`RemoveTempFiles` (still used by the oversized-loader temp path in `WebView2Backend::NavigateToString`).
- [x] 2.1 `BaseFileProcessor::OpenIn`: on `_WIN32`, when `IsNetworkPath(mPath)`, rewrite `http://local.example` → `evh://local.example` in the generated loader before `NavigateToString`.
- [x] 2.2 `HtmlProcessor::OpenIn` and `OtherProcessor::OpenIn`: on `_WIN32`, navigate network-rooted files through `evh://local.example` (same scheme ForcedHtmlExt already uses).
- [x] 2.3 `WebPolicy::IsLocalUri`: accept the `evh:` scheme alongside `ev:` so OfflineMode/policy treat plugin-hosted scheme content as local.
- [x] 3.1 Add `IsNetworkPath` classification tests to `EdgeViewer.Tests/tier3_paths.cpp` (tag `[t3]`).
- [x] 3.2 Build both Windows platforms (x64 + Win32 Release) and run both test suites.
- [x] 4.1 Update `openspec/specs/temp-file-management/spec.md` and `openspec/specs/plugin-config/spec.md` via delta specs.
- [x] 4.2 Reconcile `openspec/specs/linux-parity/spec.md`, `openspec/notes/rendering-pipeline.md`, `openspec/notes/manual-testing-checklist.md` and `EdgeViewer.Tests/readme.md`.
- [ ] 5.1 Manual verification on a real SMB share (documented in the manual-testing checklist, rows 3.9/3.10): F3 a `.md` with relative images and an `.html` with relative sub-resources; confirm images/CSS/links resolve against the share folder.