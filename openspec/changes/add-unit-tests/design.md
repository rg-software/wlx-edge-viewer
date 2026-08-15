## Context

See `proposal.md` for motivation. The project today has no tests ("verification is a build + manual load" per `AGENTS.md`). The branch `characterize-existing-behavior` adds 18 capability specs baseline-documenting existing behavior with ~110 requirements / ~250 scenarios. Those specs are the contract the executable layer needs to verify; this change builds the harness and the first four tiers (T1 pure helpers, T2 ini-seeded, T3 Win32-bound paths, T4 small pure-function extractions). Tier 5 (mock-IWebView for processor body tests) is gated on the `IWebView` abstraction introduced by `port-to-double-commander-linux`; that's the port's first refactor task. Tier 6 (COM shell mocking for thumbnails / popup menu) is deferred indefinitely.

## Goals / Non-Goals

**Goals:**

- ~50 tests across Tiers 1-4 covering ~12 of the 18 baseline specs by executable assertion.
- Three small pure-function extractions (`BuildDetectString`, pure `ZoomHotkeyHandled`, `BuildFindScript` + `BuildPrintScript`) that improve the Windows source independent of the port.
- A reusable harness that the port's Tier 5 tests plug into.
- Win32 and x64 test binaries produce the same pass/fail set.

**Non-Goals:**

- Tier 5 (mock-IWebView processor tests). Belongs in the port.
- Tier 6 (COM shell mocks for directory thumbnails and popup context menu). Deferred indefinitely; characterize specs are the documentation regression.
- Linux execution. The C/C++ test sources are Linux-safe, but the CMake target that produces them on Linux is introduced by the port change. Today's harness is MSBuild-only.
- Continuous integration. The project doesn't have CI today; the verify step is local runs.
- Test coverage gates on PR merge. The harness is opt-in; the verify step in `tasks.md` runs tests and expects zero failures, but no GitHub-side enforcement is added.

## Decisions

### Decision 1: Catch2 v3 (not doctest)

Comparison summary (also discussed in conversation): Catch2 has broader matchers, longer ecosystem familiarity, and excellent CMake integration via `catch_discover_tests()`. doctest has faster cold compiles and a single-header design, but the syntax is intentionally similar enough that muscle memory transfers. Given the user's existing Catch familiarity and the upcoming cross-platform build (where `catch_discover_tests` and `find_package(Catch2)` are well-trodden paths on both MSBuild and CMake), Catch2 v3 is the lower-friction choice across the lifetime of the project.

**Alternative** considered: doctest (rejected for ecosystem familiarity with the specific developer); GoogleTest (rejected — heavier, requires more boilerplate per test, adds `gtest_main` ceremony).

### Decision 2: Test project compiles the DLL's .cpp files directly; does NOT link the DLL

Linking against `EdgeViewer.dll` would force every helper we want to test through a `.def`-exported entry point, which the WLX exports don't cover (the helpers are internal). Compiling the DLL's `.cpp` files *as part of the test project* exposes internal static functions and class internals for testing without requiring any new export surface. Trade-offs:

- **Reuse**: test project's `.vcxproj` includes a curated subset of `EdgeViewer/*.cpp` (`Globals.cpp`, `Navigator.cpp`, `Processors/ProcessorInterface.cpp`, `Processors/HtmlProcessor.cpp`, `Processors/DirProcessor.cpp` — the ones with helpers we test). Complementary files (`WebView2.cpp`, `EdgeLister.cpp`, `DllMain.cpp`, `Processors/MdProcessor.cpp` etc. — those that pull COM/WebView2/window headers) are *not* compiled into the test project, because they need WebView2/COM headers we don't want to require.
- **Risk**: duplicate-compiling a `.cpp` in both the DLL and the test exe means two copies of any file-scope static. Symptom: file-scope `extern` linkage differs between the two binary units. This is OK because the test exe doesn't *load* the DLL — there's no symbol clash at runtime; the two binaries are completely independent. The shared `.cpp`s compile twice and live in different binaries.
- **Build setup**: the test `.vcxproj` has its own `AdditionalIncludeDirectories` matching the DLL's (mINI, plus the project root), but no `webview2` / `wil` includes (we deliberately exclude files that pull those). It links `shlwapi.lib` / `wininet.lib` for the Tier 3 path tests.

**Alternative**: refactor all testable helpers into a separate static lib linked by both the DLL and the tests. Rejected for now: would expand the change scope into a real archives-restructure; the compile-the-sources-directly approach is the lowest-friction way to ship the harness. The eventual `IWebView` refactor in the port change *will* land that separation naturally (because the shared processors become platform-neutral and the test project picks the abstraction); this change tees that up rather than pre-empts it.

### Decision 3: Three Tier 4 extractions — small refactors, preserve correct behavior, fix clear bugs encountered

All three pull pure logic out of impure sites so it can be unit-tested. The extractions aim to preserve existing behavior; where a clear, observable bug is encountered during the extraction, it is fixed as part of the same change and a regression test is added — fixing and documenting are both preferable to silently carrying the bug forward into the new pure function. Latent bugs that would only manifest on unusual inputs (such as a search pattern containing `'` or `\`) are exactly the kind of defect the testable extraction surfaces; preserving them for "byte-identical behavior" would defeat the purpose of the refactor.

All three pull pure logic out of impure sites so it can be unit-tested. The extracted signatures are:

- `EdgeViewer/WlxDetect.h`:
  ```cpp
  // Build the TC detect string from the [Extensions] section of the ini.
  std::string BuildDetectString(const mINI::INIStructure& ini);
  ```
  Body: same logic as `DllMain.cpp::ListGetDetectString` minus the `strcpy_s`. Caller becomes `strcpy_s(DetectString, maxlen, BuildDetectString(GlobalSettings()).c_str())`. Tested cases from `specs/wlx-contract/spec.md` requirements: detect string format, all standard type sections (HTML, Markdown, AsciiDoc, URL, MHTML, EML, RST, Images, Other), `Dirs=1` appending empty extension for directory matching.

- `EdgeViewer/ZoomHotkey.h`:
  ```cpp
  // Pure: given the key, whether Ctrl is held, and the current zoom factor,
  // return whether the key is a zoom hotkey and, if so, the new zoom factor.
  bool ZoomHotkeyHandled(UINT key, bool ctrlHeld, double currentZoom, double& newZoom);
  ```
  Body: same discrete-step-table logic as `WebView2.cpp::ZoomHotkeyHandled` minus the `controller->get_ZoomFactor` and `controller->put_ZoomFactor` calls. Caller reads the current zoom via the controller, calls the pure function, and if it returns `true`, writes `newZoom` back. Tested cases from `specs/zoom-control/spec.md`: Ctrl+Plus snap up, Ctrl+Minus snap down, Ctrl+0 reset, ceiling at 5.0, floor at 0.25, unmodified keys return `false`.

- `EdgeViewer/Navigator.h` (additions):
  ```cpp
  // Pure: build the window.find() script string from a pattern and TC search params.
  std::wstring BuildFindScript(const std::wstring& pattern, int params);
  // Pure: build the window.print() script string.
  std::wstring BuildPrintScript();
  ```
  Body: same logic as `Navigator::Search` / `Navigator::Print` minus the `mWebView->ExecuteScript(...)` call. Callers become one-liners. Tested cases from `specs/text-search/spec.md` and `specs/print/spec.md`: case-sensitive flag, backwards flag, whole-words flag, find-first reset loop generating the `while(...)` script, default search.

These are mechanical pure-function extractions. The motivation is two-fold: (a) unlock testing today, (b) make the Windows source cleaner by separating pure logic from COM/UI/thread boundaries — which is precisely the portability cleanup the port's `IWebView` work benefits from. The change is riskier than pure-additive test work, but lower-overall-effort than reaching the same refactor through the port's bigger change.

**Bugfix precedent (during tasks 1.3):** While extracting `BuildFindScript` from `Navigator::Search`, the original code's `lcs_findfirst` branch was discovered to use `str` (raw, unescaped) inside the JS template literal — whereas the default form used `jsEscape(str)`. For any search pattern containing `'\` characters, the `findfirst` branch would generate invalid JS (e.g. `while(window.find('don't', ...));` — a syntax error). This was fixed as part of the extraction; `BuildFindScript`'s `findfirst` branch now uses `jsEscape(pattern)` to match the default form. Three regression assertions lock in the corrected behavior. The fix is intentional, documented in the test name, and committed separately from the extraction itself so the bugfix is git-bisect-friendly.

### Decision 4: Test tiers and where each tier belongs

```
Tier  Test type                            When added       Belongs to change
────  ─────────────────────────────────    ──────────────   ────────────────
T1    pure helper tests (no state)         this change      add-unit-tests
T2    ini-seeded helper tests              this change      add-unit-tests
T3    Win32-bound path tests (shlwapi/fs)  this change      add-unit-tests
T4    extractions + tests                  this change      add-unit-tests
T5    mock-IWebView processor body tests   port's Task 2.x  port-to-double-commander-linux
T6    mock-shell/COM (thumbnails, menu)   indefinite       (none)
```

Tiers 5 and 6 are part of the contract but out of this change's scope; the `test-harness` spec will mention them so future changes know the tier model.

### Decision 5: Layout under `EdgeViewer.Tests/`

```
EdgeViewer.Tests/
  EdgeViewer.Tests.vcxproj       new MSBuild project (Win32 + x64, Debug + Release)
  EdgeViewer.Tests.vcxproj.filters
  main.cpp                       Catch2 main with custom reporters / loggers
  pch.h / pch.cpp                precompiled header for Catch2 (compile-time win)
  TestHelpers/IniBuilder.h       small DSL to build mINI::INIStructure in-memory
                                  for T2 tests (default + custom CSS, DirImageExt, etc.)
  TestHelpers/TempDir.h          RAII temp directory for T3 path tests
  tier1_helpers.cpp              ~15 TEST_CASEs covering pure helpers
  tier2_config.cpp               ~8 TEST_CASEs covering ini-seeded helpers
  tier3_paths.cpp                ~10 TEST_CASEs covering Win32-bound path helpers
  tier4_extractions.cpp          ~15 TEST_CASEs covering the three extractions
readme.md (in this folder)         brief: how to build/run tests, what each tier
                                  covers, what's excluded
```

### Decision 6: vcpkg.json deps — add `catch2`, no version pin

```json
{
  "dependencies": ["webview2", "wil", "catch2"],
  "builtin-baseline": "e9c53cd6c198a5c16c2e249ae67f5a73aab84b17"
}
```

`catch2` will resolve to whatever vcpkg ships at the existing baseline; no per-port version override. Static triplets (`x86-windows-static`, `x64-windows-static`) are already set in the main `.vcxproj`; the test `.vcxproj` uses the same triplets via solution-level vcpkg integration.

### Decision 7: Verifying — run tests for both Win32 and x64

After build, the project expects:
- `Build\EdgeViewer.Tests_Win32_Release\EdgeViewer.Tests.exe --reporter=console::srng` exits 0 with all tests passing.
- `Build\EdgeViewer.Tests_x64_Release\EdgeViewer.Tests.exe --reporter=console::srng` exits 0 with all tests passing.
- The two binaries produce the same pass/fail set (no platform-specific test divergence today).

There is no automated platform-equivalence check beyond manual diff of the two outputs; a divergence would be a test failure on the diverging platform.

## Risks / Trade-offs

**[Risk] Compiling shared `.cpp` files in both the DLL and the test exe doubles the build work and may surface One Definition Rule issues if any shared `.cpp` has unnamed-namespace types referenced from headers.** Mitigation: chose the subset carefully — only `.cpp` files with simple static helpers and no link-time globals are compiled into both; verify after build. The build is the verification; if a shared TU breaks ODR, the build fails immediately.

**[Risk] The three Tier 4 extractions change production Windows source.** Mitigation: extractions are pure refactors with no intended observable change. Generated detect strings, generated script strings, and computed zoom factors MUST be byte-identical to the inlined versions; the tests assert exact-string equality where possible. A behavioral drift in the Windows DLL is the regression this change is supposed to prevent, not introduce.

**[Risk] vcpkg `catch2` version drift** when vcpkg baseline advances. Mitigation: `builtin-baseline` is already pinned in `vcpkg.json`. Catch2 v3 has stable API; upgrading within v3.x has historically been safe. The `annual` vcpkg baseline will not advance unexpectedly because the baseline SHA is committed.

**[Risk] Tests reach hundreds in the future and slow the build.** Mitigation: Catch2 v3 supports precompiled headers and decoupled test compilation. The `pch.h` setup at T4 launch keeps cold compile reasonable. Long term, test runtime scales linearly; this is not a concern for ~50 tests.

**[Trade-off] No CI integration yet.** Tests run by an engineer before merging; there's no PR-side enforcement. This matches the project's existing "verification is a build + manual load" culture, just one run deeper. Adding CI is independent future-work.

**[Trade-off] The Tier 4 extractions are technically out-of-scope for a "test harness" change.** Including them is the small risk the user accepted: threading pure-function reorganizations through the harness change saves time vs doing them separately. The reward is testability of three areas that would otherwise wait for the port.

## Migration Plan

1. Branch from `master` (already done as `add-unit-tests`).
2. Land Tier 4 extractions first in `EdgeViewer/` (3 small commits, each extracting one pure helper and updating the call site). Run the Windows build (`BuildMakeSetup.bat` workflow) after each extraction; ensure binary-equivalence-or-equivalence in behavior.
3. Add `catch2` to `vcpkg.json`; ensure vcpkg integration installs it on next MSBuild invocation.
4. Create the test project; link and copy the minimal `.cpp` subset from `EdgeViewer/` to compile into the test exe.
5. Implement T1 → T2 → T3 → T4 tests in order.
6. Run tests on Win32 + x64; expect zero failures.
7. Tag a release checkpoint on the branch (`test-harness-stable`).
8. Once archived, the port branch rebases against master and adds Tier 5 tests during its Task 2.x.

Rollback: each tier is in its own commit; the harness and extractions can be reverted independently. A Tier 4 extraction revert requires restoring the inline logic in `DllMain.cpp` / `WebView2.cpp` / `Navigator.cpp`; mechanical.

## Open Questions

- **Q1**: When Tier 5 (mock-IWebView processor body tests) lands in the port, should the mock live in `EdgeViewer.Tests/Mocks/MockWebView.h` or in `EdgeViewer/Tests/Mocks/` (so the DLL-side mocks can also use it for additional internal tests)? Default: `EdgeViewer.Tests/Mocks/` (test-side concern). Defer the final answer to the port change; either layout works.
- **Q2**: Should the harness include a fast "smoke" subset tag (`[smoke]`) — the ~10 most important tests that catch the most regressions — for quick iteration? Default in tasks.md: add `[smoke]` tags to T1 + T4 tests (T1 because pure helpers are the densest regression surface; T4 because the extractions are the only behavior-change sites in this change). Defer to implementation-time judgment.