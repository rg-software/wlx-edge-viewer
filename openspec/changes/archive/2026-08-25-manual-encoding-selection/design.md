# Design: Manual Encoding Selection

## Context

See proposal.md for motivation. Current mechanics that shape the approach:

- **MHT**: `Resources/assets/mhtml/loader.html` holds the file bytes as a base64 literal and hands a JS string to `mhtml2html.convert()`. The library decides charsets internally: it first tries the UTF-8 `escape`/`decodeURIComponent` trick, and on failure falls back to charset from part/document headers, then `<meta>`, decoding via `new TextDecoder(name, {fatal:true})`. Wrong/absent declarations produce mojibake.
- **HTML**: no loader wrapper; `HtmlProcessor::OpenIn` navigates directly to `local.example/<file>` (Linux: `ev://local.example/<file>`), and both backends register a document-created script that injects `[HTML] CSS` on pages whose URL starts with `local.example` — Windows `AddApplyStyleScript` (WebView/WebViewFactory.cpp:78), Linux mirror (WebView/QtWebEngineBackend.cpp:466).
- The removed `[HTML] DetectEncoding` machinery (interceptor + `gs_Htmls`) is intentionally **not** revived (future-work #1 stays closed); this change adds no host-side request interception and no persistence.

## Goals / Non-Goals

**Goals:**

- Right-click "Encoding" submenu usable on HTML and MHT views, identical on both backends.
- Re-decode of the *already loaded* file entirely in-page; zero new JS→host commands, zero C++ state.
- Clean reset semantics: every navigation/reload returns to auto-detection.

**Non-Goals:**

- Automatic detection improvements (that is future-work #1 territory).
- Override support for Markdown/RST/AsciiDoc/EML (see proposal).
- Persistence across reloads/windows/sessions, ini keys, or remembered per-file choices.

## Decisions

### D1: Extend the engines' native context menus (revised during apply)

**Original decision (superseded):** an in-page DOM menu replacing the standard context menu. First user testing rejected it twice: a two-level "Encoding → entry" submenu vanished when the pointer crossed the gap to the flyout, and the flattened top-level list still *hid the standard browser menu entirely*.

**Revised decision:** extend each engine's built-in right-click menu with an "Encoding" submenu — standard entries stay, our entries ride along, and because the menu is host-owned it also survives any in-page `document.write` rewrite (fixing the "menu disappears after first re-encode" defect of the DOM approach).

- **Windows:** `ICoreWebView2_11::add_ContextMenuRequested` + `ICoreWebView2Environment9::CreateContextMenuItem` (`SUBMENU` kind + command children). Per SDK docs, the handler must leave `Handled=FALSE`: WebView2 then displays its default menu **with our appended items**; picks arrive via each item's `add_CustomItemSelected`. (`Handled=TRUE` would suppress the runtime-drawn menu entirely and demand a host-drawn one — first build did exactly that by mistake, killing right-click altogether; there is no `ShowMenu` method in this API.)
- **Linux:** `QWebEngineView::customContextMenuRequested` + `QWebEnginePage::createStandardContextMenu()` returns the stock `QMenu`; append a separator and an "Encoding" `QMenu` built from the shared list, `exec()`, dispatch via `runJavaScript`.

Both paths end at the same page-side executor (D2), so no JS→host commands and no persistence are involved. This also matches the plugin's existing precedent for host-owned menus: the dirviewer shell menu (`CMD_MENU|path` → `showPopupMenu`) — different pages, zero interaction.

Gating placement differs by construction order but is behaviorally equivalent:
- **Windows:** the factory knows the processor when the controller completes, so both registrations are gated on `processor->supportsEncodingOverride()` (virtual on `ProcessorInterface`, overridden true only by `HtmlProcessor`/`MhtProcessor`).
- **Linux:** `EdgeLister_Linux` constructs the backend before `Navigator::Open` picks a processor, so the handler is always connected and gates itself at popup time on the current page URL: `ev://local.example…` (HTML) or `ev://assets.example/mhtml/` (MHT loader). Every other view gets the stock menu untouched.

### D2: Page-side executor contract `window.__evEncodingApply(tag)`

The menus own nothing but dispatch: picking an entry runs `window.__evEncodingApply && __evEncodingApply('<tag>')` through `ExecuteScript` (Win) / `runJavaScript` (Linux); `null` means Auto-detect. Two providers:

- **HTML pages**: the document-created bootstrap (same registration site as the CSS injector, same `local.example` gate) defines the executor: forced charset = fetch-from-own-origin + `TextDecoder` + `document.open/write/close`; auto = `location.reload()`. Window expandos survive `document.open()` rewrites in Chromium, so the executor — and therefore the menu — keeps working after every forced re-decode.
- **MHT loader**: `mhtml/loader.html` defines the same executor over its retained `rawBytes` and `render()` pipeline. The loader lives under `NavigateToString`, so no bootstrap is needed.

Error reporting is a tiny self-contained toast helper inlined at both provider sites (the former shared `EncodingMenu.notify` is gone).

The intermediate shared asset `Resources/assets/encoding-menu.js` was **deleted**: with both menus native, no in-page UI component remains.

### D3: HTML re-decode via fetch-from-own-origin + `document.write`

`apply(label)` on an HTML page: `fetch(location.href)` → `arrayBuffer()` → `new TextDecoder(label).decode()` → `document.open(); document.write(html); document.close();`

- Same-origin fetch keeps every relative subresource URL resolving (origin/base unchanged), so images/CSS of the archived page keep working.
- No interceptor: unlike the removed machinery, nothing touches `WebResourceRequested` or the `ev://` scheme handler. AGENTS.md's prohibition on ad-hoc interceptor re-adds is honored.
- `OfflineMode=1` compatibility: the fetch targets the file's own `local.example` URL, which the shared `IsLocalUri` policy classifies as plugin-local, so it is not blocked.
- Auto-detect reset: simply re-`Navigate()` to the same URL (host reload), restoring engine sniffing.

### D4: MHT re-decode via scoped `TextDecoder` override around `convert()`

Forcing a charset must beat the library's internal decision order, including the "header lies" case. Rather than forking the minified vendored `mhtml2html.min.js` (rejected: unmaintainable diff), the loader temporarily replaces the global `TextDecoder` binding while `convert()` runs:

```js
const RealTD = window.TextDecoder;
window.TextDecoder = (label, opts) => new RealTD(forcedLabel, opts);
try { h = mhtml2html.convert(src); } finally { window.TextDecoder = RealTD; }
```

Why this works: the library's fallback path decodes candidate text via `TextDecoder(detected, {fatal:true})`; overriding the constructor forces every such attempt to use the user's label, and a failing decode lands in the library's existing catch (raw passthrough), never blanking the view. Parts that are pure ASCII (base64 images, QP payloads, boundaries) are unaffected. Auto-detect mode calls `convert()` normally.

Coverage gap found during implementation and folded into D4: when a part declares **no** charset anywhere, the library skips `TextDecoder` entirely and keeps the raw latin1 string — the constructor patch alone cannot intercept that branch. The loader therefore first rewrites every `Content-Type: text/html` header line to carry `charset="<forced label>"` (replacing an existing value or inserting one). Header rewriting is ASCII-safe (boundaries/base64/QP payloads are untouched) and routes both branches through the library's own header-driven decode path, where the patched constructor applies the user's label.

### D5: Curated encoding list (identical on both platforms)

Fixed array in `EdgeViewer/EncodingList.h` (`EncodingList::kItems` — display label + `TextDecoder` tag; empty tag = Auto-detect), included by both backends so the menus can never drift. Labels: Auto-detect; Unicode (UTF-8, UTF-16LE); Western (windows-1252, ISO-8859-15, ISO-8859-1); Central European (windows-1250, ISO-8859-2); Cyrillic (windows-1251, KOI8-R, ISO-8859-5); Greek (windows-1253, ISO-8859-7); Turkish (windows-1254); Hebrew (windows-1255); Arabic (windows-1256); Baltic (windows-1257); Vietnamese (windows-1258); Thai (windows-874); Japanese (Shift-JIS, EUC-JP); Simplified Chinese (GBK, GB2312→gbk label); Traditional Chinese (Big5); Korean (EUC-KR). No runtime enumeration API exists; the list is data, trivially editable.

### D6: C++ surface — two backend functions + one flag

- `WebViewFactory.cpp`: `AddEncodingBootstrapScript` (executor for HTML pages) and `AddNativeEncodingMenu` (ContextMenuRequested wiring), both called from the controller-completed callback when `processor->supportsEncodingOverride()`.
- `QtWebEngineBackend.cpp`: same executor bootstrap block plus the `customContextMenuRequested` handler in the constructor.
- `ProcessorInterface.h`: `virtual bool supportsEncodingOverride() const { return false; }`; overridden true in `HtmlProcessor.h` / `MhtProcessor.h`.
- New shared header `EdgeViewer/EncodingList.h` (D5).

Nothing else in `EdgeViewer/` moves: `IWebView`, WLX exports, the JS→host bridge, and all other processors are untouched.

### D7: Dependency status

None added — `TextDecoder`, `fetch`, `document.write` are engine built-ins. **vcpkg.json is unchanged.**

## Risks / Trade-offs

- [Injected script re-run after `document.write`] → Resolved by design: WebView2 does *not* re-fire document-created scripts after a rewrite (observed), which is exactly why the menu moved host-side; the executor itself survives as a window expando. The CSS link is re-appended explicitly inside the executor (dedup id `ev-html-style-link`).
- [Archived page with aggressive CSP] → Document-created scripts run before page scripts and are not subject to page CSP in either engine; low risk. The native-menu dispatch path (`ExecuteScript`/`runJavaScript`) is not CSP-gated.
- [Whole-view rewrite loses engine-managed state (zoom persists via host, scroll resets)] → acceptable for a corrective action; scroll-to-top is arguably desired after a re-render.
- [Very large files decoded twice in memory] → transient spike bounded by file size; viewers rarely exceed a few MB.
- [`escape`-trick false positive in MHT (bytes happen to be valid UTF-8 while truly being another codepage)] → decode succeeds silently as today; no regression, just an inherent limit of the library's order — forced override cannot intercept what never reaches `TextDecoder`.
- [Right-click conflicts] → dirviewer's shell-menu `contextmenu` handler lives on different pages (dirviewer loader), no overlap with the `local.example` gate or the MHT loader. On Linux the URL gate also keeps the Encoding submenu off dirviewer/markdown/etc.
- [WebView2 SDK surface] → requires `ICoreWebView2_11`/`Environment9`; vcpkg pins webview2 1.0.2277.86, well above the 1.0.1108 minimum. Both queries fail soft (feature silently absent) if an older loader runtime ever appears.

## Migration Plan

Purely additive C++/loader edits; the transient DOM-menu asset was added then removed within this change. Rollback = drop the two registration calls. No config, no schema, no stored state to migrate.

## Open Questions

None blocking. Exact wording/grouping of the menu entries can be tuned during review without touching specs.
