## MODIFIED Requirements

### Requirement: Encoding menu reflects the active state

The "Encoding" submenu on HTML and MHT views SHALL visually indicate the currently active encoding as an exclusive checked entry (Windows: radio items; Linux: checkable actions): "Auto-detect" is checked by default; after an automatic charset correction (see `charset-autodetect`) the "Auto-detect" entry remains checked but its label SHALL carry the suggested code page (e.g. "Auto-detect (Windows-1251)"); after a manual pick, the chosen entry is checked and auto-detection is inactive. On MHT views the suggested code page SHALL be surfaced through the same label whichever loader path produced it — a `CMD_AUTO_ENCODING` provisional re-apply or a `CMD_AUTO_ENCODING_REPORT` (declared charset decodes cleanly, or detector agrees with the declaration). The menu SHALL be rebuilt from the backend's current state on every open so the check reflects the live view. Behavior SHALL be identical on Win32 and x64.

#### Scenario: Auto-detect checked by default

- **WHEN** the user opens the Encoding submenu on a freshly-opened HTML/MHT view
- **THEN** "Auto-detect" is the checked entry

#### Scenario: Auto-suggested code page is shown

- **WHEN** the view was auto-corrected to windows-1251 (see `charset-autodetect`)
- **THEN** the checked entry reads "Auto-detect (Windows-1251)" and remains provisional until the user interacts

#### Scenario: MHT reports the resolved code page on the label

- **WHEN** an MHT view is loaded whose declared charset decodes the payload cleanly (or the detector agrees with the declaration) so no re-render occurs
- **THEN** the checked Auto-detect entry reads "Auto-detect (<resolved code page, e.g. utf-8>)" and the view is unchanged

#### Scenario: Manual pick is checked and disables auto

- **WHEN** the user picks "KOI8-R" from the Encoding submenu
- **THEN** "KOI8-R" is the checked entry and auto-detection does not re-fire for this view