## Context

See proposal.md — Why. Current state: `mINI/` is a vendored upstream subtree at v0.9.14 added 2022-10-11 (`d667e2c`) with no local modifications. Only `mINI/src/mini/ini.h` is consumed; it is included as `<mini/ini.h>` from `EdgeViewer/Globals.h`, `EdgeViewer/WlxDetect.h`/`WlxDetect.cpp`, `EdgeViewer/Globals.cpp`, and the test-side `EdgeViewer.Tests/pch.h`, `TestHelpers/IniBuilder.h`. The include path is wired into `EdgeViewer.vcxproj` + `EdgeViewer.Tests.vcxproj` (4 configs each) and `CMakeLists.txt:79`.

Constraint that shapes the whole design: the pinned vcpkg baseline `e9c53cd6...` already resolves the `pulzed-mini` port to **0.9.14**, and I byte-verified (CRLF-normalized SHA256) that the vendored `ini.h` is identical to the 0.9.14 tag content (`1396DB49...`). So Windows can go dependency-based with zero code-behavior delta and no baseline bump.

## Goals / Non-Goals

**Goals:**
- Delete every file under `mINI/` from the repo while keeping plugin behavior byte-identical.
- Source mINI on Windows via vcpkg and on Linux via CMake from a pinned upstream commit, both resolving to the exact 0.9.14 content used today.
- Leave `#include <mini/ini.h>` and the `mINI::` API usage untouched in C++.

**Non-Goals:**
- No version bump of mINI on either platform (0.9.14 stays 0.9.14; Windows and Linux stay content-identical).
- No changes to `openspec/specs/*` — this is build infrastructure, behavior is unchanged (`skip_specs: true`).
- No migration of the Linux backend off Qt or addition of vcpkg on Linux (still forbidden by `linux-runtime`).
- No C++ (`EdgeViewer/`) or static-asset (`Resources/assets/`) edits beyond those already uncommitted in the working tree and unrelated to this change.

## Decisions

### 1. Windows: add the `pulzed-mini` vcpkg port, no version override

`vcpkg.json` gains `"pulzed-mini"` under `dependencies`. The port is the vcpkg registry's port for this exact library (maintained against the `pulzed/mINI` sync-fork, whose codebase is the upstream metayeti/mINI; the port's 0.9.14 tag content hashes identical to upstream 0.9.14). The pinned `builtin-baseline` resolves it to 0.9.14 with zero intervention, matching what is vendored today. The port installs header-only to `include/mini/ini.h`, and the MSBuild vcpkg integration already injects the installed include dir (the same mechanism that resolves `<webview2.h>`/`<wil/com.h>` today with no manual path entries).

- Then drop the 4 `$(SolutionDir)mINI\src\` / `..\mINI\src\` entries from `EdgeViewer.vcxproj` and `EdgeViewer.Tests.vcxproj`.
- Alternatives rejected: bumping the `builtin-baseline` to get the newer 0.9.17 port version (unneeded version delta on Windows; would also move `webview2`/`wil`/`catch2` and is out of scope); vendoring a single header only on Windows (pure build hygiene — vcpkg is the mandated dependency channel).

### 2. Linux: `file(DOWNLOAD)` a single header pinned to the upstream 0.9.14 commit

The upstream 0.9.14 tag (`a1ff72e8898db8b53282e9eb7c7ec5973519787e`) predates upstream's `CMakeLists.txt` (added in 0.9.17), so there is no `mINI` CMake target at 0.9.14 to consume. Instead of bumping the library, CMake downloads the one file at configure time:

```cmake
file(DOWNLOAD
    "https://raw.githubusercontent.com/metayeti/mINI/${MINI_COMMIT}/src/mini/ini.h"
    "${CMAKE_CURRENT_BINARY_DIR}/mini/mini/ini.h"
    EXPECTED_HASH SHA256=1396DB49DD4EEC37E5556341989AAEB6256FC44349D7BCB8F3E1581F277C44A3
    TLS_VERIFY ON)
```

replacing `"${CMAKE_CURRENT_SOURCE_DIR}/mINI/src"` in `target_include_directories` with the `${CMAKE_CURRENT_BINARY_DIR}/mini` include dir. `1396DB49...` is the SHA256 of the LF line-ending upstream file (verified identical to the vendored copy modulo CRLF). Downloading into the binary dir keeps the source tree vendor-free; GNUMake/Ninja re-run the configure step when the file is absent so a stale build dir still works, and a fresh checkout needs network only once per build dir.

- Alternatives rejected: `FetchContent` with `URL` on the raw file — heavier machinery for a single header (no archive to extract); git submodule — re-introduces a quasi-vendored clone plus update surface, contradicts the "pinned deps, no vendoring" convention; system package (AUR `cpp-mini` is Arch-only, absent from Debian/Ubuntu/Fedora main repos). A later upstream version with the `INTERFACE` `mINI` target (0.9.17+) was rejected because it forces a cross-platform version divergence unless the Windows baseline is also moved, which Decision 1 explicitly avoids.

### 3. Version-alignment invariant between platforms

Both platforms must continue compiling the identical `ini.h` bytes. Windows pins it via the vcpkg baseline (0.9.14); Linux pins via the commit SHA + `EXPECTED_HASH`. If one is ever bumped, the change must bump both and re-verify byte-equality. Recording this invariant means the `AGENTS.md`/design note names both pin sites (`vcpkg.json → pulzed-mini`, `CMakeLists.txt → MINI_COMMIT`).

### 4. License attribution survives the deletion

mINI is MIT. Attribution stays intact after `git rm mINI/`: the header's in-file MIT notice (top of `ini.h`) is the content every build compiles, and vcpkg additionally installs `share/pulzed-mini/copyright` on Windows. No `THIRD-PARTY-NOTICES` file is required; no other vendored copy is referenced.

## Risks / Trade-offs

- **[No network on Linux configure]** `file(DOWNLOAD)` needs a working HTTPS connection to raw.githubusercontent.com at CMake configure time (the Windows vcpkg flow already requires network for first build). → Mitigation: pinned URL + `EXPECTED_HASH` make the fetch deterministic; failure `STATUS` is checked and errors out with a clear message; once the file lands in the build dir, subsequent incremental builds reuse it (no re-download on every configure — only when absent, so real configure-time cost is one small file).
- **[vcpkg port named after a fork]** `pulzed-mini` is maintained against `pulzed/mINI`, not the `metayeti` org. → Mitigation: content is verified byte-identical at 0.9.14 (both the port's tag and upstream tag hash to `1396DB49...`); the port existence/version is locked by the committed baseline; record the fork caveat in `AGENTS.md`.
- **[Stale include path references]** Any doc or out-of-tree script that still points at `mINI/src` will fail to resolve. → Mitigation: task list greps the tree (and `AGENTS.md`) for `mINI` after the delete; build failure is loud, not silent.
- **[Incremental Linux rebuild after vendor removal]** A previously configured build dir whose CMake cache references the deleted `mINI/src` must be re-configured. → Mitigation: documented in tasks (re-run `cmake -B build -S .`); build produces a clear configure-time error if the source list is stale.

## Migration Plan

1. Add `pulzed-mini` to `vcpkg.json`; edit both `.vcxproj` files (remove mINI include dirs). Windows build first (both platforms verify the swap before the delete).
2. Add the `file(DOWNLOAD)` + include-dir logic to `CMakeLists.txt`.
3. `git rm -r mINI`.
4. Grep for lingering `mINI`/`mINI/` references; update `AGENTS.md` to name both pin sites and the fork caveat.
5. Verify: Release Win32 + x64 builds, both `EdgeViewer.Tests.exe`s (45 tests / 186 assertions), Linux `cmake --build`.

Rollback: revert the single merge — vcpkg.json, the two `.vcxproj` files, CMakeLists.txt, and restore `mINI/` from the pre-change commit. No data or config migration applies.

## Open Questions

None — decisions 1–4 fully resolve the approach; the only deferrable items (e.g. a future shared 0.9.17+ bump) are recorded as invariants, not blockers.