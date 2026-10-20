# Tasks

## 1. Windows ExpandEnv pseudo-variable support

- [x] 1.1 Add `%PluginDir%` and `%UserDir%` substitution to `ExpandEnv` in `EdgeViewer/Platform_Win.cpp` before delegating to `ExpandEnvironmentStringsW`, using a bounded loop (max 4 passes) that stops when no change occurs, and verify a `%PluginDir%` value resolves to `GetModulePath()`.
- [x] 1.2 Resolve `%UserDir%` to the configured `[WebView] UserDir` value after its own expansion, falling back to the plugin directory when the key is absent, and verify `%UserDir%` chaining works in a manual load.
- [x] 1.3 Verify self-referential values (`UserDir=%UserDir%`) terminate without hanging or crashing.

## 2. Linux ExpandEnv pseudo-variable support

- [x] 2.1 Extend the `$VAR`/`${VAR}` scanner in `EdgeViewer/Platform_Linux.cpp` to recognize `$PluginDir`/`${PluginDir}` and `$UserDir`/`${UserDir}` before falling through to `getenv`, and verify resolution against `GetModulePath()`.
- [x] 2.2 Apply the same bounded-loop guard as on Windows and verify self-referential values terminate cleanly.

## 3. Documentation and template

- [x] 3.1 Add a commented example using `%PluginDir%`/`$PluginDir` to `Resources/edgeviewer.ini` (e.g. under `[WebView]`) and verify the shipped template still parses.
- [x] 3.2 Update the `plugin-config` spec's UserDir/environment-expansion wording is already captured by the delta; verify `openspec validate` passes for the change.

## 4. Verification

- [x] 4.1 Build Release for both Win32 and x64 (`msbuild EdgeViewer.sln ...`) and confirm zero errors.
- [x] 4.2 Manual load in Total Commander: set `UserDir=%PluginDir%\Profile` and `BrowserExecutableX64Folder=%UserDir%\Edge` (or the 32-bit equivalent), confirm both resolve next to the plugin, and confirm a self-referential value does not hang.