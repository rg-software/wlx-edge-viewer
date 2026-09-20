# Design: Send link to VirusTotal from the link context menu

## Context

The plugin renders documents inside two embedded engines that both ship the Chromium link context menu: WebView2 on Windows (`EdgeViewer/WebView/WebViewFactory.cpp`) and Qt Web Engine on Linux (`EdgeViewer/WebView/QtWebEngineBackend.cpp`). Both backends already extend that built-in menu — the Encoding submenu rides on it (`AddNativeEncodingMenu`, `ContextView::contextMenuEvent`). The VirusTotal item is a second, always-on extension of the same menu, scoped by the right-clicked link rather than by processor type. See `proposal.md` for the motivation and scope decision.

Key constraints from the codebase:

- On Windows the `ContextMenuRequested` handler is registered **only** when the processor reports `supportsEncodingOverride()` (`WebViewFactory.cpp:451`). That registration must become unconditional for the VirusTotal item to reach all views; the Encoding submenu build must then re-gate internally on the backend's per-view `m_encodingOverrideSupported` flag (`WebView2Backend.cpp:334`).
- On Linux `ContextView::contextMenuEvent` already runs on every view (`QtWebEngineBackend.cpp:455`); it gates the Encoding submenu on `m_impl->encodingOverrideSupported`. The VirusTotal item slots into this method with its own gate.
- Both engines expose the right-clicked link: WebView2 `ICoreWebView2ContextMenuRequestedEventArgs::get_LinkUri()`, Qt `QWebEngineView::lastContextMenuRequest()->linkUrl()`.
- The plugin already opens URLs in the OS default browser: `ShellExecuteW` (Windows pattern, see WebView2 new-window sample) and `QDesktopServices::openUrl` (used at `QtWebEngineBackend.cpp:1183` for print output).
- The directory viewer (`DirProcessor.cpp:105`) links to local files under `local.example`/`ev://`; the `http`/`https` gate excludes them from the item.

## Goals / Non-Goals

**Goals:**
- A single "Send link to VirusTotal" menu entry visible on every view when an `http:`/`https:` link is under the cursor, on both platforms.
- Open the VirusTotal URL-analysis page in the user's default browser; no in-viewer navigation.
- Minimal diff: reuse the existing context-menu extension machinery; share the URL-building and launch logic between backends.

**Non-Goals:**
- No new ini keys, no user-configurable list of scanners, no persistence.
- No change to the Encoding submenu behavior (still HTML/MHT-only by processor capability).
- No scanning via the VirusTotal public API (that needs an API key, brings rate limits and a different trust model; the web deep-link is the same tab the user would open by hand).
- No handling for `mailto:`, `file:`, `ev://` etc. — only scannable web links.

## Decisions

### D1. Gate on the link under the cursor, not the processor

Both engines report the hovered link at menu-build time, so the item's visibility is decided there. On Windows the handler reads `get_LinkUri()`; on Linux `lastContextMenuRequest()->linkUrl()`. Only `http:`/`https:` survive → local directory/viewer links, images, and empty areas never show the item.

Rationale vs. a processor-allowlist: a taxonomy would have to be kept in sync with every loader that can render links (Markdown, RST, AsciiDoc, MHT, EML already can) and would wrongly exclude a new processor until edited. The link-under-cursor predicate is the exact condition the user cares about.

Alternative considered: gate on `supportsEncodingOverride()` (HTML/MHT/EML). Rejected — Markdown/RST/AsciiDoc render links too, and the user explicitly wanted EML covered.

### D2. Make the Windows context-menu hook unconditional, re-gate Encoding inside

`QueueConfigureWebView2` currently only calls `AddNativeEncodingMenu` when `processor->supportsEncodingOverride()` is true. The change: always register `add_ContextMenuRequested`, and in the handler:
- build the VirusTotal item when `get_LinkUri()` is an `http(s)` URL;
- build the Encoding submenu only when the view's backend reports `m_encodingOverrideSupported`.

Rationale: matches Linux's structure exactly (`contextMenuEvent` always runs, Encoding gated on the flag), and keeps Encoding's externally observable behavior identical while the hook becomes universal. The backend flag is already set per-load (`SetEncodingOverrideSupported` follows `supportsEncodingOverride()` in `ProcessorInterface`).

Alternative considered: register two separate `ContextMenuRequested` handlers (one unconditional for VirusTotal, one conditional for Encoding). Rejected — two handlers both enumerating the same item collection is more code and harder to reason about than one gate-augmented handler.

### D3. One shared URL-builder + launcher, platform only for the syscall

Add a small shared helper (e.g. `EdgeViewer/UrlLauncher.{h,cpp}`) exposing:
- `std::wstring BuildVirusTotalUrl(const std::wstring& link)` — returns `L"https://www.virustotal.com/gui/url/<escaped>"`;
- `bool OpenInDefaultBrowser(const std::wstring& url)` — Windows: `ShellExecuteW(nullptr, L"open", ...)`; Linux: `QDesktopServices::openUrl`. Implemented per-platform (mirroring `Platform_Win.cpp` / `Platform_Linux.cpp`) so the backends call one function each.

Rationale: `ShellExecuteW` and `QDesktopServices::openUrl` are the correct OS mechanisms for "open in the user's browser" (the plugin owns no browser UI; there is no `NewWindowRequested`/`createWindow` handler in either backend, so the built-in "new window" entry is unreliable for this purpose). Centralizing keeps the two backends consistent and unit-testable for the URL shape.

Alternative considered: virtual method on `IWebView`. Rejected — launching a browser is not a webview operation and has no engine-specific requirements.

### D4. Deep-link: exact escaping form validated at build

VirusTotal's `/gui/url/` page historically accepted a URL-safe base64 of the target URL; the current UI accepts the URL itself (optionally encoded) in the path segment. The plausible forms are:
- `https://www.virustotal.com/gui/url/` + percent-encoded target;
- `https://www.virustotal.com/gui/url/` + URL-safe base64 (no padding).

The helper will percent-encode the target's reserved characters so query/fragment survive the path, and the *implementer must verify* by pasting an encoded sample into a browser which form the current VirusTotal UI accepts, falling back to the other if the first lands on a search/error page. This is the one piece of the design the build task explicitly validates (`tasks.md` T6). It does not change any spec requirement — the spec only requires the full original URL to survive the round trip.

## Risks / Trade-offs

- [D2 restructures Windows menu registration] → The Encoding submenu's gating flag already exists (`m_encodingOverrideSupported`) and is exercised today only on Win32/x64; moving it inside an always-on handler is behavior-preserving but must be sanity-checked on an HTML and a non-HTML view during manual verification.
- [Exact VirusTotal deep-link form is outside our control and may change] → The helper isolates the URL construction in one function; a future site change is a one-line edit. Manual test verifies the landed page contains the scan target.
- [Item noise on every link right-click] → The item is a single fixed entry under the standard link group, matching the "no configuration" goal; removing it later is a one-line delete from the same two menu builders.
- [Linux launches the default browser from the Qt main thread inside a menu event] → `QDesktopServices::openUrl` is non-blocking and the same call already runs in this process for print output; no threading concern.

## Migration Plan

No migration. This is purely additive: a new menu item, an always-on menu hook on Windows, one new shared helper file. Rollback is deleting the hook registration and the helper references; `New` helper is compiled into the Windows and Linux build systems (`.vcxproj`/`.vcxproj.filters` and `CMakeLists.txt` respectively; Linux additionally exports no new WLX symbol, so the version script is untouched).