# EdgeViewer Tests

Unit and feature tests for the EdgeViewer WLX Lister plugin, using [Catch2 v3](https://github.com/catchorg/Catch2).

## Build

The test project is part of `EdgeViewer.sln`:

```
msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=x64
```

Or build the test project directly:

```
msbuild EdgeViewer.Tests\EdgeViewer.Tests.vcxproj /p:Configuration=Release /p:Platform=x64
```

The test executable is produced at:

```
winbuild\EdgeViewer.Tests_<Platform>_<Configuration>\EdgeViewer.Tests.exe
```

## Run

```
winbuild\EdgeViewer.Tests_x64_Release\EdgeViewer.Tests.exe
```

For the smoke subset (fast signal):

```
winbuild\EdgeViewer.Tests_x64_Release\EdgeViewer.Tests.exe "[smoke]"
```

## Test tiers

| Tier | Scope | File | Notes |
|------|-------|------|-------|
| T1 | Pure helpers (no global state) | `tier1_helpers.cpp` | `to_utf8`, `to_utf16`, `to_int`, `replacePlaceholders`, `jsEscape`, `stripTwodots`, `extensionsToMaskRegex`, `detectedFromBom`, `detectedFromMeta`, `detectedCharset` |
| T2 | Ini-seeded helpers (GlobalSettings) | `tier2_config.cpp` | Uses a static initializer to write a test `edgeviewer.ini` before main runs. Tests `[Extensions]`, per-type CSS sections, `[Directory]`, `isType`, `ForcedHtmlExt`. |
| T3 | Win32-bound path helpers | `tier3_paths.cpp` | Uses `TempDir` RAII fixtures. Tests `GetPhysicalPath` prefix stripping, ForcedHtmlExt temp-copy, GenTempFile + RemoveTempFiles lifecycle, GetPhysicalPathForLink fallback. UNC path tests and symlink tests are deferred (substantial fixture cost); characterize specs document behavior. |
| T4 | Pure extractions | `tier4_extractions.cpp` | Tests `BuildDetectString`, pure `ZoomHotkeyHandled`, `BuildFindScript`, `BuildPrintScript` — the three extractions from tasks 1.1-1.3. |
| T5 | Mock-IWebView processor body tests | `tier5_processors.cpp` | Processor `OpenIn` bodies via the `MockWebView` (`IWebView` implementation). |
| T6 | Mock-Windows-shell / offline policy | `tier6_offline.cpp` | Windows-shell-dependent behavior deferred indefinitely; characterize specs document directory thumbnail generation and popup context menu behavior. |

## Win32 and x64 test parity

Both test binaries (`Win32` and `x64` builds) MUST produce the same pass/fail set. No test in Tiers 1-4 intentionally diverges between platforms. If a divergence is observed, it is a test failure or an unspecified platform-specific behavior — the divergence MUST NOT be suppressed by platform-conditional test code.

## What's not covered

- **T5 processor body tests** (e.g., asserting `MdProcessor::OpenIn` calls `RegisterVirtualHost` then `NavigateToString` with expected HTML) already exist in `tier5_processors.cpp` via the `MockWebView`/`IWebView` harness.
- **T6 thumb generation, popup context menu** — require COM shell mocking; too costly for direct test value. The [characterize specs](../openspec/specs/) document behavior.

## Adding new helpers

When a new pure or nearly-pure helper is introduced into `EdgeViewer/` source, the change SHALL include a corresponding test at the appropriate tier. The harness is not a coverage gate enforced by tooling, but a convention: code review SHALL verify a test exists.