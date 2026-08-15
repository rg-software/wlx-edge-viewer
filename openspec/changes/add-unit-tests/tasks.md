## 1. Tier 4 extractions (do first — improve the Windows source before testing it)

- [ ] 1.1 Create `EdgeViewer/WlxDetect.h` declaring `std::string BuildDetectString(const mINI::INIStructure& ini);` Move the EXT="..." construction logic from `DllMain.cpp::ListGetDetectString` into a new `EdgeViewer/WlxDetect.cpp`. Update `DllMain.cpp::ListGetDetectString` to: `strcpy_s(DetectString, maxlen, BuildDetectString(GlobalSettings()).c_str());`. Build the DLL (Release|x64); the existing detect-string behavior is byte-identical.
- [ ] 1.2 Create `EdgeViewer/ZoomHotkey.h` declaring `bool ZoomHotkeyHandled(UINT key, bool ctrlHeld, double currentZoom, double& newZoom);` Move the discrete-step-table snap logic from `WebView2.cpp::ZoomHotkeyHandled` into a new `EdgeViewer/ZoomHotkey.cpp`. The new function is pure: takes current zoom via parameter, returns the new zoom via out-parameter, returns whether the key was handled. Update `WebView2.cpp::ZoomHotkeyHandled` to: read current zoom from `ctrl->get_ZoomFactor`, call the pure function, write `newZoom` back via `ctrl->put_ZoomFactor` only if it returns true. Build the DLL (Release|x64); zoom hotkey behavior is byte-identical.
- [ ] 1.3 Add free function declarations `std::wstring BuildFindScript(const std::wstring& pattern, int params);` and `std::wstring BuildPrintScript();` to `EdgeViewer/Navigator.h`. Implement them in `Navigator.cpp` by extracting the script-string construction logic from `Navigator::Search` and `Navigator::Print`. The methods become one-line wrappers: `BuildFindScript` returns the string, `Search` passes it to `mWebView->ExecuteScript`. Build the DLL (Release|x64); search and print behavior is byte-identical.
- [ ] 1.4 Verify all three extractions on Win32 too: build Release|Win32, load both DLLs in TC, sanity-check detect string (`Examples/` files route to the right processors), Ctrl+Plus/Minus/0 zoom, and Ctrl+F search. Tag commit as `extractions-stable` on the branch.

## 2. Harness setup

- [ ] 2.1 Add `catch2` to `vcpkg.json` dependencies (alphabetical order: `catch2`, `webview2`, `wil`). Verify next MSBuild invocation restores the package into `vcpkg_installed/`.
- [ ] 2.2 Create `EdgeViewer.Tests/EdgeViewer.Tests.vcxproj` configured for `Debug|Win32`, `Release|Win32`, `Debug|x64`, `Release|x64`. Set `LanguageStandard` to `stdcpplatest`, `RuntimeLibrary` to match the main project per configuration (`MultiThreaded`/`MultiThreadedDebug`), `VcpkgTriplet` to `x86-windows-static`/`x64-windows-static` as appropriate. Set `OutDir` to `$(SolutionDir)Build\EdgeViewer.Tests_$(Platform)_$(Configuration)\`.
- [ ] 2.3 Configure the test project's `AdditionalIncludeDirectories` to include `$(SolutionDir)mINI\src\` and `$(SolutionDir)EdgeViewer\` (so test files can include `Globals.h`, etc.). Do NOT include any WebView2 or WIL paths.
- [ ] 2.4 Add `ClCompile` entries for the shared subset: `../EdgeViewer/Globals.cpp`, `../EdgeViewer/Navigator.cpp`, `../EdgeViewer/Processors/ProcessorInterface.cpp`, `../EdgeViewer/Processors/HtmlProcessor.cpp`, `../EdgeViewer/Processors/DirProcessor.cpp`, `../EdgeViewer/WlxDetect.cpp`, `../EdgeViewer/ZoomHotkey.cpp`. Confirm none of these TUs transit `<webview2.h>` or `<wil/com.h>` after the extractions.
- [ ] 2.5 Configure `Link` with `shlwapi.lib;wininet.lib` (matches the main project for Tier 3 path tests). Add `catch2` via vcpkg auto-link (no explicit `#pragma comment(lib,…)`.
- [ ] 2.6 Create `EdgeViewer.Tests/main.cpp` with Catch2 main: `#define CATCH_CONFIG_MAIN`, `#include <catch2/catch_all.hpp>`. Add optional reporter configuration for `--reporter=console::srng` (deterministic seed).
- [ ] 2.7 Create `EdgeViewer.Tests/pch.h` and `pch.cpp` precompiling `<catch2/catch_all.hpp>` and `<string>` / `<filesystem>` / `<map>`; enable `PrecompiledHeader` on the test project and set `pch.h` as the header. Update other test `.cpp` files to include `pch.h` first.
- [ ] 2.8 Add the test project to `EdgeViewer.sln` (Win32 and x64 configurations). Verify both configurations build clean: `msbuild EdgeViewer.sln /t:EdgeViewer.Tests /p:Configuration=Release /p:Platform=x64` and the same for Win32. Empty-test output runs and exits 0.

## 3. Test helpers

- [ ] 3.1 Add `EdgeViewer.Tests/TestHelpers/IniBuilder.h`: small DSL to build `mINI::INIStructure` in memory for T2/T4 tests (e.g. `IniBuilder().with("Extensions", "Markdown", "MD").with("Markdown", "CSS", "github.css").with("Markdown", "CSSDark", "github.dark.css").build()`). Returns an `mINI::INIStructure` value; no shared state.
- [ ] 3.2 Add `EdgeViewer.Tests/TestHelpers/TempDir.h`: RAII class that creates a unique directory under the system temp path on construction and recursively removes it on destruction; used by T3 path tests as a clean filesystem fixture.

## 4. Tier 1 — pure helper tests

- [ ] 4.1 Write `EdgeViewer.Tests/tier1_helpers.cpp` covering `to_utf8(std::wstring)` and `to_utf16(std::string)` round-trip and named charset cases (ASCII, Cyrillic, emoji, empty string). Tag `[t1][smoke]`. Reference `specs/plugin-config/spec.md` (config parsing paths).
- [ ] 4.2 Test `to_int(std::string)` parser for valid ints, negative numbers, empty string, non-numeric string (returns 0 per `atoi` semantics). Tag `[t1][smoke]`.
- [ ] 4.3 Test `ProcessorInterface::replacePlaceholders` for: single placeholder replacement, multiple placeholders in one call, no placeholder present, placeholder appearing multiple times, regex-meaningful characters in the value (e.g. `$`, `\\`). Tag `[t1][smoke]`.
- [ ] 4.4 Test `ProcessorInterface::isType` for: extension is in the type's csv list, is not in the list, case-insensitive match, empty extension, empty list. (NOTE: `isType` reads from `GlobalSettings()`, so this test is technically T2; keep the assignment here and pass a pre-seeded `GlobalSettings()` via test setup if possible, or move to T2 if a helper signature change is needed.) Tag `[t1][smoke]` or move to T2.
- [ ] 4.5 Test `Navigator::jsEscape` for: backslash escaping, single-quote escaping, no special characters, both special characters interleaved. Tag `[t1][smoke]`. Reference `specs/text-search/spec.md`.
- [ ] 4.6 Test `DirProcessor::stripTwodots` for: path ending in `..\` stripped, path not ending in `..\` unchanged, path ending in `..` (no backslash) unchanged, empty path. Tag `[t1][smoke]`. Reference `specs/directory-view/spec.md`.
- [ ] 4.7 Test `DirProcessor::extensionsToMaskRegex` for: single extension builds `.+\\.(ext)$` case-insensitive; multiple pipe-separated extensions build `.+\\.(ext1|ext2)$`; empty extensions string. Tag `[t1][smoke]`.
- [ ] 4.8 Test `HtmlProcessor::detectedFromBom` for: UTF-8 BOM (0xEF 0xBB 0xBF) → "UTF-8"; UTF-16LE BOM (0xFF 0xFE) → "UTF-16LE"; UTF-16BE BOM (0xFE 0xFF) → "UTF-16BE"; no BOM → empty string. Tag `[t1][smoke]`. Reference `specs/html/spec.md`.
- [ ] 4.9 Test `HtmlProcessor::detectedFromMeta` for: short form `<meta charset="UTF-8">` → "UTF-8"; long form `<meta content="text/html; charset=windows-1251">` → "windows-1251"; mixed case meta tag; no charset meta → empty string. Tag `[t1][smoke]`.
- [ ] 4.10 Test `HtmlProcessor::detectedCharset` for: BOM takes precedence over meta; meta used when BOM absent; empty string when neither present. Tag `[t1][smoke]`.

## 5. Tier 2 — ini-seeded helper tests

- [ ] 5.1 Write `EdgeViewer.Tests/tier2_config.cpp` test `GlobalSettings` lazy-load with `[Chromium] UserDir` present: the returned value matches the ini. Tag `[t2]`. Reference `specs/plugin-config/spec.md` and `specs/temp-file-management/spec.md`.
- [ ] 5.2 Test `GlobalSettings` lazy-load with `[Chromium] UserDir` missing: defaults to the plugin directory (via `GetModulePath()`). Tag `[t2]`.
- [ ] 5.3 Test per-type CSS section reads: `[Markdown] CSS` and `CSSDark` both returned; `[AsciiDoc] CSS` only (no `CSSDark`); `[Images] CSS`/`CSSDark` plus `FitToScreen`. Tag `[t2]`. Reference `specs/dark-mode/spec.md`.
- [ ] 5.4 Test `[Directory]` complete section read: `DirImageExt`, `DirOtherExt`, `CSS`, `CSSDark`, `ShowNames`, `ShowFolders`, `FitToScreen`, `TruncateNames`, `NamesUnderThumbnails`, `GenDirThumbs`, `DirThumbSize`. Tag `[t2]`. Reference `specs/directory-view/spec.md`.
- [ ] 5.5 Test `[Extensions]` section read: each type (`HTML`, `Markdown`, `AsciiDoc`, `URL`, `MHTML`, `EML`, `RST`, `Images`, `Other`) returns the csv list; `Dirs` returns "1" or "" depending on the test fixture. Tag `[t2]`.
- [ ] 5.6 Move the `isType` test from task 4.4 here if it needed the ini-seeded `GlobalSettings` fixture rather than a pure one. Tag `[t2]`.
- [ ] 5.7 Test `ForcedHtmlExt` matches `xml|xhtml` regex against an XML file extension, an XHTML file extension, and a non-matching extension. Tag `[t2]`. Reference `specs/temp-file-management/spec.md`.

## 6. Tier 3 — Win32-bound path tests

- [ ] 6.1 Write `EdgeViewer.Tests/tier3_paths.cpp` test `GetPhysicalPath` returns the original path unchanged for a plain absolute path with no `\\?\` prefix. Use `TempDir` fixture. Tag `[t3]`.
- [ ] 6.2 Test `GetPhysicalPath` strips the `\\?\` prefix from a normalized path: input `\\?\C:\path\file.md`, output `C:\path\file.md`. Tag `[t3]`. Reference `specs/temp-file-management/spec.md` (path prefix stripping requirement).
- [ ] 6.3 Test `GetPhysicalPath` strips the `\\?\UNC\` prefix: input `\\?\UNC\server\share\file.md`, output `\\server\share\file.md`. Tag `[t3]`.
- [ ] 6.4 Test `GetPhysicalPath` copies a UNC-path file to temp when the source is a real file under `\\?\UNC\` (set up via a local network share mock OR a UNC-path-shaped fixture created under `TempDir`). The returned path is in the system temp directory and has the original extension. The copy's bytes match the source's bytes. Tag `[t3]`. (If a real UNC fixture is infeasible on CI runners, mark with a `[!nonportable]` tag and disable by default; for now we run locally.)
- [ ] 6.5 Test `GetPhysicalPath` triggers `ForcedHtmlExt` temp-copy when the file extension matches `xml|xhtml`: a fixture `.xml` file is copied to a temp file ending in `.html`. Tag `[t3]`. Reference `specs/temp-file-management/spec.md` (ForcedHtmlExt temp-copy requirement).
- [ ] 6.6 Test `GetPhysicalPath` does NOT trigger ForcedHtmlExt when the extension doesn't match: a `.txt` file is NOT copied. Tag `[t3]`.
- [ ] 6.7 Test `GenTempFile` + `RemoveTempFiles` cycle: generate a temp file from a known source, assert it exists on disk and is tracked; call `RemoveTempFiles`; assert the file no longer exists. Tag `[t3]`.
- [ ] 6.8 Test `urlPathW` URL-escaping: a plain ASCII path round-trips unchanged (after percent-decode by the test); a path with spaces returns `%20`; a path with `#` uses the placeholder dance — input `file #1.md` produces a URL with `%23` and the placeholder string is replaced back. Tag `[t3]`. Reference `specs/virtual-host-mapping/spec.md`.
- [ ] 6.9 Test symlink resolution in `GetPhysicalPathForLink` using `TempDir` to create a symlink to a known target file; the returned path points to the real target. Tag `[t3]`. (Skipped if the test runner lacks permission to create symlinks; use the `TempDir` RAII and the `[!requiresadmin]` tag convention.)
- [ ] 6.10 Test `GetPhysicalPathForLink` returns the original path when the input does not exist (no symlink to resolve): the function falls back gracefully. Tag `[t3]`.

## 7. Tier 4 — extraction tests

- [ ] 7.1 Write `EdgeViewer.Tests/tier4_extractions.cpp` test `BuildDetectString` with the default shipped `edgeviewer.ini` produces a detect string starting with `EXT="HTM"` and containing every standard type section in order (HTML, Markdown, AsciiDoc, URL, MHTML, EML, RST, Images, Other), separated by `|EXT="..."`, ending with `"`. Tag `[t4][smoke]`. Reference `specs/wlx-contract/spec.md` (detect-string generation requirement).
- [ ] 7.2 Test `BuildDetectString` with `[Extensions] Dirs=1`: the detect string ends with `,` appended before the closing `"` (the empty extension matches directories). Tag `[t4][smoke]`.
- [ ] 7.3 Test `BuildDetectString` with `[Extensions] Dirs=0` or missing: no trailing comma. Tag `[t4][smoke]`.
- [ ] 7.4 Test `BuildDetectString` with a custom `[Extensions]` containing only one type: the result is `EXT="<exts>"` with no separators. Tag `[t4][smoke]`.
- [ ] 7.5 Test pure `ZoomHotkeyHandled` for: Ctrl+`VK_OEM_PLUS` with `currentZoom=1.0` returns `true` and `newZoom=1.1`; Ctrl+`VK_OEM_MINUS` with `1.0` returns `true` and `newZoom=0.9`; Ctrl+`'0'` or Ctrl+`VK_NUMPAD0` with any factor returns `true` and `newZoom=1.0`. Tag `[t4][smoke]`. Reference `specs/zoom-control/spec.md` (zoom hotkeys and step table requirements).
- [ ] 7.6 Test `ZoomHotkeyHandled` ceiling: `currentZoom=5.0` with Ctrl+`VK_OEM_PLUS` returns `true` and `newZoom=5.0` (no change because no higher step exists). Floor: `currentZoom=0.25` with Ctrl+`VK_OEM_MINUS` returns `true` and `newZoom=0.25`. Tag `[t4][smoke]`.
- [ ] 7.7 Test `ZoomHotkeyHandled` for keys not in the zoom set: Ctrl+`'A'` returns `false`, `newZoom` is untouched. Tag `[t4][smoke]`.
- [ ] 7.8 Test `ZoomHotkeyHandled` for unmodified zoom keys: `'0'` without Ctrl returns `false`; `VK_OEM_PLUS` without Ctrl returns `false`. Tag `[t4][smoke]`.
- [ ] 7.9 Test `BuildFindScript` produces the expected `window.find('pattern', case, backwards, false, wholeWord, false, false)` format for each parameter combination: default (no flags), `lcs_matchcase` only, `lcs_backwards` only, `lcs_wholewords` only, all three combined. Tag `[t4][smoke]`. Reference `specs/text-search/spec.md` (search parameter flags requirement).
- [ ] 7.10 Test `BuildFindScript` with `lcs_findfirst` (1) set: the script produced is the `while(window.find(...))` repeated backwards reset form. Combinations: `findfirst`+`matchcase`, `findfirst` alone. Tag `[t4][smoke]`. Reference `specs/text-search/spec.md` (find-first behavior requirement).
- [ ] 7.11 Test `BuildFindScript` escape behavior: a pattern containing `'` or `\\` is escaped via `jsEscape`, so the embedded `window.find('...')` shell remains syntactically valid. Tag `[t4][smoke]`.
- [ ] 7.12 Test `BuildPrintScript` returns the literal string `window.print();` with no parameters. Tag `[t4][smoke]`. Reference `specs/print/spec.md`.

## 8. Test project readme

- [ ] 8.1 Add `EdgeViewer.Tests/readme.md` documenting: how to build the test project (`msbuild EdgeViewer.sln /t:EdgeViewer.Tests /p:Configuration=Release /p:Platform=x64`), how to run it (path of the exe), the six tier model, the `[smoke]` tag for quick runs, the Win32/x64 parity expectation, what's not covered (T5 port-time, T6 indefinite), and the convention for new helpers (a corresponding test is expected at the tier of the helper's purity).

## 9. Verify (per AGENTS.md and the spec's parity requirement)

- [ ] 9.1 Build Release|x64 of the full solution including the test project: `vcvarsall.bat x64 && msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=x64 /p:UseEnv=true`. Confirm `EdgeViewer.dll` and `EdgeViewer.Tests.exe` both produce.
- [ ] 9.2 Run the test exe for x64: `Build\EdgeViewer.Tests_x64_Release\EdgeViewer.Tests.exe --reporter=console::srng`. Expect zero failures across all tiers.
- [ ] 9.3 Build Release|Win32 and run its test exe: `vcvarsall.bat x86 && msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=Win32 /p:UseEnv=true`, then `Build\EdgeViewer.Tests_Win32_Release\EdgeViewer.Tests.exe --reporter=console::srng`. Expect zero failures.
- [ ] 9.4 Diff the two test outputs: the set of passing test names MUST be identical between Win32 and x64 (per `specs/test-harness/spec.md` Win32/x64 parity requirement).
- [ ] 9.5 Build the main DLL Release|x64 and Release|Win32 via the existing `BuildMakeSetup.bat` workflow (or `vcvarsall + msbuild` equivalent). Confirm both DLLs still load in Total Commander; manually verify each `Examples/` file type routes correctly (verifying the Tier 4 extractions did not regress Windows behavior). Reference `specs/wlx-contract/spec.md`, `specs/text-search/spec.md`, `specs/print/spec.md`, `specs/zoom-control/spec.md` for the affected features.
- [ ] 9.6 Run `openspec validate add-unit-tests --strict` and resolve any issues before archiving.
- [ ] 9.7 Archive the change with `openspec archive add-unit-tests` once both platform builds pass manual verification and the test suites pass with zero failures on both platforms.