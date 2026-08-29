## Context

See proposal.md — Why. The EML loader (`Resources/assets/eml/loader.html` + `eml.js`) currently parses the pre-fetched bytes with `PostalMime.parse` and renders via `emlRenderer.renderEmail(email)`: an RFC 2047-decoded header block, an HTML or text body, and an attachment footer. The body charset comes solely from the part's declared `Content-Type` (PostalMime decodes with it) or falls back to UTF-8 — no detection. Meanwhile `mhtml/loader.html` already implements loader-owned charset auto-detection end-to-end (jschardet over transfer-decoded payload bytes, fatal-decode agreement gate, `CMD_AUTO_ENCODING[_REPORT]`, `__evEncodingApply` page-side re-decode, `CMD_ENCODING_APPLY_FAILED` on an unappliable pick, one-shot latch + re-pick re-run). The host/backend plumbing (menu registration, `CMD_AUTO_ENCODING`/`CMD_AUTO_ENCODING_REPORT` dispatch, loader-dispatch `ApplyCharsetOverride`, provisional `autoEncodingApplied` state) is generic and gated on `processor->supportsEncodingOverride()` (WebViewFactory.cpp:449; Qt `encodingOverrideSupported`), which `BaseFileProcessor::OpenIn` already wires via `SetEncodingOverrideSupported` (BaseFileProcessor.cpp:66). `MhtProcessor` overrides it to `true`; `EmProcessor` inherits `false`. The EML loader is a pre-fetch loader, so the pristine bytes (`rawBytes`) are already in the page.

## Goals / Non-Goals

**Goals:**
- Give EML the same loader-owned auto-detection, manual Encoding menu, and provisional re-decode behavior as MHT, with the whole feature treated as a page-side/asset change plus a one-line processor override.
- Reuse the vendored `Resources/assets/charset/` jschardet detector and the existing generic host dispatch untouched.

**Non-Goals:**
- No changes to `autodetect.js`/HTML path, no new ini keys, no host-side transcode for EML (loader-owned, like MHT), no dependency or vcpkg changes, no whole-file charset detection of `message/rfc822` attachments or nested quoted messages.
- Correcting a wrong declared charset in the **latin-1 family** (any declared tag whose `TextDecoder` decodes the payload without error under non-fatal semantics) stays out of scope — it is a parity limitation MHT already has, because those decoders are effectively total.

## Decisions

### D1: Loader-owned detection in `eml/loader.html`, mirroring the MHT block

Detection SHALL run in the EML loader over the pristine transfer-decoded payload bytes, exactly as MHT does (`runMhtAutoDetect`/`detectAndPost` in `mhtml/loader.html`): detect via `window.jschardet.detect(bin)` (dynamically `loadScript`'d from `http://assets.example/charset/jschardet.min.js`), skip on confidence < 0.90 — `normalizeMhtName` (the same `TAG_NORMALIZE` table) — skip on pure-ASCII payload, probe the declared charset with a fatal `TextDecoder`, then `post('CMD_AUTO_ENCODING|' + detected)` on disagreement or `post('CMD_AUTO_ENCODING_REPORT|' + tag)` on agreement, guarded by a one-shot `window.__evEmlAutoDetectDone` latch reset by re-picks.

- *Why not host-side via `autodetect.js`?* The HTML detector only ever sees the pre-fetched `base64` envelope literal — for EML that is the transfer-encoded MIME source, whose body bytes are quoted-printable/base64, so the detector would sniff the encoded junk. The real text bytes only exist in the page.
- *Why not a C++-side transcode?* MHT already established the loader-owned precedent; re-render of the body is inherently page-side (DOM rebuild), and reusing the generic loader-dispatch `ApplyCharsetOverride` path means zero backend edits.

### D2: Body-part selection via a recursive multi-level boundary walk (differs from MHT's shallow scan)

EML legitimately nests `multipart/mixed > multipart/related > multipart/alternative`, so `mhtml/loader.html`'s `extractTextPart` (top-level boundary sections only) would find no text part on such a message and stay silent. The EML loader SHALL implement its own `extractBodyPart(rawBytes)` that descends recursively into `multipart/*` and `message/rfc822` sections and selects, per level, the same part `renderEmail` renders: the first `text/html` part, else the largest `text/*` part. It MUST also handle **single-part** messages (no `boundary=`, no `multipart/*`): the entire source after the header/body blank line is the one text part. It returns `{ body, enc, declared, isHtml }` and `null` when there is no text part (e.g. an `image/*`-only or unparseable message).

- *Why not reuse PostalMime for byte access?* `email.html`/`email.text` are already charset-decoded strings; the raw transfer-decoded payload (needed for detection and re-decode) is not exposed for the rendered body. The walker reuses the MHT regex idioms (`content-type`/`charset`/`content-transfer-encoding` extraction, boundary split, `transferDecode` for quoted-printable soft-breaks / base64 whitespace).

### D3: Body-only re-render through a new `emlRenderer` entry point

Re-decode (manual pick and provisional auto) SHALL produce a **body-only** DOM update so the RFC 2047 header block and attachment footer are never touched (spec: headers/attachments unchanged). `loader.html` stores the selected part's payload bytes + `isHtml` flag + declared charset at load time, and exposes `window.__evEncodingApply(tag)` (defined at top level, like MHT's) that:
- takes the pristine payload, `new TextDecoder(tag, { fatal: true })`, and on failure keeps the previous render, posts `CMD_ENCODING_APPLY_FAILED`, resets the latch and re-runs detection (the `OnEncodingApplyFailed` flow);
- on success rebuilds only `#content .body`: if `isHtml`, sets `innerHTML` to the re-decoded text passed through `inlineCidImages`; if text, renders `<div class="text-body">` with the escaped text;
- on `tag === null` (host's "Auto-detect" re-pick) resets the latch and re-runs detection over the fresh render.

`eml.js` gains a small export (e.g. `applyBody(html, isHtml, attachments)`) so `renderEmail` and `__evEncodingApply` share one body-render path. Initial detection runs right after the first successful `renderEmail`, matching MHT's post-render timing; if `renderRawText` was used (unparseable message) no detection runs.

- *Why re-decode from the stored payload rather than from `email.html`?* The string is already charset-mangled; only the pristine bytes can be re-decoded. PostalMime stays the parser; the walker supplies the raw payload.
- *Why not re-parse with PostalMime on each pick?* `__evEncodingApply` must be callable synchronously by the host's `ExecuteScript`; re-parsing asynchronously per pick complicates the apply/failed contract.

### D4: C++ gate is a one-line override in `EmProcessor.h`

`EdgeViewer/Processors/EmProcessor.h` SHALL override `bool supportsEncodingOverride() const override { return true; }`, mirroring `MhtProcessor.h`. `BaseFileProcessor::OpenIn` already calls `SetEncodingOverrideSupported(supportsEncodingOverride())`, which drives the WebView2/Qt native Encoding-submenu registration and enables `CMD_AUTO_ENCODING`/`CMD_AUTO_ENCODING_REPORT`/`CMD_ENCODING_APPLY_FAILED` handling in both backends unchanged. No other C++ edits; no vcpkg.json change (jschardet already vendored under `Resources/assets/charset/`).

- *Why the flag instead of a new processor interface method?* `supportsEncodingOverride()` is the exact existing gate the menu and auto behaviors key off; reusing it is the minimal, spec-aligned change.

### D5: Test fixtures

Add to `Examples/`: (a) a Windows-1251 EML that **undeclares** its charset (single-part `text/plain`, base64, so it currently falls back to UTF-8 mojibake) and (b) a Windows-1251 EML that **wrongly declares** `charset="utf-8"` (multipart, base64 HTML body) — mirroring `encoding-windows1251.html` / `encoding-wrong-charset.mht`.

## Risks / Trade-offs

- [Walker and PostalMime disagree on which part is "the body"] → Follow the same rule as `renderEmail` (html preferred, else text) and detect over exactly the part the loader captured; if the walker finds no text part, detection stays silent (no crash).
- [Latin-1-family declared charsets decode total → wrong declarations in that family never auto-correct] → Parity limitation with MHT (`charset-autodetect`); documented in proposal/spec, not silently "fixed" differently for EML.
- [Nested `message/rfc822` increases walker complexity / risk of infinite recursion] → Bound recursion depth (e.g. 10 levels) and only descend into `multipart/*`/`message/rfc822`; any malformed section is treated as opaque bytes.
- [`__evEncodingApply` running before the first render completes] → Guard on a `loaded` flag / stored payload exactly as MHT guards on `rawBytes`; no-op when not ready.
- [jschardet load failure (offline asset)] → `.catch(() => {})` like MHT; loader renders normally, `Auto:` hint simply not set.