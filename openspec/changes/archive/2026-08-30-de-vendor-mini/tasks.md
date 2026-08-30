## 1. Windows: source mINI through the vcpkg `pulzed-mini` port

- [x] 1.1 Add `pulzed-mini` to the `dependencies` array in `vcpkg.json` (no version entry — the committed `builtin-baseline` already pins it to 0.9.14). Run an MSBuild invocation and confirm vcpkg installs the port (header lands in `vcpkg_installed/<triplet>/include/mini/ini.h`).
- [x] 1.2 In `EdgeViewer/EdgeViewer.vcxproj`, remove `$(SolutionDir)mINI\src\;` from all four `AdditionalIncludeDirectories` entries (Win32/x64 × Debug/Release). The vcpkg integration supplies the include dir, matching how `<webview2.h>` resolves today.
- [x] 1.3 In `EdgeViewer.Tests/EdgeViewer.Tests.vcxproj`, remove `$(ProjectDir)..\mINI\src\;` from all four `AdditionalIncludeDirectories` entries.
- [x] 1.4 Build Release for both platforms (BuildMakeSetup.bat workflow or `vcvarsall.bat x86|x64 && msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=... /p:UseEnv=true`); confirm `<mini/ini.h>` resolves through vcpkg with no manual path entry and both `EdgeViewer.wlx`/`EdgeViewer.wlx64` produce.

## 2. Linux: fetch the pinned mINI header in CMake

- [x] 2.1 In `CMakeLists.txt`, define `set(MINI_COMMIT a1ff72e8898db8b53282e9eb7c7ec5973519787e)` (upstream 0.9.14 tag) and add a configure-time `file(DOWNLOAD "https://raw.githubusercontent.com/metayeti/mINI/${MINI_COMMIT}/src/mini/ini.h" "${CMAKE_CURRENT_BINARY_DIR}/mini/mini/ini.h" EXPECTED_HASH SHA256=1396DB49DD4EEC37E5556341989AAEB6256FC44349D7BCB8F3E1581F277C44A3 TLS_VERIFY ON)` that checks the returned `STATUS` and fails with a clear message on error.
- [x] 2.2 Replace `"${CMAKE_CURRENT_SOURCE_DIR}/mINI/src"` in `target_include_directories(EdgeViewer ...)` with `"${CMAKE_CURRENT_BINARY_DIR}/mini"` so `#include <mini/ini.h>` resolves from the downloaded header.
- [x] 2.3 Re-configure and build: `cmake -B build -S . && cmake --build build -j` → produces `build/EdgeViewer.wlx64` (fresh dir and existing dirty dir both work).

## 3. Delete the vendored tree

- [x] 3.1 `git rm -r mINI`.
- [x] 3.2 `git grep -n mINI` across the tree; after updating docs (task 4) the only remaining hits SHALL be the `#include <mini/ini.h>` includes in `EdgeViewer/` and `EdgeViewer.Tests/` and deliberate doc statements. Fix anything stale (scripts, Readme.md naming `mINI/`).

## 4. Documentation

- [x] 4.1 Update `AGENTS.md` Windows section: mINI now ships via the `pulzed-mini` vcpkg port (maintained against the `pulzed/mINI` sync-fork of `metayeti/mINI`), header-only, version locked by the existing `builtin-baseline` at 0.9.14 — identical content to the former vendored copy.
- [x] 4.2 Update `AGENTS.md` Linux notes: mINI is fetched at configure time by CMake from upstream commit `a1ff72e` (0.9.14, `EXPECTED_HASH` pinned) into the build dir — no vendored copy in the source tree; record the invariant that a future mINI bump must move both pin sites (vcpkg baseline and `MINI_COMMIT`) in lockstep.

## 5. Verify

- [x] 5.1 Build Release both platforms: Win32 and x64 DLLs build clean with `mINI/` deleted; `BuildMakeSetup.bat` packages Release zip successfully.
- [x] 5.2 Build and run the test suite both platforms (`msbuild EdgeViewer.Tests/EdgeViewer.Tests.vcxproj /p:Configuration=Release /p:Platform=Win32` and `/p:Platform=x64`, then run each `EdgeViewer.Tests.exe`): 45 tests / 186 assertions pass.
- [x] 5.3 `build_win32.log`/`build_x64.log`-style full rebuilds of both Win32 and x64 (Release) succeed, plus `cmake --build build -j` on Linux → `EdgeViewer.wlx64`; `git status` shows only `vcpkg.json`, the two `.vcxproj` files, `CMakeLists.txt`, `AGENTS.md`, and the `mINI/` deletion — no `EdgeViewer/*.cpp|*.h` or `Resources/assets/` edits introduced by this change.
- [x] 5.4 Manual smoke in Total Commander (32- and 64-bit): open a Markdown sample from `Examples/` and a directory view; the detect string / extension behavior via `edgeviewer.ini` is unchanged from the vendored build.
