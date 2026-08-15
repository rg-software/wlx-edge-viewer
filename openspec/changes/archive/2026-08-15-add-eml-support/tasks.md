## 1. C++ Processor

- [x] 1.1 Create `EdgeViewer/Processors/EmProcessor.h` - `EmProcessor : ProcessorInterface` mirroring `MhtProcessor.h` (fields: `fs::path mPath`)
- [x] 1.2 Create `EdgeViewer/Processors/EmProcessor.cpp` - `InitPath()` matches the `EML` extension set via `isType`; `OpenIn()` calls `mapDomains(webView, mPath.root_path())`, fills `eml/loader.html` with the `__EML_FILENAME__` placeholder (relative path via `urlPathW`), and `NavigateToString`s it. Self-register via a namespace-scope `EmProcessor` instance.
- [x] 1.3 Register `EmProcessor.cpp` / `EmProcessor.h` in `EdgeViewer/EdgeViewer.vcxproj` and `EdgeViewer/EdgeViewer.vcxproj.filters`

## 2. Static Renderer Assets

- [x] 2.1 Vendor the ready-made parser: one-shot `npx esbuild` IIFE bundle of `postal-mime` (ESM src) into `Resources/assets/eml/postal-mime.min.js` exposing a global; record the pinned version; commit it like `mhtml2html.min.js`
- [x] 2.2 Create `Resources/assets/eml/eml.js` - our renderer on top of `PostalMime.parse()`: decoded header block (subject, from, to/cc, date), pick `html` over `text`, rewrite `cid:` URLs to data URIs from `related` attachments, list non-related attachments with name/size, `prefers-color-scheme` dark styling
- [x] 2.3 Create `Resources/assets/eml/loader.html` - loads `postal-mime.min.js` and `eml.js` from `http://assets.example/eml/`, fetches `http://local.example/__EML_FILENAME__` as `arrayBuffer`, calls `PostalMime.parse()`, then invokes the renderer
- [x] 2.4 Add graceful fallback to raw file text when the file is not parseable as MIME (spec: malformed message handling)

## 3. Wiring and Samples

- [x] 3.1 Add `EML=EML` to the `[Extensions]` section of `Resources/edgeviewer.ini`
- [x] 3.2 Add `"EML"` to the `ListGetDetectString` section list in `EdgeViewer/DllMain.cpp` (it must match the new ini section name)
- [x] 3.3 Add a sample `.eml` to `Examples/` (prefer a `multipart/alternative` message with an inline image and an attachment, per the spec scenarios)

## 4. Verify

- [x] 4.1 Build Release for both Win32 and x64 (via `BuildMakeSetup.bat` or `vcvarsall` + `msbuild`), confirming no build errors and that `assets/eml/` lands in the output
- [x] 4.2 Load the plugin in Total Commander and verify with the sample `.eml`: headers shown, HTML body rendered, inline `cid:` image displayed, attachment listed; also verify a plain-text and a malformed `.eml` fall back to text; confirm 32-bit and 64-bit builds behave identically

## 5. Chrome Theming Follow-up (pre-archive)

EML's header/attachment chrome should follow TC's `lcp_darkmode` like every other viewer (not the OS theme), keeping email **body** styling as the sender authored it (the MHT precedent: saved archive HTML carries its own CSS).

- [x] 5.1 Split inline `loader.html` styling into `Resources/assets/eml/style.css` (light) and `style-dark.css` (dark); drop the `@media (prefers-color-scheme)` block
- [x] 5.2 Add `__CSS_NAME__` `<link>` in `loader.html`; `EmProcessor.cpp` reads `[EML]` `CSS`/`CSSDark` via `gs_IsDarkMode` and injects the placeholder (mirroring `MdProcessor`)
- [x] 5.3 Add `[EML]` section to `Resources/edgeviewer.ini` (`CSS=style.css`, `CSSDark=style-dark.css`)
- [x] 5.4 Update `Readme.md` to list EML support with the postal-mime link
- [x] 5.5 Rebuild Release for both platforms and confirm the new assets land in the output
