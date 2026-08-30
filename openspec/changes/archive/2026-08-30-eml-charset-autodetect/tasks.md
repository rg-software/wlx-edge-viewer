## 1. Loader asset changes (Resources/assets/eml/) — C++ untouched here

- [x] 1.1 Add the `TAG_NORMALIZE`/`normalizeMhtName` table and a `transferDecode(body, enc)` helper (quoted-printable soft-break/hex, base64 whitespace-strip) to `eml/loader.html`, mirroring `mhtml/loader.html`
- [x] 1.2 Implement `extractBodyPart(rawBytes)` in `eml/loader.html`: recursive descent into `multipart/*` and `message/rfc822` (depth-capped), single-part `text/*` support (top-level body after header/body blank line), selecting the first `text/html` else largest `text/*`, returning `{ body, enc, declared, isHtml }` or `null`
- [x] 1.3 Add `applyBody(html, isHtml, attachments)` export to `eml.js` (shared by `renderEmail` and the encoding executor): body-only innerHTML rebuild with `inlineCidImages` for HTML, `escapeHtml` in `.text-body` for text
- [x] 1.4 In `loader.html`, after the first successful `renderEmail`, capture the selected part payload + `isHtml` + declared charset and run EML auto-detect once (jschardet load, confidence < 0.90 / pure-ASCII / no-text-part → silent; fatal-decode probe of declared charset; `CMD_AUTO_ENCODING` on disagreement else `CMD_AUTO_ENCODING_REPORT`), guarded by a one-shot `window.__evEmlAutoDetectDone` latch; do not run on the `renderRawText` fallback path
- [x] 1.5 Add top-level `window.__evEncodingApply(tag)` to `loader.html`: decode stored payload with `new TextDecoder(tag, { fatal: true })`; on success body-only re-render via `applyBody`; on failure keep previous render, post `CMD_ENCODING_APPLY_FAILED`, reset latch, re-run detection; on `tag === null` reset latch and re-run detection

## 2. C++ gate

- [x] 2.1 Override `bool supportsEncodingOverride() const override { return true; }` in `EdgeViewer/Processors/EmProcessor.h` (mirrors `MhtProcessor.h`); no other C++ edits

## 3. Fixtures

- [x] 3.1 Add `Examples/encoding-windows1251.eml`: single-part `text/plain`, base64, Windows-1251 bytes with no charset declared (currently renders as UTF-8 mojibake)
- [x] 3.2 Add `Examples/encoding-wrong-charset.eml`: multipart `text/html` body, base64, Windows-1251 bytes wrongly declared `charset="utf-8"` (mirrors `encoding-wrong-charset.mht`)

## 4. Verify

- [x] 4.1 Build Release for both Win32 and x64 (`BuildMakeSetup.bat` or `msbuild EdgeViewer.vcxproj /p:Configuration=Release /p:Platform=x86|x64` from the MSVS Developer Command Prompt) — clean compile, no warnings introduced
- [x] 4.2 Load `Examples/encoding-windows1251.eml` in Total Commander: renders correct Cyrillic without user action; Encoding submenu shows auto entry
- [x] 4.3 Load `Examples/encoding-wrong-charset.eml`: wrong `utf-8` declaration auto-corrected; `Examples/multipart-sample.eml` still renders unchanged (genuine UTF-8, zero flicker)
- [x] 4.4 Manual menu checks on an EML view: pick "Windows-1251"/"Auto-detect" re-renders body only (headers/attachments intact); pick an unappliable code page reverts via `CMD_ENCODING_APPLY_FAILED` to the auto state
- [x] 4.5 Confirm same steps 4.2–4.4 on Linux (Qt Web Engine) build if available — manual check, and verified via a dlopen driver against the installed plugin: `encoding-windows1251.eml` auto-corrects to Windows-1251 on Linux