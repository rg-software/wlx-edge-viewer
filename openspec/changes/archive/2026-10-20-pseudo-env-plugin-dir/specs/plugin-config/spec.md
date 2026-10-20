# Spec Delta

## MODIFIED Requirements

### Requirement: UserDir fallback default

When the `[WebView]` section omits the `UserDir` key entirely, the plugin MUST default `UserDir` to the plugin DLL's own directory (so the WebView2 profile lives next to the plugin). When `UserDir` is present, the plugin MUST use the supplied value after expanding `%ENV%` placeholders (so the shipped `%USERPROFILE%` resolves to the user's profile directory). The fallback SHALL be applied at first parse and stored in the cache, and SHALL resolve the same value on 32-bit and 64-bit builds.

#### Scenario: UserDir key absent

- **WHEN** `edgeviewer.ini`'s `[WebView]` section has no `UserDir` key
- **THEN** the plugin DLL's directory is used as the WebView2 user-data directory

#### Scenario: UserDir key present with environment variable

- **WHEN** `[WebView] UserDir=%USERPROFILE%\EdgeViewer`
- **THEN** the WebView2 user-data directory resolves to `<expanded USERPROFILE>\EdgeViewer`

## ADDED Requirements

### Requirement: Pseudo-environment variables in path values

Path-valued ini values that pass through `ExpandEnv` MUST additionally resolve the following pseudo-environment variables, which are not provided by the operating system:

- `%PluginDir%` (Windows) / `$PluginDir` and `${PluginDir}` (Linux) MUST resolve to the directory containing the loaded plugin DLL (`GetModulePath()`).
- `%UserDir%` (Windows) / `$UserDir` and `${UserDir}` (Linux) MUST resolve to the configured `[WebView] UserDir` value after its own expansion, falling back to the plugin directory when `UserDir` is unset.

Expansion MUST be bounded (a small fixed number of passes) so that a self-referential value such as `UserDir=%UserDir%` cannot loop indefinitely; an unresolved or recursive value SHALL be left unexpanded rather than crashing. Pseudo-variable resolution SHALL behave identically on 32-bit and 64-bit builds.

#### Scenario: PluginDir resolves to install directory

- **WHEN** `[WebView] BrowserExecutableX64Folder=%PluginDir%\WebView2`
- **THEN** the pinned browser folder resolves to `<plugin directory>\WebView2`

#### Scenario: UserDir reusable from another key

- **WHEN** `[WebView] UserDir=%PluginDir%\Profile` and `[WebView] BrowserExecutableX64Folder=%UserDir%\Edge`
- **THEN** the browser folder resolves to `<plugin directory>\Profile\Edge`

#### Scenario: Self-reference does not hang

- **WHEN** `[WebView] UserDir=%UserDir%`
- **THEN** expansion terminates after the bounded number of passes and the value is left as-is (no crash, no infinite loop)