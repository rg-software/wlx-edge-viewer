# Proposal: HTML charset override (host-side re-decode)

## Why

Future-work #1's re-introduction trigger is now satisfied: reproducible user-reported fixtures exist where the engine mis-renders legacy-encoded HTML (`Examples/encoding-windows1251.html`, `encoding-koi8-r.html` — no BOM, no `<meta charset>`; plus `encoding-windows1251-wrongmeta.html` with a *wrong* declared charset). The engine sniffs UTF-8 and shows mojibake.

Additionally, the shipped `manual-encoding-selection` mechanism turned out to be **non-functional for HTML on Linux**: every in-page re-decode strategy (fetch + `document.write`, DOMParser + head/body `innerHTML` swap, body-only swap) reliably blanked the render and killed the host context menu on Qt Web Engine. The MHT path (loader-internal re-decode into a stable shell) is unaffected and stays.

## What Changes

- **HTML re-decode moves host-side** (C++), replacing the page-side `__evEncodingApply` executor for HTML views:
  - The processor hands the raw source bytes to the backend once per load (`IWebView::SetRawFileBytes`, already landed in c68637b).
  - Selecting an encoding makes the backend **transcode the pristine cached bytes into Unicode with the chosen code page** (Windows `MultiByteToWideChar`; Linux ICU-backed `QStringDecoder`) and perform a **fresh full render** of the decoded text through the proven embedded-string load path. (A `<meta charset>` splice was originally planned but is empirically inert: `NavigateToString`/`setHtml` re-encode their argument to UTF-8, so the engine always decodes a UTF-8 stream and a spliced declaration cannot force a code page.)
  - `<base href="...">` pointing at the file's directory is prepended to every embedded render so relative subresources (images/CSS of archived pages) keep resolving through the `local.example` host mapping on both platforms.
  - Auto-detect re-renders the pristine bytes without a forced decode (fresh engine sniffing over the original bytes). No reload, no JS involved.
- **HTML loading becomes embedded-string based on both platforms** (already true on Linux since c68637b): bytes are read once by `HtmlProcessor::OpenIn` and rendered via `NavigateToString`; top-level navigation through `ev://local.example/...` is abandoned (Qt Web Engine renderer cannot process scheme-served documents). Windows gains the same path, fixing its own latent issues (2 MB `NavigateToString` cap workaround, about:blank relative-ref breakage) via the `<base>` splice.
- **Removed:** the Linux HTML encoding-bootstrap userscript (dead code — every variant of it failed); the Windows fetch+document.write bootstrap for HTML pages.
- **Unchanged:** MHT loader flow; menu contents (`EncodingList.h`); transient scope (no ini key, no persistence); `[HTML] DetectEncoding` stays removed — this is a *manual* override only.

## Capabilities

- **Modified Capabilities:**
  - `encoding-override` — HTML re-decode mechanics change from in-page executor to host-side transcode + fresh render; failure semantics and parity wording updated.
  - `html` — rendering requirement changes from virtual-host navigation to embedded-string render with `<base>` prepend; sniffing remains the default charset decision.
  - `linux-runtime` — the "HTML charset override unavailable" requirement and its known-limitation note are superseded (manual override ships; automatic detection still does not).

## Impact

- **C++:**
  - new shared `EdgeViewer/CharsetOverride.{h,cpp}` — byte helpers (Latin-1 expansion, `<base>` prepend) + Windows `MultiByteToWideChar` transcoder, unit-testable
  - `IWebView.h` — `ApplyCharsetOverride(tag)` (empty = auto) added; both backends implement using their `SetRawFileBytes` cache
  - `WebView2Backend.cpp` — override `SetRawFileBytes` to cache; implement `ApplyCharsetOverride` (transcode → `NavigateToString`); drop the >2 MB temp-file NavigateToString workaround reliance for HTML (bytes are pre-fetched anyway)
  - `WebViewFactory.cpp` / `QtWebEngineBackend.cpp` — Encoding-menu pick handlers call `ApplyCharsetOverride` instead of dispatching `__evEncodingApply`; MHT picks route page-side to the loader executor via `SetEncodingOverrideHtml`; delete both HTML bootstraps
  - `HtmlProcessor.cpp` — unchanged from c68637b (already passes bytes + renders embedded)
- **Specs:** deltas under `specs/` here; `Readme.md` limitation row for future-work #1 updated.
- **No dependency changes**; no ini keys; works identically on Win32/x64/Windows-Linux backends since it relies only on each OS's standard code-page converter.

Tracking: resolves future-work #1 (manual form; automatic detection remains out of scope). Completes the Linux half of #66 that `manual-encoding-selection` left broken.
