# test-harness Specification

## Purpose
Defines the executable verification layer for the EdgeViewer Lister plugin: which test tiers exist, what each tier covers, what framework is used, what the coverage rule is for new helper code before merge, and the Win32/x64 parity guarantee that the test layer enforces. The harness is the executable counterpart to the behavior specs under `openspec/specs/`; together they form the project's regression net before the cross-platform port.
## Requirements
### Requirement: Test framework

The project SHALL use Catch2 v3 as the unit/feature test framework. Catch2 SHALL be pulled via vcpkg manifest mode (`catch2` entry in `vcpkg.json`) with the existing static triplets (`x86-windows-static`, `x64-windows-static`). Tests SHALL be authored using `TEST_CASE`, `SECTION`, and `REQUIRE`/`CHECK` macros.

#### Scenario: Adding a new dependency for tests

- **WHEN** the project needs an additional test-only library
- **THEN** it SHALL be added to `vcpkg.json` alongside `catch2`, honoring the existing static-triplet policy and `builtin-baseline` pin; no ad-hoc vendoring of libs

#### Scenario: Test uses Catch2 macros

- **WHEN** an engineer authors a new test case
- **THEN** the test uses `TEST_CASE("[tier] description")` with a tier tag (one of `t1`, `t2`, `t3`, `t4`, `t5`, `t6`), `SECTION` for sub-cases, and `REQUIRE` for assertions that abort the test on failure

### Requirement: Test tier model

The harness SHALL classify tests into six tiers that reflect what infrastructure the test depends on. Tiers 1 (pure helpers, no state), 2 (ini-seeded helpers), 3 (Win32-bound path helpers), and 4 (extractions + tests) SHALL be present in this change. Tier 5 (mock-IWebView processor body tests) SHALL be reserved for the cross-platform port change that introduces `IWebView`. Tier 6 (mock-Windows-shell for thumbnails and popup menu) SHALL be deferred indefinitely; the specs continue to act as the documentation regression for those features.

#### Scenario: Tier 1 test placement

- **WHEN** an engineer adds a test for a pure helper with no dependencies (e.g. `to_utf8`, `replacePlaceholders`)
- **THEN** the test SHALL be placed in `EdgeViewer.Tests/tier1_helpers.cpp` and tagged `[t1]`

#### Scenario: Tier 3 test placement

- **WHEN** an engineer adds a test that touches Win32 file APIs (`GetPhysicalPath`, `GenTempFile`, `urlPathW` via `shlwapi`)
- **THEN** the test SHALL be placed in `EdgeViewer.Tests/tier3_paths.cpp` and tagged `[t3]`; it MAY use `TestHelpers/TempDir.h` for filesystem fixtures

#### Scenario: Tier 5 deferred to port

- **WHEN** an engineer wants to assert that a processor's `OpenIn` calls specific `IWebView` methods with specific arguments
- **THEN** the test SHALL be added in the `port-to-double-commander-linux` change (not here), because it depends on the `IWebView` mock that the port's refactor introduces

#### Scenario: Tier 6 deferred indefinitely

- **WHEN** an engineer wants executable regression coverage for the directory shell thumbnail generation or the popup context menu
- **THEN** the test is not added; the characterization specs under `specs/directory-view/spec.md` and `specs/popup-context-menu/spec.md` serve as the documentation contract

### Requirement: Test project build

The test project SHALL be a separate MSBuild project (`EdgeViewer.Tests/EdgeViewer.Tests.vcxproj`) in `EdgeViewer.sln`. It SHALL support all four `Configuration|Platform` combinations that the main DLL supports (`Debug|Win32`, `Release|Win32`, `Debug|x64`, `Release|x64`). It SHALL compile against the same C++ standard (`stdcpplatest`) as the main project and use the same static-CRT runtime library selection. The test project SHALL NOT require WebView2-specific headers, WIL, or `windows.h` in its public include path.

#### Scenario: Building the test project

- **WHEN** an engineer runs `msbuild EdgeViewer.sln /t:EdgeViewer.Tests /p:Configuration=Release /p:Platform=x64`
- **THEN** `EdgeViewer.Tests.exe` is produced under `winbuild\EdgeViewer.Tests_x64_Release\` without requiring WebView2 tooling

#### Scenario: Test project uses static CRT

- **WHEN** the main project is built with `MultiThreaded` (Release) or `MultiThreadedDebug` (Debug)
- **THEN** the test project SHALL use the same runtime library for the matching configuration

### Requirement: Source compilation strategy

The test project SHALL compile a curated subset of `EdgeViewer/*.cpp` files directly into the test executable rather than linking against `EdgeViewer.dll`. The subset SHALL include only files whose helpers we test (e.g. `Globals.cpp`, `Navigator.cpp`, `Processors/ProcessorInterface.cpp`, `Processors/HtmlProcessor.cpp`, `Processors/DirProcessor.cpp`) and SHALL exclude files that require WebView2 or WIL headers (e.g. `WebView2.cpp`, `EdgeLister.cpp`, `DllMain.cpp`). This strategy exposes internal linkage helpers for testing without adding new exports to the DLL.

#### Scenario: Internal helper is testable

- **WHEN** a helper function has internal (`static`) linkage in `Globals.cpp`
- **THEN** the test project, which compiles `Globals.cpp` directly, can call that helper without needing a new DLL export

#### Scenario: WebView2-dependent file is not compiled into tests

- **WHEN** the test project is built
- **THEN** no file that includes `<webview2.h>` or `<wil/com.h>` is compiled into the test executable, eliminating the WebView2 SDK dependency from the test build

### Requirement: Win32 and x64 test parity

The Win32 test executable and the x64 test executable SHALL produce the same pass/fail set for every test authored in this change. No test in Tiers 1-4 SHALL intentionally diverge between the two platforms. If a divergence is observed, it SHALL be a test failure (a bug or an unspecified platform-specific behavior); the divergence SHALL NOT be suppressed by platform-conditional test code.

#### Scenario: Same pass set on both platforms

- **WHEN** `EdgeViewer.Tests.exe` is built and run for both Win32 and x64 Release configurations
- **THEN** the set of passing tests is identical between the two builds

#### Scenario: Divergence is a failure

- **WHEN** a test passes on one platform and fails on the other
- **THEN** the change SHALL NOT add `#ifdef _WIN64` branching to silence the divergence; the underlying cause SHALL be fixed or the test removed (with a corresponding spec update)

### Requirement: New helper coverage rule

When a new pure or nearly-pure helper is introduced into `EdgeViewer/` source, the change that introduces the helper SHALL include a corresponding test in `EdgeViewer.Tests/` at the appropriate tier. The harness is not a coverage gate enforced by tooling, but a convention: code review SHALL verify a test exists. The helper's spec (if any) MAY be referenced from the test name to keep the traceability explicit.

#### Scenario: New helper is added without a test

- **WHEN** an engineer adds a new pure helper function to `Globals.cpp` and submits a PR without a corresponding test
- **THEN** the reviewer SHALL request a test be added before merging; the convention is that helpers carry their tests and tests carry their helpers

### Requirement: Smoke tier tagging

The harness SHALL support a `[smoke]` tag that selects a subset of tests for quick iteration. Tests covering the pure helpers (Tier 1) and the three extractions (Tier 4) SHALL carry the `[smoke]` tag in addition to their tier tag, so `EdgeViewer.Tests.exe "[smoke]"` runs the densest regression subset.

#### Scenario: Quick smoke run

- **WHEN** an engineer invokes `EdgeViewer.Tests.exe "[smoke]"` to get fast signal during iteration
- **THEN** only `t1` and `t4` tests run, completing in a small fraction of the full test suite time

#### Scenario: Full run

- **WHEN** an engineer invokes `EdgeViewer.Tests.exe` with no filter
- **THEN** all tests in all tiers run

### Requirement: Test artifact output

The test executable SHALL live under `winbuild\EdgeViewer.Tests_<Platform>_<Configuration>\EdgeViewer.Tests.exe`, mirroring the main project's `winbuild\EdgeViewer_<Platform>_<Configuration>\` layout (so build outputs stay grouped and gitignored by the existing `winbuild/` exclusion).

#### Scenario: Test exe path

- **WHEN** the test project is built for `Release|x64`
- **THEN** the resulting binary is `winbuild\EdgeViewer.Tests_x64_Release\EdgeViewer.Tests.exe`

### Requirement: Extraction purity contract (Tier 4)

The three Tier 4 extractions (`BuildDetectString`, pure `ZoomHotkeyHandled`, `BuildFindScript`, `BuildPrintScript`) SHALL be pure functions: no global state access, no COM calls, no heap allocation of objects with non-trivial destructors beyond the function scope, no I/O. The original call sites SHALL retain the impure operations (reading zoom factor from the controller, executing the script on the web view, copying the detect string to the caller's buffer in `ListGetDetectString`). The extracted functions SHALL be testable without instantiating any WebView2 or COM object.

#### Scenario: Extraction has no global state access

- **WHEN** the test project compiles a Tier 4 extracted function in isolation
- **THEN** the function does not reference `gs_IsDarkMode`, `gs_Views`, `GlobalSettings()`, or any other global; it receives all inputs as parameters

#### Scenario: Caller retains impure operation

- **WHEN** `DllMain.cpp::ListGetDetectString` calls `BuildDetectString`
- **THEN** the caller still owns the `strcpy_s(DetectString, maxlen, ...)` copy; the extracted function returns a `std::string` and does not touch the caller's buffer

