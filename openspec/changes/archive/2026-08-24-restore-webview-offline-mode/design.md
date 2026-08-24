# Design: restore-webview-offline-mode

## Context

Master's `[Chromium] OfflineMode` lived inside the same `WebResourceRequested` handler that served the HTML charset override (`EdgeViewer/WebView2.cpp:154-158`): a filter on `*`/ALL contexts returned `CreateWebResourceResponse(nullptr, 403, L"Blocked")` for every intercepted request not routed to the encoding override. It worked precisely because `SetVirtualHostNameToFolderMapping()` bypasses interception for mapped hosts — so local content never hit the handler. The port retired that whole machinery (proposal §Removed, design Decision 6/10); #63 re-introduces only the offline half as a dedicated cross-platform change. On Linux there is no per-request handler yet; Qt Web Engine exposes `QWebEngineUrlRequestInterceptor` (profile-level, works with the existing global `ev://` scheme handler).

Config plumbing already exists: `GlobalSettings()["WebView"]` cached parse + `to_int` are used by `KeepZoom` in `WebViewFactory.cpp`; Linux reads the same parse via `Globals.h`.

## Goals / Non-Goals

**Goals:**
- One URI-classification policy shared by both backends, unit-testable without COM or Qt objects.
- Windows parity with master's observable behavior (403-empty response; local virtual-host content unaffected).
- Linux blocking at profile level so every page of the shared default profile is covered.
- Zero behavior change when the key is absent/0.

**Non-Goals:**
- The HTML charset override (`OverrideEncoding`, #57) — stays removed; its future handler can join this interceptor later.
- Allow-listing specific remote hosts (no `[WebView] OfflineAllow=` syntax) — can be added on demand.
- Per-view dynamic toggling at runtime — the value is read from the cached ini parse at view creation, like every other key.

## Decisions

**D1 — Shared predicate in a platform-neutral unit (`EdgeViewer/WebPolicy.{h,cpp}`).**
A single function, e.g. `bool IsLocalUri(const std::wstring& uri)` (+ narrow-string overload for the Qt side), classifies a URI:
- scheme `about`, `data`, `blob` → local;
- authority exactly `assets.example` or `local.example` → local (the fixed hosts registered by `ProcessorInterface::mapDomains`);
- scheme `ev` → local (Linux scheme + `_close`/`_cmd` bridges);
- everything else (remote schemes, `file:` outside the mapped hosts) → non-local.
Both backends call it, so the two platforms block identically by construction, and EdgeViewer.Tests covers the table directly. Alternative considered: each backend hardcodes its own check — rejected, drift risk and untestable behind COM/Qt.

Note on Windows: requests to virtual-host-mapped resources historically do not fire `WebResourceRequested` events, but the predicate still lists those hosts explicitly — harmless today, correct if WebView2 ever starts reporting them.

**D2 — Windows: register the interceptor per view in `WebViewFactory.cpp`.**
New setup helper next to `DisableBrowserHotkeys`/`SetColorProfile`: when `to_int(GlobalSettings()["WebView"]["OfflineMode"])`, call `AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL)` and add a handler that returns an empty `403 Blocked` response via `ICoreWebView2Environment::CreateWebResourceResponse` for URIs where `!IsLocalUri(...)`. The environment pointer is reachable through `ICoreWebView2_2::get_Environment` (same as master). When the key is off, no filter is registered at all — no overhead, exact pre-change behavior.
Alternatives: `COREWEBVIEW2_WEB_RESOURCE_CONTEXT_SCRIPT/IMAGE/...` per-context filters — unnecessary granularity; document-level `Navigate` veto doesn't exist synchronously in WLX flow.

**D3 — Linux: one profile-level `QWebEngineUrlRequestInterceptor`.**
In `QtWebEngineBackend`'s once-only `std::call_once` block (beside the scheme registration), if offline mode is on, `defaultProfile()->setUrlRequestInterceptor(new EvOfflineInterceptor)` whose `interceptRequest(QWebEngineUrlRequestInfo&)` sets `block(true)` when `!IsLocalUri(info.requestUrl().toString().toStdWString())`. Profile-level (not page-level) so all pages share it; installed once because config is cached process-lifetime anyway. The `ev://` scheme is allow-listed by D1, keeping the ESC/zoom bridges functional.
Alternatives: `QWebEnginePage::certificateError`-style hooks (wrong layer); blocking inside `EvSchemeHandler` (only sees `ev://`, never external URLs).

**D4 — Config surface: read-only key, template entry added.**
No new parsing code — mINI + `GlobalSettings()["WebView"]["OfflineMode"]`. Shipped `Resources/edgeviewer.ini` gains a commented-default `OfflineMode=0` under `[WebView]` documenting the privacy/offline use case. No vcpkg.json changes (Qt Web Engine types come from the system qt6-webengine-dev; WebView2/wil already pinned).

**D5 — Tests target the predicate only.**
`EdgeViewer.Tests` adds a tier covering `IsLocalUri` (virtual hosts, ev/about/data/blob allowed; http/https/file/ws blocked; case-insensitivity; malformed URIs default to non-local = fail-closed). Backend wiring itself stays manual-verified (TC/DC load), consistent with the suite's scope.

## Risks / Trade-offs

- [Engines differ in what fires an event vs. gets blocked] → Both backends use the same D1 predicate on the same URI strings they receive; fail-closed default means unknown URIs are blocked while offline, never leaked.
- [blob:/data: allowance could be abused to smuggle remote content] → Engines disallow network access from data/blob origins for subresources in practice; master had the same posture (it blocked even less selectively than we do — we are strictly more permissive only for scheme-internal URIs).
- [Mermaid/MathJax lazy-loading web fonts from CDN] → Under offline mode those fetches will fail by design; documents must rely on local assets (all shipped loaders do).
- [Qt interceptor applies process-wide] → Acceptable: DC plugins are single-profile, config is process-cached; matches the existing process-wide `EvSchemeHandler` precedent documented in `QtWebEngineBackend.cpp`.
- [403 Blocked page may look bare] → Same UX as master; the failure is visible rather than silent.

## Migration Plan

Opt-in key: ship default `OfflineMode=0`; no user action needed. Rollback = remove the ini line or reload the previous DLL. Spec deltas merge into `plugin-config` + new `offline-mode` capability at archive; future-work row 9 and Readme updated in the same change.

## Open Questions

None.
