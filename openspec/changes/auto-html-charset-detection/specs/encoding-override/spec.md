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

## ADDED Requirements

### Requirement: Unappliable manual pick reverts to auto-detect

A manual Encoding pick whose chosen code page **cannot** be applied to the actual file bytes is not a successful override, so it SHALL NOT leave the view in a false "picked" state. The plugin SHALL revert the view to auto-detection: the engine re-sniffs the pristine bytes (HTML, host-side transcode failure) or the loader keeps the previous render and re-runs its detector (MHT, page-side re-decode failure), and the Encoding submenu SHALL return to its auto state with the detected "Auto: <tag>" hint restored rather than a bare or stuck entry. This is the only case where auto-detection re-fires after a user Encoding-menu interaction: a *successful* manual pick still disables auto for the view.

#### Scenario: Undecodable HTML pick returns to auto

- **GIVEN** an HTML file loaded via its real URL (engine sniffing), e.g. a UTF-8 file
- **WHEN** the user picks a code page that cannot decode the byte-oriented source (e.g. UTF-16LE) and the host transcode fails
- **THEN** the pick is abandoned, the view re-navigates so the engine re-sniffs the pristine bytes, the detected page is shown, and the Auto-detect entry is checked with its "Auto: <tag>" label restored

#### Scenario: Undecodable MHT pick returns to auto

- **GIVEN** an MHT view whose loader previously rendered correctly (auto or declared charset)
- **WHEN** the user picks a code page the loader's page-side re-decode cannot apply (its `__evEncodingApply` render throws)
- **THEN** the view keeps showing the previous bytes (no partial re-render), the failed entry is not shown as checked, and the Auto-detect entry goes back to being checked with its "Auto: <tag>" label restored