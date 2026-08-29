## Why

The project vendors a full upstream checkout of mINI (v0.9.14, commit `d667e2c`) even though only a single header (`mINI/src/mini/ini.h`) is consumed. mINI is now available as a pinned dependency on both build systems — vcpkg (Windows) and CMake `FetchContent` (Linux) — so the vendored subtree (its test suite, README, CHANGELOG, and dead include-directory entries in two `.vcxproj` files) can be deleted, eliminating a redundant ~40-file copy that must otherwise be manually tracked against upstream releases.

## What Changes

- **Windows (vcpkg):** add `pulzed-mini` to `vcpkg.json` dependencies. The port installs the same `include/mini/ini.h`. The existing `builtin-baseline` already resolves it to **0.9.14** — the exact version the project vendors (verified byte-identical, CRLF-normalized) — so parsing behavior is unchanged with no baseline bump.
- Remove the vendored `mINI/` directory **BREAKING** (build-time only): the tree is deleted from the repo; any out-of-tree build that referenced `$(SolutionDir)mINI\src\` or `mINI/src` directly must stop doing so.
- **Windows project files:** drop `$(SolutionDir)mINI\src\` / `$(ProjectDir)..\mINI\src\` from `AdditionalIncludeDirectories` in `EdgeViewer.vcxproj` and `EdgeViewer.Tests.vcxproj` (the MSBuild vcpkg integration injects the installed include dir automatically).
- **Linux (CMake):** replace the `mINI/src` include-directory entry in `CMakeLists.txt` with `FetchContent` pinned to the upstream **0.9.14** commit (`a1ff72e8898db8b53282e9eb7c7ec5973519787e`), copying `src/mini/ini.h` into the build tree — same content both platforms compile today, no Linux version bump.
- **Docs:** update `AGENTS.md` (dependency sourcing: Windows vcpkg `pulzed-mini`, Linux FetchContent) and any other docs that reference the vendored `mINI/` path.
- No C++ source changes; no `Resources/assets/` changes; no config-key changes. `#include <mini/ini.h>` remains the include form everywhere.

## Capabilities

### New Capabilities

None — no externally observable plugin behavior changes; the parsed INI bytes and mINI semantics are identical.

### Modified Capabilities

None — `plugin-config` still reads `edgeviewer.ini` with the same mINI parser (same library, same version); `linux-runtime` still forbids vcpkg/WebView2/WIL on Linux (`FindContent` shipping a header is neither). This is a pure build-infrastructure change, so `.openspec.yaml` declares `skip_specs: true`.

## Impact

- **Build files:** `vcpkg.json` (add `pulzed-mini`); `CMakeLists.txt` (FetchContent + target invocation); `EdgeViewer.vcxproj` / `EdgeViewer.Tests.vcxproj` (4 include-dir entries each).
- **Deleted:** `mINI/**` (header tree vendored since 2022-10-11, `git rm`).
- **Dependencies:** Windows gains the `pulzed-mini` vcpkg port (header-only, version-locked by the pinned baseline); Linux gains a one-time network fetch at CMake configure time (pinned commit, cached under the build dir).
- **Docs:** `AGENTS.md`; any open-spec note that names the `mINI/` path.
- **Verification:** Release builds both Win32 and x64 plus `EdgeViewer.Tests.exe` both platforms (45 tests / 186 assertions); Linux `cmake -B build -S . && cmake --build build -j` produces `EdgeViewer.wlx64`.