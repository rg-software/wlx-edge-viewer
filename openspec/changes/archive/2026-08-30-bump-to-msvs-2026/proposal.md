## Why

The project currently builds with the **v143** toolset (MS Visual Studio 2022). Moving to **v145** (MS Visual Studio 2026) keeps the Windows build on a supported, current toolchain and modernizes the C++23/`stdcpplatest` experience. The user has VS 2026 Community 18.8 installed but — as the probe below confirms — the required C++ build tools are **missing**, so the bump itself and the tool-install checklist are both in scope.

## What Changes

- **BREAKING (toolchain):** Switch the Windows build from PlatformToolset `v143` (VS 2022) to `v145` (VS 2026) in both projects.
  - `EdgeViewer/EdgeViewer.vcxproj` — `<PlatformToolset>v143</PlatformToolset>` → `v145` in all 4 configs (Debug/Release × Win32/x64).
  - `EdgeViewer.Tests/EdgeViewer.Tests.vcxproj` — same change, all 4 configs.
- Update the solution-format marker in `EdgeViewer.sln` (`# Visual Studio Version 17` / `VisualStudioVersion = 17.2...`) to the VS 2026 (18.x) value so IDEs don't warn.
- **Tool-install prerequisite verified** (this is the check the user asked for): confirm the "Desktop development with C++" workload plus the Windows 11 SDK are present in the VS 2026 instance before building.
- Update tooling documentation: `AGENTS.md` (Windows build section: MSVS 2022 → 2026, v143 → v145) and `Readme.md` (line ~81 "MS Visual Studio 2022").

## Capabilities

### New Capabilities
- (none — tooling-only)

### Modified Capabilities
- (none — no runtime/plugin behavior changes; `skip_specs: true`)

## Impact

- **Code:** none. Sources are compiler-portable C++23; only the project/solution build metadata changes.
- **Build config:** `EdgeViewer/EdgeViewer.vcxproj`, `EdgeViewer.Tests/EdgeViewer.Tests.vcxproj`, `EdgeViewer.sln`.
- **Toolchain:** requires VS 2026 with the C++ Desktop workload + Win SDK **installed** (see verification).
- **Dependencies (vcpkg):** unchanged `vcpkg.json` (`webview2`, `wil`, `catch2`, static triplets `x86-windows-static`/`x64-windows-static`). vcpkg is present at `C:\vcpkg` with MSBuild integration (`%LOCALAPPDATA%\vcpkg\vcpkg.user.props`), so manifest deps rebuild against the v145 toolset automatically.

## Verification result (tool-install check)

Probed the machine (`vswhere -all` + VS Setup state + filesystem) on 2026-08-29:

| Install | Path | Has C++ tools? |
|---|---|---|
| VS 2026 Community 18.8.12105.206 | `C:\Program Files\Microsoft Visual Studio\18\Community` | **No** |
| VS 2022 Community 17.14 | `C:\Program Files\Microsoft Visual Studio\2022\Community` | Yes |
| VS 2022 Build Tools 17.14 | `...\2022\BuildTools` | Yes |

The VS 2026 instance's installed-package list contains only the `CoreEditor`, `ManagedGame` (Unity), `NetCrossPlat` (MAUI), and `Office` workloads. It has **no** `Microsoft.VisualStudio.Component.VC.Tools.x86.x64` and no `Microsoft.VisualStudio.Workload.NativeDesktop`. The stray `VC\Tools\MSVC\14.51.36231` dir is a partial/failed toolset: only `bin\Hostx64\x64\cl.exe` (no x86 cross-compiler), no `include` headers, no standard `lib\x64`/`lib\x86` (only `lib\onecore`), and no `vcvarsall.bat` (only `vcvars64.bat`). It is **not buildable**.

**Conclusion:** before this change can be verified, the "Desktop development with C++" workload (which pulls the x86/x64 toolset, `vcvarsall.bat`, and `Include`/`Lib` trees) and a Windows 11 SDK must be installed into the VS 2026 Community instance, e.g. via the Visual Studio Installer → Modify → Desktop development with C++ (or `vs_Community.exe modify --add Microsoft.VisualStudio.Workload.NativeDesktop`).
