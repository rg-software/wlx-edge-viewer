## Why

The Linux port sets `gs_IsDarkMode = ShowFlags & lcp_darkmode` at every
WLX entry point (`DllMain.cpp:75,114,239,265,277`). Total Commander
propagates the `lcp_darkmode` (0x80) bit in `ShowFlags` on Windows, but
Double Commander's Qt6 WLX caller does **not** propagate it on Linux —
confirmed by instrumentation in `openspec/changes/characterize-linux-parity/`
(Row 21): `ShowFlags=0x00` and `lcp_darkmode=0` in both KDE light and
KDE dark themes. Result: every CSSDark override is dead code on Linux,
and dark-mode users see light styling regardless of the system palette.

**Source-level confirmation:** DC's `CallListLoad` in `src/uwlxmodule.pas`
guards the dark-mode injection with a compile-time conditional:

```pascal
{$IF DEFINED(MSWINDOWS) and (DEFINED(LCLQT5) or DEFINED(DARKWIN))}
  if g_darkModeEnabled then
  begin
    ShowFlags:= ShowFlags or lcp_darkmode;
    if g_darkModeSupported then
      ShowFlags:= ShowFlags or lcp_darkmodenative;
  end;
{$ENDIF}
```

On Linux, `MSWINDOWS` is not defined, so this entire block is **compiled
out**. The `ShowFlags` parameter passed to `ListLoad`/`ListLoadW` will
never have bit 7 (`0x80`, `lcp_darkmode`) set, regardless of the
user's desktop theme. The `g_darkModeEnabled` / `g_darkModeSupported`
variables live in `src/platform/win/udarkstyle.pas` (Windows-only unit
using undocumented Win10 `uxtheme.dll` APIs); there is no Linux
equivalent. This is not a bug in DC — it is a deliberate design choice
because the WLX `lcp_darkmode` flag was designed for Win32 dark-mode
APIs that have no Linux counterpart.

The fix is a plugin-side fallback that mirrors the existing Windows
sampling behavior (read once per `ListLoad*`, no real-time palette swap).

## What Changes

- **Modified** (`dark-mode` capability): add a Linux-only fallback that
  reads `QGuiApplication::styleHints()->colorScheme()` and treats
  `Qt::ColorScheme::Dark` as `lcp_darkmode` when the bit is not set in
  `ShowFlags`. Windows behavior is unchanged.
- **New** `ComputeDarkMode(int showFlags)` helper in `DllMain.cpp`'s
  `#else // _WIN32` branch, used at all three Linux `gs_IsDarkMode`
  assignment sites (DoListLoad, ListLoadNextW/Linux, ListPrintW/Linux).
- **Modified** (`Readme.md`): document the dark-mode approach on both
  platforms in a new "Dark mode" section so users know the sampling
  semantics and the platform difference.

## Capabilities

### Modified Capabilities

- `dark-mode`: extend the "Dark mode flag source" requirement with a
  Linux-specific Scenario covering the fallback path. Windows
  scenarios unchanged. The existing 32/64-bit parity requirement
  continues to apply within each platform; the cross-platform parity
  is loose (`lcp_darkmode` is the canonical Windows source, Qt's
  colorScheme() is the canonical Linux source).

## Impact

- **Code**: ~25 LOC added in `EdgeViewer/DllMain.cpp` (helper +
  includes + 3 one-line call-site replacements). Windows builds
  untouched.
- **Build**: no CMake change; the existing Linux `#else // _WIN32`
  block absorbs the new includes.
- **Dependencies**: `<QGuiApplication>` and `<QStyleHints>` from Qt6
  (already a transitive dep via `Qt6::Widgets`/`Qt6::WebEngineWidgets`).
- **Readme**: new section under the platform notes.
- **Systems**:
  - Linux users in dark-themed KDE/GNOME/XFCE: dark CSS overrides now
    activate at load time.
  - Linux users in light themes: no change.
  - Theme changes mid-session: still not picked up (sampled at load
    time only, matching Windows).
  - Windows users: no change (TC still sets the bit).
