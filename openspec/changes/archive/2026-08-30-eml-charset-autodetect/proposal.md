# Proposal: Automatic charset detection for EML with provisional auto re-decode

## Why

EML messages render through the `Resources/assets/eml/` loader, which decodes the body with the charset declared in the part's `Content-Type` (or falls back to UTF-8). Legacy messages that declare the wrong charset, or omit it (so the loader falls back to UTF-8), render as mojibake with no recourse short of editing the file. HTML and MHT already gained statistical charset auto-detection (`charset-autodetect`, `encoding-override`); EML is the remaining text loader without it. Issue #70 asks whether the same functionality can be recreated for EML — it can, using the MHT loader-owned pattern.

## What Changes

- **Loader-owned statistical detection for EML, mirroring MHT.** `Resources/assets/eml/loader.html` runs the already-vendored jschardet detector over the **transfer-decoded body payload** of the primary text part (base64/quoted-printable first — the raw MIME envelope hides the real bytes), never the DOM or the raw envelope.
- **Single-part EML support.** Unlike MHT's extractor (multipart-only), EML detection selects the primary text part from `PostalMime.parse` output, which also yields top-level `text/plain`/`text/html` bodies with no `multipart/*` boundary.
- **Same agreement gate and message flow as MHT.** The part's **declared** charset is authoritative only if it decodes the payload without error (fatal `TextDecoder` probe). On a high-confidence detector guess that disagrees with a failing/absent declaration, the loader posts a single `CMD_AUTO_ENCODING|<tag>`; the host provisionally re-renders page-side via the existing `__evEncodingApply` machinery (`SetEncodingOverrideHtml(false)` loader-dispatch path). On agreement/clean declared decode, only `CMD_AUTO_ENCODING_REPORT|<tag>` is posted so the Encoding submenu labels the Auto-detect entry. Pure-ASCII payload, no text part, or confidence below the shared 0.90 threshold → no message.
- **Provisional, non-destructive, same as MHT.** The auto-applied re-decode is marked auto (never `userPicked`), the header block and attachment list stay untouched, a manual encoding pick always wins and disarms auto for the view, "Auto-detect" re-pick re-runs EML detection (resets the page-side one-shot latch), and an unappliable manual pick reverts to auto via the existing `CMD_ENCODING_APPLY_FAILED` flow.
- **One-line C++ gate.** `EdgeViewer/Processors/EmProcessor.h` overrides `supportsEncodingOverride()` → `true` (currently inherits `false`). This alone wires the existing generic backend: Encoding submenu registration (WebViewFactory.cpp), auto-detect script availability, and `CMD_AUTO_ENCODING[_REPORT]` dispatch. No new dependencies, no vcpkg change.
- **Menu parity.** The right-click Encoding submenu now also appears on EML views, with the same curated list, "Auto: <tag>" hinting, and checkable state as HTML/MHT.

## Capabilities

### New Capabilities
- *(none — the `charset-autodetect` and `encoding-override` capabilities already exist)*

### Modified Capabilities
- `charset-autodetect`: add an EML-view requirement — statistical detection over the transfer-decoded text-part payload with the same fatal-decode agreement gate, `CMD_AUTO_ENCODING`/`CMD_AUTO_ENCODING_REPORT` flow, and provisional re-decode semantics as the existing MHT requirement.
- `encoding-override`: extend the Encoding submenu to EML views and add the EML loader-owned re-decode-with-forced-charset requirement (mirroring the MHT one), replacing EML's current exclusion from the menu.

## Impact

- **JS assets (edit only, no new files):** `Resources/assets/eml/loader.html` — select primary text part (incl. single-part), transfer-decode (base64/QP), jschardet detection over the pristine payload, fatal-decode agreement gate, `__evEncodingApply(tag)` re-render of the body only (HTML → rebuilt body + cid rewrite; text → escaped text), one-shot auto latch, re-pick re-run. `Resources/assets/eml/eml.js` may need a small hook to re-render the body without re-parsing/re-rendering the whole message. Reuses vendored `Resources/assets/charset/` (jschardet + glue) — nothing shipped there changes.
- **C++ (minimal):** `EdgeViewer/Processors/EmProcessor.h` — `virtual bool supportsEncodingOverride() const override { return true; }` (mirrors `MhtProcessor.h`). All host/backend behavior (`ApplyCharsetOverride` loader-dispatch branch, `OnEncodingApplyFailed`, menu registration gated on `supportsEncodingOverride()`) already exists and is generic.
- **No dependency changes:** vcpkg.json untouched; jschardet already vendored.
- **Test fixtures (add):** an undeclared/Windows-1251 `Examples/*.eml` sample and a wrongly-declared (e.g. `charset=utf-8` on Windows-1251 bytes) sample for manual verification, alongside the existing `Examples/multipart-sample.eml`.
- **Both platforms/bits:** behavior identical on Windows WebView2 (Win32 + x64) and Linux Qt Web Engine; verify by building Release for both Windows platforms and loading samples in TC.
- **Known parity limitation:** latin-1-family declared charsets (windows-1252, iso-8859-1, …) never fail the fatal-decode probe, so a wrong declaration in that family is not auto-corrected — identical to the existing MHT limitation.