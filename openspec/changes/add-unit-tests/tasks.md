## 1. Tier 4 extractions (do first — improve the Windows source before testing it)

- [x] 1.1 Create `EdgeViewer/WlxDetect.h` declaring `std::string BuildDetectString(const mINI::INIStructure& ini);` Move the EXT="..." construction logic from `DllMain.cpp::ListGetDetectString` into a new `EdgeViewer/WlxDetect.cpp`. Update `DllMain.cpp::ListGetDetectString` to: `strcpy_s(DetectString, maxlen, BuildDetectString(GlobalSettings()).c_str());`. Build the DLL (Release|x64); the existing detect-string behavior is byte-identical.
- [x] 1.2 Create `EdgeViewer/ZoomHotkey.h` declaring `bool ZoomHotkeyHandled(UINT key, bool ctrlHeld, double currentZoom, double& newZoom);` Move the discrete-step-table snap logic from `WebView2.cpp::ZoomHotkeyHandled` into a new `EdgeViewer/ZoomHotkey.cpp`. The new function is pure: takes current zoom via parameter, returns the new zoom via out-parameter, returns whether the key was handled. Update `WebView2.cpp::ZoomHotkeyHandled` to: read current zoom from `ctrl->get_ZoomFactor`, call the pure function, write `newZoom` back via `ctrl->put_ZoomFactor` only if it returns true. Build the DLL (Release|x64); zoom hotkey behavior is byte-identical.
- [x] 1.3 Add free function declarations `std::wstring BuildFindScript(const std::wstring& pattern, int params);` and `std::wstring BuildPrintScript();` to `EdgeViewer/Navigator.h`. Implement them in `Navigator.cpp` by extracting the script-string construction logic from `Navigator::Search` and `Navigator::Print`. The methods become one-line wrappers: `BuildFindScript` returns the string, `Search` passes it to `mWebView->ExecuteScript`. Build the DLL (Release|x64); search and print behavior is byte-identical.
- [x] 1.4 Verify all three extractions on Win32 too: build Release|Win32, load both DLLs in TC, sanity-check detect string (`Examples/` files route to the right processors), Ctrl+Plus/Minus/0 zoom, and Ctrl+F search. Tag commit as `extractions-stable` on the branch.

## 2. Harness setup

- [x] 2.1 Add `catch2` to `vcpkg.json`
- [x] 2.2 Create `EdgeViewer.Tests/EdgeViewer.Tests.vcxproj`
- [x] 2.3 Configure the test project's `AdditionalIncludeDirectories`
- [x] 2.4 Add `ClCompile` entries for the shared subset
- [x] 2.5 Configure `Link` with `shlwapi.lib;wininet.lib`
- [x] 2.6 Create `EdgeViewer.Tests/main.cpp` with Catch2 main
- [x] 2.7 Create `EdgeViewer.Tests/pch.h` and `pch.cpp`
- [x] 2.8 Add the test project to `EdgeViewer.sln`

## 3. Test helpers

- [x] 3.1 Add `EdgeViewer.Tests/TestHelpers/IniBuilder.h`
- [x] 3.2 Add `EdgeViewer.Tests/TestHelpers/TempDir.h`

## 4. Tier 1 — pure helper tests

- [x] 4.1 Write `EdgeViewer.Tests/tier1_helpers.cpp`
- [x] 4.2 Test `to_int(std::string)` parser
- [x] 4.3 Test `ProcessorInterface::replacePlaceholders`
- [x] 4.4 Test `ProcessorInterface::isType`
- [x] 4.5 Test `Navigator::jsEscape`
- [x] 4.6 Test `DirProcessor::stripTwodots`
- [x] 4.7 Test `DirProcessor::extensionsToMaskRegex`
- [x] 4.8 Test `HtmlProcessor::detectedFromBom`
- [x] 4.9 Test `HtmlProcessor::detectedFromMeta`
- [x] 4.10 Test `HtmlProcessor::detectedCharset`

## 5. Tier 2 — ini-seeded helper tests

- [x] 5.1 Write `EdgeViewer.Tests/tier2_config.cpp`
- [x] 5.2 Test `GlobalSettings` lazy-load with `[Chromium] UserDir` missing
- [x] 5.3 Test per-type CSS section reads
- [x] 5.4 Test `[Directory]` complete section read
- [x] 5.5 Test `[Extensions]` section read
- [x] 5.6 Move the `isType` test from task 4.4 here
- [x] 5.7 Test `ForcedHtmlExt` matches

## 6. Tier 3 — Win32-bound path tests

- [x] 6.1 Write `EdgeViewer.Tests/tier3_paths.cpp` test `GetPhysicalPath` returns the original path unchanged for a plain absolute path with no `\\?\` prefix. Use `TempDir` fixture. Tag `[t3]`.
- [x] 6.2 Test `GetPhysicalPath` strips the `\\?\` prefix from a normalized path: input `\\?\C:\path\file.md`, output `C:\path\file.md`. Tag `[t3]`. Reference `specs/temp-file-management/spec.md` (path prefix stripping requirement).
- [x] 6.3 Test `GetPhysicalPath` strips the `\\?\UNC\` prefix: input `\\?\UNC\server\share\file.md`, output `\\server\share\file.md`. Tag `[t3]`.
- [x] 6.4 Test `GetPhysicalPath` copies a UNC-path file to temp when the source is a real file under `\\?\UNC\` (set up via a local network share mock OR a UNC-path-shaped fixture created under `TempDir`). The returned path is in the system temp directory and has the original extension. The copy's bytes match the source's bytes. Tag `[t3]`. (If a real UNC fixture is infeasible on CI runners, mark with a `[!nonportable]` tag and disable by default; for now we run locally.)
- [x] 6.5 Test `GetPhysicalPath` triggers `ForcedHtmlExt` temp-copy when the file extension matches `xml|xhtml`: a fixture `.xml` file is copied to a temp file ending in `.html`. Tag `[t3]`. Reference `specs/temp-file-management/spec.md` (ForcedHtmlExt temp-copy requirement).
- [x] 6.6 Test `GetPhysicalPath` does NOT trigger ForcedHtmlExt when the extension doesn't match: a `.txt` file is NOT copied. Tag `[t3]`.
- [x] 6.7 Test `GenTempFile` + `RemoveTempFiles` cycle: generate a temp file from a known source, assert it exists on disk and is tracked; call `RemoveTempFiles`; assert the file no longer exists. Tag `[t3]`.
- [ ] 6.8 Test `urlPathW` URL-escaping: a plain ASCII path round-trips unchanged (after percent-decode by the test); a path with spaces returns `%20`; a path with `#` uses the placeholder dance — input `file #1.md` produces a URL with `%23` and the placeholder string is replaced back. Tag `[t3]`. Reference `specs/virtual-host-mapping/spec.md`.
- [ ] 6.9 Test symlink resolution in `GetPhysicalPathForLink` using `TempDir` to create a symlink to a known target file; the returned path points to the real target. Tag `[t3]`. (Skipped if the test runner lacks permission to create symlinks; use the `TempDir` RAII and the `[!requiresadmin]` tag convention.)
- [x] 6.10 Test `GetPhysicalPathForLink` returns the original path when the input does not exist (no symlink to resolve): the function falls back gracefully. Tag `[t3]`.

## 7. Tier 4 — extraction tests

- [x] 7.1 Write `EdgeViewer.Tests/tier4_extractions.cpp` test `BuildDetectString`
- [x] 7.2 Test `BuildDetectString` with `[Extensions] Dirs=1`
- [x] 7.3 Test `BuildDetectString` with `[Extensions] Dirs=0` or missing
- [x] 7.4 Test `BuildDetectString` with a custom `[Extensions]`
- [x] 7.5 Test pure `ZoomHotkeyHandled`
- [x] 7.6 Test `ZoomHotkeyHandled` ceiling
- [x] 7.7 Test `ZoomHotkeyHandled` for keys not in the zoom set
- [x] 7.8 Test `ZoomHotkeyHandled` for unmodified zoom keys
- [x] 7.9 Test `BuildFindScript` produces the expected format
- [x] 7.10 Test `BuildFindScript` with `lcs_findfirst` (1) set
- [x] 7.11 Test `BuildFindScript` escape behavior
- [x] 7.12 Test `BuildPrintScript` returns the literal string

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