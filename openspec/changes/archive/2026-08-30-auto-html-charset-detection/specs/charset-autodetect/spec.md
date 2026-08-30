## Purpose

Automatically correct mojibake in legacy-encoded HTML files (no BOM, no charset declaration) by statistically detecting the code page from the file's pristine bytes and, only when the engine's own decision is wrong, re-rendering through the host-side decoder — without ever mutating the live DOM or clobbering the user's manual encoding choice.

## ADDED Requirements

### Requirement: Automatic charset detection on HTML views

The plugin SHALL run a statistical charset detector over an HTML file's pristine bytes and compare its confident guess to the web engine's actual decode decision (`document.characterSet`). If they disagree (and the guess is high-confidence), the plugin SHALL issue a single provisional host-side re-decode via the existing encoding-override path (`ApplyCharsetOverride`). The detection SHALL be performed page-side over the already-injected pristine bytes — no live DOM mutation, no re-decode logic in the page. The behavior SHALL be identical on Windows (WebView2) and Linux (Qt Web Engine).

#### Scenario: Windows-1251 file is auto-corrected

- **GIVEN** a windows-1251 HTML file with no charset declaration, which Chromium sniffs as UTF-8 (mojibake)
- **WHEN** the detector's high-confidence guess is windows-1251 and `document.characterSet` disagrees
- **THEN** the view re-renders through the host-side decoder and displays correct Cyrillic

#### Scenario: No correction when the engine is right

- **GIVEN** a UTF-8 HTML file
- **WHEN** the detector's guess agrees with `document.characterSet`
- **THEN** no re-render occurs (zero flicker) and the engine's decision stands

#### Scenario: Wrong declared charset is corrected by default

- **GIVEN** the default `[HTML] ForceDetectEncoding=1` and a windows-1251 HTML file that declares `<meta charset="utf-8">` (so Chromium decodes as UTF-8 → mojibake)
- **WHEN** the document renders
- **THEN** the detector's high-confidence windows-1251 guess (disagreeing with `document.characterSet`=utf-8) triggers the provisional re-decode and the view displays correct Cyrillic

#### Scenario: Genuine declared file is untouched by default

- **GIVEN** the default `ForceDetectEncoding=1` and a real UTF-8 HTML file with `<meta charset="utf-8">`
- **WHEN** the document renders
- **THEN** the detector agrees with `document.characterSet`=utf-8, so no re-render occurs and the view is unchanged

### Requirement: Automatic charset detection on MHT views

MHT views SHALL run statistical detection over the **transfer-decoded payload** of a text part, never over the raw MIME envelope (whose quoted-printable/base64 layer hides the real text bytes — the envelope is pure ASCII even for a Windows-1251 body). Detection SHALL run page-side in the mhtml loader (`mhtml/loader.html`), which owns the pristine bytes. The loader SHALL detect over the first `text/html` part it finds, else the largest `text/*` part; it SHALL undo the chosen part's `Content-Transfer-Encoding` (quoted-printable soft-break/hex decode, base64 whitespace-strip) before running the detector. The guess SHALL be normalized to a menu-representable tag (e.g. jschardet's `x-mac-cyrillic` → `windows-1251`).

The engine-agreement gate used for HTML (`document.characterSet`) does not exist for MHT: the loader decodes the payload through the part's **declared** charset instead. Detection SHALL therefore use a fatal-decode agreement gate — querying whether the declared charset decodes the payload without error. If the declared charset decodes cleanly (or the detected tag equals the declared tag), the load was correct: SHALL NOT re-render, SHALL only post `CMD_AUTO_ENCODING_REPORT|<tag>` so the Encoding submenu surfaces the resolved charset. If the declared charset FAILS to decode (a wrong declaration, e.g. `utf-8` on Windows-1251 bytes), SHALL post a single `CMD_AUTO_ENCODING|<detected-tag>` so the host provisionally re-renders page-side with the detected tag (same page-side machinery as a manual menu pick). Detection SHALL NOT fire on a pure-ASCII payload, when there is no text part, or on a detector confidence below the shared 0.90 threshold, and SHALL NOT fire again after a manual encoding pick for the same view.

The MHT auto path SHALL be gated host-side on the same provisional/non-destructive rules as HTML: it must not mark the user as having chosen, must be transient per logical load, and picking "Auto-detect" from the Encoding menu SHALL re-run MHT detection (reset the page-side one-shot latch) and re-apply/report its result. Behavior SHALL be identical on Windows (WebView2) and Linux (Qt Web Engine), and on Win32 and x64.

#### Scenario: Wrongly-declared MHT is auto-corrected

- **GIVEN** an MHT file whose text part declares `charset="utf-8"` but whose quoted-printable payload is actually Windows-1251 Cyrillic
- **WHEN** the loader renders and runs detection over the transfer-decoded payload
- **THEN** the declared `utf-8` fails to decode the payload, the high-confidence detector guess is `windows-1251`, and the view provisionally re-renders correctly as Windows-1251

#### Scenario: Genuine MHT is untouched

- **GIVEN** an MHT file whose text part is genuinely UTF-8 (or whose declared charset decodes the payload without error)
- **WHEN** the loader renders and runs detection over the transfer-decoded payload
- **THEN** no re-render occurs and the view is unchanged; the loader only reports the resolved charset so the Encoding submenu can label the Auto-detect entry

#### Scenario: Detector stays silent

- **GIVEN** an MHT file whose selected text part is pure ASCII, or which has no text part at all
- **WHEN** the loader renders
- **THEN** no detection message is posted and no re-render occurs

#### Scenario: Auto-detect re-pick re-runs detection on MHT

- **GIVEN** an MHT view previously auto-corrected (or manually re-encoded), showing mojibake
- **WHEN** the user picks "Auto-detect" from the Encoding submenu
- **THEN** the one-shot detection latch is reset, the loader re-runs detection over the transfer-decoded payload, and the detected tag is re-applied (or the hint re-reported)

#### Scenario: Failed manual MHT pick reverts to auto

- **GIVEN** an MHT view whose loader previously rendered correctly (auto or declared charset)
- **WHEN** the user picks a code page that the loader's page-side re-decode cannot apply (its `__evEncodingApply` render throws)
- **THEN** the loader keeps the previous render, posts `CMD_ENCODING_APPLY_FAILED` to the host, and re-runs its detection; the host clears the abortedly-checked entry and re-arms the auto latch, so the Auto-detect entry is checked again with its "Auto: <tag>" hint restored

### Requirement: ForceDetectEncoding disable

`[HTML] ForceDetectEncoding` SHALL default to `1`, making auto-detection run for every HTML file, including those with an explicit encoding declaration (BOM or `<meta charset>`/`http-equiv`). Setting it to `0` SHALL restore the opt-out behavior: a file that carries an explicit declaration is treated as authoritative and is NOT auto-detected (only files without a declaration are). In both cases the detector still only fires on a high-confidence disagreement with `document.characterSet` and never on an agreement. The key is `[HTML]`-scoped and does not apply to MHT views, which follow their own loader-owned auto-detection path (`Automatic charset detection on MHT views`).

#### Scenario: Flag off keeps declared files untouched

- **GIVEN** `ForceDetectEncoding=0` and a windows-1251 HTML file that declares `<meta charset="utf-8">`
- **WHEN** the document renders
- **THEN** auto-detection is suppressed (the browser keeps the mojibake; the manual Encoding menu remains the fix), but the Encoding submenu still labels the Auto-detect entry with the charset the engine actually used (`Auto: utf-8` from `document.characterSet`)

#### Scenario: Flag off still detects undeclared files

- **GIVEN** `ForceDetectEncoding=0` and a windows-1251 HTML file with **no** charset declaration
- **WHEN** the document renders
- **THEN** auto-detection runs normally (disagreement → provisional re-decode) because no declaration suppresses it

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

A user-selectable encoding choice SHALL always take precedence over auto-detection. Once the user picks an encoding from the Encoding menu for the current view, auto-detection SHALL NOT fire again for that view. The sole exception is an unappliable pick (`encoding-override`): a chosen code page that cannot be applied to the bytes is not a successful override, so the view reverts to auto-detection and auto may re-fire to restore the detected render and menu hint.

#### Scenario: Manual pick sticks

- **GIVEN** a file auto-corrected to windows-1251
- **WHEN** the user then picks "KOI8-R" from the Encoding menu
- **THEN** the view re-renders with KOI8-R and auto-detection does not fire again for this view

### Requirement: Encoding menu reflects the active state

The Encoding submenu SHALL visually indicate the current encoding state as a checked entry: "Auto-detect" by default; the auto-detected code page indicated (e.g. "Auto: windows-1251") on the Auto-detect entry whenever high-confidence detection succeeded — whether the engine agreed (data shown as-is, no re-decode), the host re-decoded after a disagreement, or the file carried a genuine declared charset (`document.characterSet`) — and the user-selected entry checked after a manual pick. Checking SHALL be identical on Windows (radio items) and Linux (checkable actions).

#### Scenario: Auto-suggested state visible after a correction

- **GIVEN** a file auto-corrected to windows-1251 (engine disagreed, re-decode applied)
- **WHEN** the user opens the Encoding submenu
- **THEN** "Auto: windows-1251" is the checked (radio/check) entry and auto-detection stays active

#### Scenario: Auto-suggested state visible when data was shown as-is

- **GIVEN** a file whose bytes the engine already decoded correctly with windows-1251 (engine and detector agree, so no re-decode occurred)
- **WHEN** the user opens the Encoding submenu
- **THEN** the Auto-detect entry is checked and its label reads "Auto: windows-1251", with no re-render having happened

#### Scenario: Unrecognized detection still shows the engine charset

- **GIVEN** a file with no charset declaration whose statistical detector yields no menu-representable code page (e.g. pure ASCII reports "ascii")
- **WHEN** the user opens the Encoding submenu
- **THEN** the Auto-detect entry is checked and its label reads "Auto: <engine's actual charset>" (e.g. "Auto: windows-1252"), so the actually-used charset is always surfaced

#### Scenario: Manual pick is checked

- **GIVEN** a file where the user picked "Windows-1251"
- **WHEN** the user opens the Encoding submenu
- **THEN** "Windows-1251" is the checked entry and auto-detection is inactive for this view