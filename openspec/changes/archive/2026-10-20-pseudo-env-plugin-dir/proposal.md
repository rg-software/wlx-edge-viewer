# Proposal

## Why

`[WebView] UserDir` and other path-valued ini keys are expanded with the OS
environment-variable mechanism (`ExpandEnvironmentStringsW` on Windows,
`$VAR`/`${VAR}` on Linux), but the plugin's own install directory is not an
OS environment variable. Users therefore cannot write paths relative to the
plugin (e.g. a WebView2 profile or a pinned browser folder living next to the
DLL) without hard-coding an absolute path, which breaks when the plugin is
moved or updated into a different folder. The issue asks for a pseudo-env
variable that resolves to the plugin directory; the user additionally wants
the resolved `UserDir` value to be reusable from other ini values.

## What Changes

- Introduce a pseudo-environment variable `%PluginDir%` (and the equivalent
  `$PluginDir`/`${PluginDir}` form on Linux) that expands to the directory
  containing the loaded plugin DLL (`GetModulePath()`), available in every
  path-valued ini value that goes through `ExpandEnv`.
- Make the resolved `UserDir` value available the same way as `%UserDir%`
  (and `$UserDir`/`${UserDir}`), so a value such as
  `BrowserExecutableX64Folder=%UserDir%\Edge` works. `%UserDir%` resolves
  to the configured `UserDir` after its own `%PluginDir%`/`%ENV%` expansion,
  with the plugin directory as the fallback when `UserDir` is unset.
- Guard expansion against self-reference/infinite loops (a bounded number of
  passes; an unresolved/recursive value is left as-is rather than crashing).
- Document the new pseudo-variables in the `plugin-config` spec and the
  shipped `Resources/edgeviewer.ini` template.

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `plugin-config`: extend the `%ENV%` expansion requirement to cover the
  `%PluginDir%` and `%UserDir%` pseudo-variables, with resolution and
  recursion-guard semantics.

## Impact

- Affected code: `EdgeViewer/Platform_Win.cpp` (`ExpandEnv`),
  `EdgeViewer/Platform_Linux.cpp` (`ExpandEnv`), `EdgeViewer/Globals.cpp`
  (UserDir fallback / pseudo-value source), and the ini template
  `Resources/edgeviewer.ini`.
- No new dependencies; no vcpkg changes.
- Backward compatible: existing `%ENV%` values keep working; the new
  pseudo-variables are opt-in.