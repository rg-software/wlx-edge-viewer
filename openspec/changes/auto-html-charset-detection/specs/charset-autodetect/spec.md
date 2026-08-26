## Purpose

Automatically correct mojibake in legacy-encoded HTML files (no BOM, no charset declaration) by statistically detecting the code page from the file's pristine bytes and, only when the engine's own decision is wrong, re-rendering through the host-side decoder — without ever mutating the live DOM or clobbering the user's manual encoding choice.

## ADDED Requirements

### Requirement: Automatic charset detection on HTML views

When an HTML file **without an explicit encoding declaration** is rendered (and `[HTML] ForceDetectEncoding` is unset/0), the plugin SHALL run a statistical charset detector over the pristine file bytes and compare its confident guess to the web engine's actual decode decision (`document.characterSet`). If they disagree (and the guess is high-confidence), the plugin SHALL issue a single provisional host-side re-decode via the existing encoding-override path (`ApplyCharsetOverride`). The detection SHALL be performed page-side over the already-injected pristine bytes — no live DOM mutation, no re-decode logic in the page. The behavior SHALL be identical on Windows (WebView2) and Linux (Qt Web Engine).

#### Scenario: Windows-1251 file is auto-corrected

- **GIVEN** a windows-1251 HTML file with no charset declaration, which Chromium sniffs as UTF-8 (mojibake)
- **WHEN** the detector's high-confidence guess is windows-1251 and `document.characterSet` disagrees
- **THEN** the view re-renders through the host-side decoder and displays correct Cyrillic

#### Scenario: No correction when the engine is right

- **GIVEN** a UTF-8 HTML file with no charset declaration
- **WHEN** the detector's guess agrees with `document.characterSet`
- **THEN** no re-render occurs (zero flicker) and the engine's decision stands

#### Scenario: Declared encoding is not second-guessed by default

- **GIVEN** `[HTML] ForceDetectEncoding` is unset (0) and an HTML file has a UTF-8 BOM or `<meta charset="utf-8">`
- **WHEN** the document renders
- **THEN** the plugin SHALL NOT run auto-detection (the source declaration is authoritative)

### Requirement: ForceDetectEncoding override

When `[HTML] ForceDetectEncoding=1` is set, auto-detection SHALL run even for HTML files that carry an explicit encoding declaration (BOM or `<meta charset>`/`http-equiv`). It SHALL still compare the detector's high-confidence guess against `document.characterSet`: if they agree, nothing happens (zero flicker); only a high-confidence **disagreement** triggers the single provisional host-side re-decode. The value 0 SHALL restore the default "declared encoding is authoritative" behavior. MHT views SHALL be unaffected by this setting (their loader re-decodes page-side and never participates in auto-detection).

#### Scenario: Wrong declared charset is corrected under force

- **GIVEN** `ForceDetectEncoding=1` and a windows-1251 HTML file that declares `<meta charset="utf-8">` (so Chromium decodes as UTF-8 → mojibake)
- **WHEN** the document renders
- **THEN** the detector's high-confidence windows-1251 guess (disagreeing with `document.characterSet`=utf-8) triggers the provisional re-decode and the view displays correct Cyrillic

#### Scenario: Genuine declared file is untouched under force

- **GIVEN** `ForceDetectEncoding=1` and a real UTF-8 HTML file with `<meta charset="utf-8">`
- **WHEN** the document renders
- **THEN** the detector agrees with `document.characterSet`=utf-8, so no re-render occurs and the view is unchanged

#### Scenario: Flag off keeps declared files untouched

- **GIVEN** `ForceDetectEncoding=0` (or unset) and a windows-1251 HTML file that declares `<meta charset="utf-8">`
- **WHEN** the document renders
- **THEN** auto-detection is suppressed (the browser keeps the mojibake; the manual Encoding menu remains the fix)

### Requirement: Provisional, non-destructive auto override

The automatically-applied re-decode SHALL be marked as auto (not user-chosen). The pristine source bytes SHALL remain cached exactly as loaded. Picking "Auto-detect" from the Encoding menu SHALL restore the engine's sniffed render (removing the auto override). The override SHALL be transient and reset on any navigation/reopen.

#### Scenario: Reset to engine sniffing after auto-correct

- **GIVEN** a windows-1251 file auto-corrected to windows-1251
- **WHEN** the user picks "Auto-detect" from the Encoding menu
- **THEN** the view re-renders the pristine bytes with engine-default sniffing (mojibake again), because auto is provisional

#### Scenario: Auto does not survive reopen

- **GIVEN** a windows-1251 file auto-corrected to windows-1251
- **WHEN** the file is reopened in a fresh lister window (or another file is loaded)
- **THEN** the view starts fresh from engine sniffing and auto-detection runs anew

### Requirement: Manual selection overrides auto

A user-selectable encoding choice SHALL always take precedence over auto-detection. Once the user picks an encoding from the Encoding menu for the current view, auto-detection SHALL NOT fire again for that view.

#### Scenario: Manual pick sticks

- **GIVEN** a file auto-corrected to windows-1251
- **WHEN** the user then picks "KOI8-R" from the Encoding menu
- **THEN** the view re-renders with KOI8-R and auto-detection does not fire again for this view

### Requirement: Encoding menu reflects the active state

The Encoding submenu SHALL visually indicate the current encoding state as a checked entry: "Auto-detect" by default, the auto-suggested code page indicated (e.g. "Auto-detect (Windows-1251)") after an auto correction, and the user-selected entry checked after a manual pick. Checking SHALL be identical on Windows (radio items) and Linux (checkable actions).

#### Scenario: Auto-suggested state is visible

- **GIVEN** a file auto-corrected to windows-1251
- **WHEN** the user opens the Encoding submenu
- **THEN** "Auto-detect (Windows-1251)" is the checked entry

#### Scenario: Manual pick is checked

- **GIVEN** a file where the user picked "Windows-1251"
- **WHEN** the user opens the Encoding submenu
- **THEN** "Windows-1251" is the checked entry and auto-detection is inactive for this view