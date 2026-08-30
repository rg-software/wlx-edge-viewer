## 1. Toolchain prerequisite (VS 2026 C++ workload)

- [x] 1.1 Install the missing C++ build tools into VS 2026 Community (`C:\Program Files\Microsoft Visual Studio\18\Community`): via the Visual Studio Installer → **Modify** → enable the **Desktop development with C++** workload (which pulls `Microsoft.VisualStudio.Component.VC.Tools.x86.x64`, the MSVC x86/x64 toolset, `vcvarsall.bat`, and the Win SDK), or from the install dir: `vs_Community.exe modify --add Microsoft.VisualStudio.Workload.NativeDesktop`.
- [x] 1.2 Re-probe the install to confirm it is now buildable: verify all of `%VSDIR18%\VC\Tools\MSVC\<ver>\{include, lib\x64, lib\x86}`, `%VSDIR18%\VC\Auxiliary\Build\vcvarsall.bat`, and both cross-compilers `bin\Hostx64\x64\cl.exe` + `bin\Hostx64\x86\cl.exe` (needed for the Win32 target) exist. Log the resolved MSVC version.

## 2. Project / solution toolset bump (v143 → v145)

- [x] 2.1 In `EdgeViewer/EdgeViewer.vcxproj`, replace `<PlatformToolset>v143</PlatformToolset>` with `<PlatformToolset>v145</PlatformToolset>` in all 4 configuration blocks (Release|Win32, Debug|Win32, Release|x64, Debug|x64).
- [x] 2.2 In `EdgeViewer.Tests/EdgeViewer.Tests.vcxproj`, apply the same `v143` → `v145` replacement in all 4 configuration blocks.
- [x] 2.3 In `EdgeViewer.sln`, update the informational header marker for VS 2026: `# Visual Studio Version 17` → `# Visual Studio Version 18` and set `VisualStudioVersion = 18.8.12105.206` (keep `Format Version 12.00` and `MinimumVisualStudioVersion` as-is).

## 3. Documentation

- [x] 3.1 Update `AGENTS.md` Windows build section: "MS Visual Studio 2022 (v143 toolset)" → "MS Visual Studio 2026 (v145 toolset)"; note the build must run from the **VS 2026** Developer Command Prompt so `%VCINSTALLDIR%` points at the 2026 install (per `BuildMakeSetup.bat`).
- [x] 3.2 Update `Readme.md` (~line 81): "MS Visual Studio 2022" → "MS Visual Studio 2026", and mention the Desktop development with C++ workload requirement.

## 4. Verify

- [x] 4.1 From the **VS 2026** Developer Command Prompt, build Release for both Win32 and x64: `msbuild EdgeViewer.sln /t:Build /p:Configuration=Release /p:Platform=Win32 /p:UseEnv=true` and `... /p:Platform=x64` (or run `BuildMakeSetup.bat`). Confirm both `EdgeViewer-Win32.dll` and `EdgeViewer-x64.dll` build cleanly with **no warnings or errors**; confirm vcpkg manifest deps (`webview2`, `wil`, `catch2`) rebuild successfully against the v145 toolset. (Required `VCPKG_VISUAL_STUDIO_PATH` + `VCPKG_PLATFORM_TOOLSET=v145` for vcpkg's VS 2026 detection — see AGENTS.md.)
- [x] 4.2 Build and run the test suite on both platforms (`msbuild EdgeViewer.Tests/EdgeViewer.Tests.vcxproj /p:Configuration=Release /p:Platform=Win32|x64`, then run `winbuild\EdgeViewer.Tests_<Platform>_Release\EdgeViewer.Tests.exe`); confirm 60 tests / 263 assertions pass on both platforms with the v145 toolset.
- [x] 4.3 Package with `BuildMakeSetup.bat` and do a manual-load sanity check in Total Commander over `Examples/` (Markdown, AsciiDoc, RST, HTML, images, directories, PDF) for both the 32- and 64-bit plugin, confirming no behavior regression versus the v143 build.
