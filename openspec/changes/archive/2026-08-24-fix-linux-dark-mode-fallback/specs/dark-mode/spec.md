## ADDED Requirements

### Requirement: Linux dark-mode fallback from system color scheme

On Linux, when the WLX caller does not set `lcp_darkmode` (0x80) in the
`ShowFlags` argument passed to `ListLoad*`, `ListLoadNext*`, or
`ListPrint*`, the plugin SHALL determine the dark-mode flag by consulting
the host application's view of the active system color scheme (Qt's
`QGuiApplication::styleHints()->colorScheme()` on Linux). When that
scheme is `Dark`, the plugin SHALL behave as if `lcp_darkmode` had been
set; otherwise the plugin SHALL behave as if it had been cleared.

- This requirement applies on Linux only. The Windows-side behavior is
  governed by the existing "Dark mode flag source" requirement, which is
  unchanged.
- The fallback is sampled once per `ListLoad*` call, at the same call
  sites that already update `gs_IsDarkMode`. The plugin SHALL NOT
  re-sample on palette/theme changes mid-session (existing listers keep
  their current CSS until the next load).
- The fallback SHALL be a pure OR with the `lcp_darkmode` bit: if the
  bit is set, dark mode is on regardless of the system scheme; if the
  bit is clear, the system scheme decides.

#### Scenario: Linux host in dark theme, lcp_darkmode bit clear

- **WHEN** the user opens a file from Double Commander while the host
  application reports a dark color scheme, and `ShowFlags` does not
  carry `lcp_darkmode`
- **THEN** the plugin's global dark mode flag is set to true and the
  processors select their `CSSDark` overrides

#### Scenario: Linux host in light theme, lcp_darkmode bit clear

- **WHEN** the user opens a file from Double Commander while the host
  application reports a light color scheme, and `ShowFlags` does not
  carry `lcp_darkmode`
- **THEN** the plugin's global dark mode flag is set to false and the
  processors select their `CSS` overrides

#### Scenario: Linux host with lcp_darkmode bit set overrides scheme

- **WHEN** `ShowFlags` carries `lcp_darkmode` regardless of the host's
  reported color scheme
- **THEN** the plugin's global dark mode flag is set to true (the bit
  wins over the fallback)
