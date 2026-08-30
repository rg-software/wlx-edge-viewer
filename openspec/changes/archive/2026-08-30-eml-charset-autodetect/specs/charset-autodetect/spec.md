## ADDED Requirements

### Requirement: Automatic charset detection on EML views

EML views SHALL run statistical detection over the **transfer-decoded payload** of the rendered body part, never over the raw MIME envelope (whose quoted-printable/base64 layer hides the real text bytes — the envelope is pure ASCII even for a Windows-1251 body). Detection SHALL run page-side in the eml loader (`Resources/assets/eml/loader.html`), which owns the pristine bytes, and SHALL select the same body part that is rendered: the HTML part when the message (or its selected `multipart/alternative`) has one, else the plain-text part — including top-level single-part messages with no `multipart/*` boundary. The loader SHALL undo the chosen part's `Content-Transfer-Encoding` (quoted-printable soft-break/hex decode, base64 whitespace-strip) before running the detector. The guess SHALL be normalized to a menu-representable tag (e.g. jschardet's `x-mac-cyrillic` → `windows-1251`).

The engine-agreement gate used for HTML (`document.characterSet`) does not exist for EML: the loader decodes the payload through the part's **declared** charset instead. Detection SHALL therefore use the same fatal-decode agreement gate as MHT — querying whether the declared charset decodes the payload without error. If the declared charset decodes cleanly (or the detected tag equals the declared tag), the load was correct: SHALL NOT re-render, SHALL only post `CMD_AUTO_ENCODING_REPORT|<tag>` so the Encoding submenu surfaces the resolved charset. If the declared charset FAILS to decode (a wrong declaration, e.g. `utf-8` on Windows-1251 bytes), or no charset is declared, SHALL post a single `CMD_AUTO_ENCODING|<detected-tag>` so the host provisionally re-renders page-side with the detected tag (same page-side machinery as a manual menu pick). Detection SHALL NOT fire on a pure-ASCII payload, when there is no text part, or on a detector confidence below the shared 0.90 threshold, and SHALL NOT fire again after a manual encoding pick for the same view.

The EML auto path SHALL be gated host-side on the same provisional/non-destructive rules as HTML and MHT: it must not mark the user as having chosen, must be transient per logical load, must leave the RFC 2047 header block and attachment footer untouched, and picking "Auto-detect" from the Encoding menu SHALL re-run EML detection (reset the page-side one-shot latch) and re-apply/report its result. Behavior SHALL be identical on Windows (WebView2) and Linux (Qt Web Engine), and on Win32 and x64.

#### Scenario: Wrongly-declared EML is auto-corrected

- **GIVEN** an EML file whose body part declares `charset="utf-8"` but whose transfer-decoded payload is actually Windows-1251 Cyrillic
- **WHEN** the loader renders and runs detection over the transfer-decoded payload
- **THEN** the declared `utf-8` fails to decode the payload, the high-confidence detector guess is `windows-1251`, and the view provisionally re-renders correctly as Windows-1251, with the header block and attachments unchanged

#### Scenario: Genuine EML is untouched

- **GIVEN** an EML file whose body part is genuinely UTF-8 (or whose declared charset decodes the payload without error)
- **WHEN** the loader renders and runs detection over the transfer-decoded payload
- **THEN** no re-render occurs and the view is unchanged; the loader only reports the resolved charset so the Encoding submenu can label the Auto-detect entry

#### Scenario: Single-part EML is detected

- **GIVEN** a single-part EML file (top-level `text/plain` body, no `multipart/*` boundary) whose charset is undeclared or wrongly declared
- **WHEN** the loader renders
- **THEN** detection runs over that part's transfer-decoded payload and applies the same auto/report decision as a multipart message

#### Scenario: Detector stays silent

- **GIVEN** an EML file whose rendered body part is pure ASCII, or which has no text part at all
- **WHEN** the loader renders
- **THEN** no detection message is posted and no re-render occurs

#### Scenario: Auto-detect re-pick re-runs detection on EML

- **GIVEN** an EML view previously auto-corrected (or manually re-encoded), showing mojibake
- **WHEN** the user picks "Auto-detect" from the Encoding submenu
- **THEN** the one-shot detection latch is reset, the loader re-runs detection over the transfer-decoded body payload, and the detected tag is re-applied (or the hint re-reported)