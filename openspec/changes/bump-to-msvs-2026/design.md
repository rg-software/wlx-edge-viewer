## Context

The Windows build targets the **v143** platform toolset (VS 2022) in both `EdgeViewer.vcxproj` and `EdgeViewer.Tests.vcxproj`. The user runs VS 2026 Community and wants the build bumped there. Machine probe (see proposal.md — Verification result) showed the VS 2026 instance **lacks** the C++ Desktop workload — only a non-buildable partial toolset (`14.51.36231`: x64-only `cl.exe`, no `include`, no standard `lib`, no `vcvarsall.bat`) plus no `Microsoft.VisualStudio.Component.VC.Tools.x86.x64` in its installed-package state. VS 2022 (Community + Build Tools) still has the full C++ toolset and remains available until the 2026 toolchain is proven.

No source code changes are expected: the code is standard C++23 and the toolset bump is purely build metadata.

## Goals / Non-Goals

**Goals:**
- Make the Windows build produce the same two DLLs (`EdgeViewer-Win32.dll`, `EdgeViewer-x64.dll` → packaged as `EdgeViewer.wlx`/`EdgeViewer.wlx64`) and the test exe using the **v145** toolset.
- Keep both platforms (Win32 **and** x64) building — this plugin must ship both.
- Document the exact tool-install requirement so a fresh machine can follow along.

**Non-Goals:**
- No runtime behavior change; no source edits; no `vcpkg.json` change.
- Not migrating to any alternate build system (MSBuild stays; Linux CMake tree is out of scope and unaffected — `CMakeLists.txt` does not reference the MSVC PlatformToolset).
- Not handling the *Linux* build (Qt6/CMake is independent of the MSVC toolset).

## Decisions

### D1. New PlatformToolset value is `v145`
VS 2026's toolset is `v145` (the 2026 install's own `Microsoft.VCRedistVersion.v145.default.props` + `MSBuild\Microsoft\VC\v180` platform props place it at v145). Replace `v143` → `v145` in all 8 configuration blocks (4 per project).

- *Alternative considered:* keeping `v143` and loading VS2022 tools from VS2026 — rejected; user explicitly wants the 2026 toolchain, and v143 requires VS2022 components that defeat the purpose.

### D2. Relax the solution-format marker in `EdgeViewer.sln`
`Format Version 12.00` stays; update the informational header comment `# Visual Studio Version 17` → `# Visual Studio Version 18` and `VisualStudioVersion = 17.2.32630.192` → the 18.x value (e.g. `18.8.12105.206`). This is cosmetic (MSBuild ignores it) but stops the IDE from offering an upgrade on open. Minimal and safe.

### D3. `BuildMakeSetup.bat` needs no functional change — but relies on `vcvarsall.bat` from the dev prompt
The script already calls `%VCINSTALLDIR%\Auxiliary\Build\vcvarsall.bat` then `msbuild` with `UseEnv=true`. Once the 2026 C++ workload is installed, `VCINSTALLDIR` points at `...\18\Community` from the 2026 Developer Command Prompt and `vcvarsall.bat` will exist. **No script edit required.** One doc note: the build must run from the **VS 2026** Developer Command Prompt (or a shell where `%VCINSTALLDIR%` is set to the 2026 install), not 2022.

- *Alternative considered:* hardcoding `18\Community` in the script — rejected as fragile; following the same `%VCINSTALLDIR%` convention keeps it environment-agnostic.

### D4. vcpkg manifest deps require no config change
`vcpkg.json` (`webview2`, `wil`, `catch2`, static triplets) is unchanged. vcpkg is installed at `C:\vcpkg` with MSBuild integration (`%LOCALAPPDATA%\vcpkg\vcpkg.user.props`). In manifest mode the deps rebuild against whatever toolset MSBuild currently selects, so building with v145 pulls v145-built deps automatically. `vcpkg_installed/` is gitignored and will be regenerated.

- *Call-out:* `webview2`/`wil`/`catch2` are header-heavy/COM binding libs; no ABI-sensitive static linkage that would couple them to a specific MSVC minor version.

### D5. Verification must happen against a fully-provisioned 2026 toolchain
The 2026 install is currently **not buildable**. The task list will include an explicit **install step** (VS Installer → Modify → "Desktop development with C++" + Windows 11 SDK, or `vs_Community.exe modify --add Microsoft.VisualStudio.Workload.NativeDesktop`), then re-probe (check for `VC\Tools\MSVC\<ver>\{include,lib\x64,lib\x86}`, `VC\Auxiliary\Build\vcvarsall.bat`, both `Hostx64\x86` and `Hostx64\x64` cl.exe). Rollback to VS2022 is trivial (v143 is still installed) until the bump is proven.

## Risks / Trade-offs

- **[Risk] 2026 C++ workload not yet installed** → The proposal already surfaced this; task 1 is the explicit install + re-probe gating the whole change. If the user declines to install, the change is halted and the v143 baseline is untouched.
- **[Risk] Compiler emits new/removed warnings or errors on `stdcpplatest`** (v145 may implement newer C++23/26 features with different diagnostics) → Address as they appear; no source change is expected, but if a genuinely new warning appears, fix it rather than suppress. Verify cleanly on both Win32 and x64.
- **[Risk] Static triplet/dep rebuild under a new toolset takes time and may hit vcpkg port issues** → The three deps are header/COM-centric; low risk. If a port fails against v145, pin or report rather than vendoring.
- **[Trade-off] This is an irreversible-looking bump but actually reversible** → VS2022 v143 remains installed; reverting the `.vcxproj` tag is a one-line change. Keeping 2022 installed through verification provides an escape hatch.

## Migration Plan

1. Install the missing C++ workload + Win SDK into the 2026 instance; re-probe toolset completeness.
2. Apply `v143` → `v145` in both `.vcxproj` files; update `EdgeViewer.sln` header marker.
3. Update docs (`AGENTS.md`, `Readme.md`) to VS 2026 / v145.
4. Build Release Win32 + x64 from the 2026 Developer Command Prompt; run `EdgeViewer.Tests` on both platforms.
5. Package via `BuildMakeSetup.bat`; manual-load sanity check in Total Commander.
6. Rollback if any step fails: restore `v143` tags (git) — no other change is irreversible.

## Open Questions

- None — the only unknown (whether 2026 build tools are present) was answered by the probe and is now an explicit prerequisite task.
