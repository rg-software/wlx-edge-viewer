## Why

Issue #49 asks for `.eml` support: email messages (RFC 822 / MIME) are, like MHT, web-native documents that fit the plugin's WebView2 rendering model, and users expect to view them inline in Total Commander. Today an `.eml` falls through to the "Other" processor (PDF viewer) and is not shown.

The easiest way to handle it is to follow the existing MHT path: a thin C++ processor that loads the file via the virtual host, plus a small static JS MIME parser that renders headers and the HTML body (no new external dependencies).

## What Changes

- New `EmProcessor` C++ class (`EdgeViewer/Processors/EmProcessor.{h,cpp}`) mirroring `MhtProcessor`: `InitPath()` matches the `EML` extension set, `OpenIn()` maps `local.example` to the file's drive root and navigates to a `loader.html` template that fetches the `.eml` over `http://local.example/...`.
- New static renderer under `Resources/assets/eml/`: a vendored ready-made MIME parser (**postal-mime**, the same "ready-made component" approach as `mhtml2html`) plus a small `eml.js` + `loader.html` that
  - displays the decoded header block (Subject, From, To/Cc, Date),
  - renders the HTML body (falling back to plain text),
  - resolves `cid:` references to inline images as data URIs,
  - lists non-inline attachments,
  - falls back to raw text when the file is not a parseable message.
- Wire-up: add `EML=EML` to the `[Extensions]` section of `Resources/edgeviewer.ini` and add the `EML` section name to the `ListGetDetectString` section list in `EdgeViewer/DllMain.cpp`.
- Build: add the new files to `EdgeViewer/EdgeViewer.vcxproj` (+ `.filters`). New assets are picked up automatically because `BuildMakeSetup.bat` copies the whole `Resources\` tree.
- Sample: add an `.eml` file to `Examples/` for manual verification.

## Capabilities

### New Capabilities

- `eml`: Renders `.eml` mail messages in the lister - email headers shown as a header block, the HTML (or plain-text) body rendered as a document, inline images resolved, and attachments listed.

### Modified Capabilities

- None.

## Impact

- **C++ (EdgeViewer/)**: new `EmProcessor.{h,cpp}`, edited `EdgeViewer.vcxproj` / `.filters`, edited `DllMain.cpp` (one extra section name in the detect-string list).
- **Static assets (Resources/assets/)**: new `eml/loader.html` and `eml/eml.js`; edited `Resources/edgeviewer.ini` (one extension line). No changes to existing assets.
- **Dependencies**: no vcpkg changes and no C++ dependencies; one vendored static JS asset (`postal-mime.min.js`, MIT No Attribution, zero-dependency) following the existing `mhtml2html` precedent.
- **Platforms**: identical for 32- and 64-bit (single code path, no arch-specific behavior).
- **Docs/samples**: `Examples/` gets a sample `.eml`.
