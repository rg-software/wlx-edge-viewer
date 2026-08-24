# Proposal: Manual Encoding Selection

## Why

Old archived HTML and MHT files frequently declare no charset (or a wrong one). The web engine then falls back to locale-default sniffing, and legacy-codepage content (Windows-1251, KOI8-R, GB2312, Shift-JIS, ...) renders as mojibake with no way for the user to fix it. GitHub issue #66 requests a right-click menu to re-display the file with an explicitly chosen code page.

This is deliberately **not** the removed `[HTML] DetectEncoding` machinery (future-work #1): there is no ini key, no automatic detection change, and no persistence. The override is transient — chosen per view from a context menu, valid until the next navigation/reload naturally resets to auto-detection.

## What Changes

- Extend the engine's **built-in right-click menu** with an "Encoding" submenu listing common code pages plus an "Auto-detect" reset entry (native menu extension on both backends; no DOM overlay replacing the standard menu).
- Scope: **HTML** and **MHT** processors only.
  - MHT: the loader re-decodes the inlined bytes with the chosen `TextDecoder` label before handing the source to `mhtml2html.convert()`.
  - HTML: an injected document-created executor (same mechanism as dark-mode CSS injection) re-decodes via fetch-from-own-origin + `document.write`; Auto-detect reloads so engine sniffing applies again.
- Text loaders (Markdown, RST, AsciiDoc, EML) are intentionally excluded: Markdown/RST/AsciiDoc are UTF-8 by spec/convention; EML charsets are honored per-part by postal-mime. The shared encoding list keeps later wiring trivial if a real case appears.
- No persistence across reloads, ListLoadNextW, or sessions; no ini keys; no new JS→host commands (the host reaches the page through `ExecuteScript`/`runJavaScript` on a `__evEncodingApply` executor).

## Capabilities

- **New Capabilities:**
  - `encoding-override` — user-selected transient charset override for HTML and MHT views: menu behavior, re-decode semantics, reset-to-auto, scoping rules.
- **Modified Capabilities:**
  - none (the existing `html`/`mhtml` rendering requirements are unchanged; auto-detection remains the default path)

## Impact

- **C++ (both backends):**
  - new `EdgeViewer/EncodingList.h` — single source of truth for the entry list, shared by both platforms
  - `WebViewFactory.cpp` — `AddEncodingBootstrapScript` (page-side executor for HTML pages) + `AddNativeEncodingMenu` (extends the default WebView2 context menu via `ContextMenuRequested` + `CreateContextMenuItem`); registered only when the processor supports re-decoding
  - `QtWebEngineBackend.cpp` — same executor bootstrap + native extension of Qt Web Engine's standard context menu (`createStandardContextMenu` + "Encoding" actions), gated at popup time by page URL
  - `ProcessorInterface.h`, `HtmlProcessor.h`, `MhtProcessor.h` — `supportsEncodingOverride()` flag
  - `Resources/assets/mhtml/loader.html` — forced-charset decode branch + `__evEncodingApply` exposure
- **Removed:** the intermediate DOM-menu asset `Resources/assets/encoding-menu.js` (superseded by the native menus during implementation)
- **No dependency changes**: no vcpkg.json edits; pure Chromium-standard `TextDecoder` labels.
- **Platforms:** identical entries and semantics on Windows (WebView2) and Linux (Qt Web Engine); the only divergence is where the gate lives (processor pointer at creation vs page URL at popup time) — behavior is equivalent.
- Tracking: closes #66 once shipped; future-work #1 (full DetectEncoding revival) stays open and unaffected.
