# Spec Delta

## ADDED Requirements

### Requirement: QuickView PassTabToTC option

The plugin SHALL support a `[QuickView] PassTabToTC` key read from the cached ini parse. `1` (shipped default) enables quick-view `Tab` forwarding to Total Commander; `0` keeps native in-page `Tab` navigation in all modes. An absent key or absent section SHALL behave as `0` under a sample-less empty configuration and as the shipped sample's value otherwise; values other than `1` SHALL behave as `0`. The key SHALL be Windows-only and ignored on Linux.

#### Scenario: Forwarding enabled by the shipped sample

- **WHEN** `edgeviewer.ini` is created from the shipped sample containing `[QuickView] PassTabToTC=1` and a file is opened in quick view
- **THEN** pressing `Tab` forwards the key to the TC parent instead of navigating page links

#### Scenario: Forwarding disabled by the user

- **WHEN** the user sets `[QuickView] PassTabToTC=0` and opens a file in quick view
- **THEN** pressing `Tab` navigates page links and does not switch TC panels

#### Scenario: Invalid value treated as disabled

- **WHEN** `[QuickView] PassTabToTC=yes` is set
- **THEN** quick-view `Tab` keeps in-page navigation

#### Scenario: Same value on both builds

- **WHEN** the same ini is placed next to the Win32 and x64 DLLs
- **THEN** both builds apply the same `Tab` policy for the same view mode
