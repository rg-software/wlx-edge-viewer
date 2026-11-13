# Spec Delta

## ADDED Requirements

### Requirement: ListLoad records per-view quick-view mode

On `ListLoadW`, the plugin SHALL record whether the view is hosted in quick view by testing the `WS_CHILD` style bit on the supplied parent window (set = quick view, clear = standalone F3 lister). The mode SHALL be stored per lister view for the view's lifetime, refreshed on `ListLoadNextW`, and cleared on `ListCloseWindow`. Multiple concurrent lister windows SHALL each keep their own mode.

#### Scenario: Quick view detected from a child parent

- **WHEN** TC opens a file with a `WS_CHILD`-style parent window
- **THEN** the view is recorded as quick view and the quick-view `Tab` policy applies to it

#### Scenario: F3 lister detected from a top-level parent

- **WHEN** TC opens a file with a parent window lacking `WS_CHILD`
- **THEN** the view is recorded as standalone lister and `Tab` keeps page navigation

#### Scenario: Mode follows navigation and close

- **WHEN** TC navigates the same view to another file or closes the view
- **THEN** the recorded mode persists across the navigation and is dropped on close

#### Scenario: Mixed modes coexist

- **WHEN** an F3 lister and a quick-view panel are open at the same time
- **THEN** each view applies its own mode's `Tab` behavior independently
