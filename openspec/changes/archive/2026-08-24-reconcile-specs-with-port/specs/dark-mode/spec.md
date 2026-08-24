## MODIFIED Requirements

### Requirement: Web engine color scheme

On Windows, the plugin SHALL tell the underlying WebView2 engine about the preferred color scheme, matching the plugin's global dark mode flag. The engine's profile SHALL be set to a DARK or LIGHT preferred color scheme (WebView2 `put_PreferredColorScheme`), affecting engine defaults not covered by the injected style sheet, such as the default background before the document is painted, form controls and scrollbars that follow the `prefers-color-scheme` media query, and the default canvas color.

On Linux, engine-level color-scheme propagation is NOT replicated: Qt Web Engine exposes no public equivalent of `put_PreferredColorScheme`. The Linux build relies on the injected `CSSDark` stylesheet plus the `QGuiApplication::styleHints()->colorScheme()` fallback (see "Linux dark-mode fallback from system color scheme"). Pages that depend on the `prefers-color-scheme` media query rather than the injected CSS will not follow the system theme on Linux.

#### Scenario: dark mode sets engine profile to dark

- **WHEN** the global dark mode flag is true and a new WebView2 control is created for rendering
- **THEN** the engine's preferred color scheme is set to DARK, so engine-default UI matches the dark document styling

#### Scenario: light mode sets engine profile to light

- **WHEN** the global dark mode flag is false and a new WebView2 control is created for rendering
- **THEN** the engine's preferred color scheme is set to LIGHT, so engine-default UI matches the light document styling

#### Scenario: Linux has no engine-level color scheme

- **WHEN** the Linux build renders a document with the dark-mode flag set
- **THEN** the injected `CSSDark` stylesheet is applied, but the engine-level `prefers-color-scheme` is not changed (no public Qt Web Engine API exists)
