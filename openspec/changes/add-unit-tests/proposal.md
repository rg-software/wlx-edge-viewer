## Why

The `characterize-existing-behavior` branch added formal specs for ~110 requirements across 18 capability areas, but none of them is yet backed by an executable test. The upcoming `port-to-double-commander-linux` change will refactor core logic (introduce `IWebView`, drop encoding override, change config section names, rework window/IPC) and needs an executable regression net on `master` to land safely. Building the harness + Tier 1-4 tests now means the port can rely on green tests instead of guesswork, and the three small pure-function extractions performed as a side effect (detect-string builder, zoom-key handler, find-script builder, print-script) improve the Windows source independent of the port.

## What Changes

- **New**: Catch2 v3 added to `vcpkg.json` (`catch2`).
- **New**: `EdgeViewer.Tests/` test project added to `EdgeViewer.sln`, producing `EdgeViewer.Tests.exe`. Static CRT, Release/Debug, Win32 and x64 — mirroring the main project. Currently Windows-only execution; Linux execution comes with the port change (CMake target added there).
- **New**: A thin `test-harness` spec under `specs/test-harness/spec.md` describing the contract the test layer satisfies: what tiers exist, what's required of a new helper before merge, and the governs-tests-but-doesn't-duplicate-specs rule.
- **New**: ~50 tests across four tiers (details in design.md):
  - **Tier 1 — pure helpers**: `to_utf8`, `to_utf16`, `to_int`, `ProcessorInterface::replacePlaceholders`, `ProcessorInterface::isType`, `Navigator::jsEscape`, `DirProcessor::stripTwodots`, `DirProcessor::extensionsToMaskRegex`, `HtmlProcessor::detectedFromBom`, `HtmlProcessor::detectedFromMeta`, `HtmlProcessor::detectedCharset`.
  - **Tier 2 — ini-seeded helpers**: `GlobalSettings()` lazy load with `UserDir` default fallback when `[Chromium] UserDir` is missing; `GlobalSettings()` reading per-type sections (`[Markdown] CSS` vs `CSSDark`, `[AsciiDoc] CSS` only, `[Directory] DirImageExt`, `[Extensions] Dirs`).
  - **Tier 3 — Win32-bound path helpers**: `GetPhysicalPath` prefix stripping (`\\?\`, `\\?\UNC\`), `GenTempFile` tracking + `RemoveTempFiles` iteration, `ForcedHtmlExt` regex match triggering temp-copy on matched extensions and not on non-matching, `ProcessorInterface::urlPathW` URL-escaping (including the `#`-placeholder dance) via `shlwapi`. These run on Windows only and link `shlwapi`/`wininet` like the main project.
  - **Tier 4 — small extractions + tests**: three pure helpers pulled out of impure code and tested in isolation:
    - `BuildDetectString(const INIStructure&) -> std::string` extracted from `DllMain.cpp::ListGetDetectString`. The string-builder logic is pure; the caller keeps the `strcpy_s` to the buffer.
    - `ZoomHotkeyHandled(UINT key, bool ctrlHeld, double currentZoom, double& newZoom) -> bool` extracted from `WebView2.cpp::ZoomHotkeyHandled`. The COM `controller->get_ZoomFactor` call stays at the call site; the pure step-snap logic moves to the new function.
    - `BuildFindScript(const std::wstring& pattern, int params) -> std::wstring` and `BuildPrintScript() -> std::wstring` extracted from `Navigator::Search` / `Navigator::Print`. Both methods become thin wrappers calling the pure builder then `webView->ExecuteScript(builder(...))`.
- **BREAKING** (none): existing behavior of all functions is preserved; the three Tier 4 extractions are pure refactors with no observable change.
- **Excluded (Tier 5)**: processor body tests (asserting `MdProcessor::OpenIn` calls `RegisterVirtualHost` then `NavigateToString` with expected HTML) — gated on the `IWebView` abstraction the port introduces. Will be added during the port as the first refactor step, against the same `specs/<capability>/spec.md` baselines.
- **Excluded (Tier 6)**: directory-thumbnail generation and shell-context-menu tests — require COM shell mocking, deferred indefinitely (the characterize specs act as the documentation regression for these).

## Capabilities

### New Capabilities

- `test-harness`: the executable test layer contract — what tiers exist (T1 pure / T2 ini-seeded / T3 Win32-bound / T4 extraction / T5 IWebView-mock — port-time / T6 shell-bound — deferred), what framework is used, what the coverage rule is for a new helper before it merges to master, and the parity requirement that Win32 and x64 test builds produce the same pass/fail set.

### Modified Capabilities

- (none) — the existing `eml` spec and the 18 baseline specs from `characterize-existing-behavior` (not yet archived) describe behavior; this change adds the executable verification layer but does not modify any spec-level behavior.

## Impact

- **Code**:
  - New test project: `EdgeViewer.Tests/EdgeViewer.Tests.vcxproj`, `EdgeViewer.Tests/main.cpp`, `EdgeViewer.Tests/tier1_helpers.cpp`, `EdgeViewer.Tests/tier2_config.cpp`, `EdgeViewer.Tests/tier3_paths.cpp`, `EdgeViewer.Tests/tier4_extractions.cpp`. The test project links against the *shared* compile units from `EdgeViewer/` (compiles `Globals.cpp`, `Navigator.cpp`, `Processors/ProcessorInterface.cpp`, `HtmlProcessor.cpp`, `DirProcessor.cpp` directly — instead of linking the DLL — so it can reach internal helpers without export ceremony).
  - **Tier 4 extractions**:
    - `EdgeViewer/WlxDetect.h` declares `BuildDetectString`; `DllMain.cpp::ListGetDetectString` becomes `strcpy_s(DetectString, maxlen, BuildDetectString(GlobalSettings()).c_str())`.
    - `EdgeViewer/ZoomHotkey.h` declares `ZoomHotkeyHandled(UINT, bool, double, double&)` taking the current zoom and returning the new zoom via out-param; `WebView2.cpp::ZoomHotkeyHandled` calls the pure function after issuing `ctrl->get_ZoomFactor`.
    - `EdgeViewer/Navigator.h`/`.cpp` gain two free helper declarations `BuildFindScript`, `BuildPrintScript`; the existing methods call them and pass the result to `ExecuteScript`.
  - `vcpkg.json` gains `catch2` (no version pin; inherits vcpkg default; static triplets `x86-windows-static` / `x64-windows-static` as today).
  - `EdgeViewer.sln` adds the `EdgeViewer.Tests` project with project-to-project reference to `EdgeViewer` (for header sharing only — see design.md for the "compile sources directly, don't link DLL" rationale).
- **Build**: new test executable built by `msbuild EdgeViewer.sln /t:EdgeViewer.Tests /p:Configuration=Release /p:Platform=x64`. Added as a separate project so the main DLL build is unaffected when the test project is excluded.
- **Dependencies**: `catch2` added to `vcpkg.json` (manifest mode, pinned by vcpkg baseline as today for `webview2`/`wil`).
- **CI**: not present in this project today; verify step in tasks.md runs `EdgeViewer.Tests.exe` for Win32 and x64 locally.
- **Downstream**: once archived, the port branch (`port-to-double-commander-linux`) adds Tier 5 tests against this harness as the first refactor task; its `tasks.md` is amended after this archive.