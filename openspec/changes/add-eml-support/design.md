## Context

See proposal.md for motivation. Relevant current state:

- `MhtProcessor` already demonstrates the exact pattern we need: a thin C++ processor that maps `local.example` to the file's drive root and loads a `loader.html` template that `fetch()`es the file from `http://local.example/<path>` and renders it in the browser.
- `mhtml2html.min.js` was considered for reuse but is not usable for EML: its parser requires a `Content-Location` header on the first `text/html` part to identify the main document ("Index not found" otherwise). EML files carry RFC 822 headers (From/To/Subject/Date) and no `Content-Location`.
- Renderer behavior belongs in static JS/CSS under `Resources/assets/`; C++ processors are deliberately thin. `Resources\` is copied wholesale into release builds (`BuildMakeSetup.bat`), so new asset files need no build wiring.
- No test suite; verification is a Release build for both platforms plus a manual load in Total Commander.

## Goals / Non-Goals

**Goals:**
- Handle the overwhelming majority of real-world EML files correctly: header display, HTML/plain body, `multipart/alternative`, `multipart/related` with `cid:` inline images, `multipart/mixed` with an attachment list, base64 + quoted-printable, RFC 2047 header decoding, per-part charset decoding.
- Keep the C++ footprint tiny (one processor mirroring `MhtProcessor`), keep all parsing/render logic in static JS.
- Reuse a proven, ready-made MIME parser (postal-mime) rather than writing one, mirroring the `mhtml2html` decision; add no vcpkg.json or C++ dependencies.

**Non-Goals:**
- Full MIME/RFC 822 compliance (exotic encodings, RFC 2231 parameter continuations, `multipart/signed`, S/MIME, TNEF).
- Saving or opening attachments from the lister - attachments are listed only.
- Editor-like features (reply/forward/compose).

## Decisions

### 1. New `EmProcessor` in C++ (EdgeViewer/Processors/) mirroring `MhtProcessor`

`InitPath()` matches against the `EML` extension set from `edgeviewer.ini`; `OpenIn()` calls `mapDomains(webView, mPath.root_path())` and fills `Resources/assets/eml/loader.html` via `replacePlaceholders`, then `NavigateToString`.

- Alternative considered: extend `HtmlProcessor` or `MhtProcessor` to also accept `.eml`. Rejected - each processor owns one file type, and EML needs dedicated rendering (header block, MIME structure), not raw HTML.
- Alternative considered: parse MIME in C++ (e.g. a MIME parsing library). Rejected - the C++ surface grows massively (MIME tree, charsets, base64/QP) and charset handling is far easier with the browser's `TextDecoder`. "Easiest way" = keep C++ trivial, put logic in JS.

### 2. Vendor a ready-made browser MIME parser (postal-mime) + small renderer (Resources/assets/eml/)

Same "ready-made component" approach as `mhtml2html`: vendor the zero-dependency **postal-mime** library (MIT No Attribution, from the nodemailer/EmailEngine author) as `Resources/assets/eml/postal-mime.min.js`. `PostalMime.parse(arrayBuffer)` returns exactly the shape the renderer needs: `subject`, `from`/`to`/`cc` (already RFC 2047-decoded `Address` objects), `date`, `html`, `text`, and `attachments` (with `contentId` and `related` flags for inline images). Our own code is then a thin `eml.js` renderer (~80-100 lines): header block, pick `html` over `text`, rewrite `cid:` image URLs to data URIs, attachment list, raw-text fallback on parse failure.

postal-mime is ESM/CommonJS and the project has no JS toolchain, so the vendored file is produced once at implementation time (one-shot `npx esbuild` IIFE bundle, pinned version noted) and committed like `mhtml2html.min.js`. Loading it as a classic `<script src="http://assets.example/eml/postal-mime.min.js">` avoids the ES-module MIME-type uncertainty of serving `.mjs` from the virtual host.

- Alternative considered: hand-rolled ~200-line MIME parser. Rejected - re-implements RFC 2047, charset, boundary and base64/QP decoding that postal-mime already handles battle-tested; the same reasoning that led to `mhtml2html` over a custom MHT parser.
- Alternative considered: vendor the old `mimeparser` npm package. Rejected - deprecated (last published ~12 years ago), streaming/callback API needs more glue, no browser bundle.
- Alternative considered: normalize EML → MHT and reuse `mhtml2html.js`. Rejected - it requires `Content-Location` headers on parts and produces no mail header rendering.

### 3. Fetch as `arrayBuffer`, pass straight to `PostalMime.parse()`

`loader.html` fetches `http://local.example/__EML_FILENAME__` as an `arrayBuffer` and hands it to `PostalMime.parse()`. The library accepts ArrayBuffer/Uint8Array input, does per-part charset decoding internally, and enforces built-in security limits (MIME nesting depth, total header size).

- Alternative considered: decoding bytes ourselves (latin1-lossless round-trip + per-part `TextDecoder`). Rejected - postal-mime does this internally; reimplementing adds code for no gain.

### 4. `cid:` images inlined as data URIs; no external loading

Attachments flagged `related: true` with a `contentId` are rendered as `data:` URIs and substituted for `cid:` references in the HTML body. The document is loaded via `NavigateToString` (no real origin), so external URLs cannot resolve - consistent with the mhtml viewer's offline stance and effectively safer (body `<script>` injected via `innerHTML` does not execute).

### 5. Layout: header block + body, light/dark via `prefers-color-scheme`

`loader.html` renders a styled header block (Subject, From, To, Cc, Date) above the message body and an attachment list at the bottom. Dark styling uses a `@media (prefers-color-scheme: dark)` block, so no extra C++/ini wiring is needed for dark mode.

### 6. Wiring (small, both files known from config.yaml)

- `Resources/edgeviewer.ini`: add `EML=EML` to `[Extensions]`.
- `EdgeViewer/DllMain.cpp`: add `"EML"` to the `ListGetDetectString` section list.
- `EdgeViewer/EdgeViewer.vcxproj` (+ `.filters`): register `EmProcessor.cpp/.h`.

## Risks / Trade-offs

- The MIME parsing correctness rests on a vendored third-party lib → mitigation: postal-mime is zero-dependency, actively maintained, RFC-compliant, and battle-tested (used by EmailEngine); the pinned version is recorded at bundle time; a parse-failure fallback still shows the raw file text.
- Bundling the lib needs npm/npx at implementation time → mitigation: one-shot step during development only; the resulting `postal-mime.min.js` is committed, so the build itself needs nothing.
- Attachments are listed, not savable, which some users may find limiting → accepted as a lister-scope trade-off; noted in Non-Goals.
- Dark mode follows the OS theme rather than TC's `lcp_darkmode` flag → subtle styling keeps any mismatch acceptable.
- Body HTML is injected via `innerHTML`; external URLs won't load and `<script>` does not run, but `on*` attributes would still fire on user interaction → acceptable for a local file lister, same trust model as the existing mhtml/markdown viewers.

## Migration Plan

Additive change - no migration. Rollback is a revert of the new files and the two wiring lines. No vcpkg.json or C++ dependency changes are involved; the only new dependency is the committed `postal-mime.min.js` asset.

## Open Questions

None blocking. Whether attachments should eventually be openable in an external handler is a possible future feature, not a decision this change depends on.
