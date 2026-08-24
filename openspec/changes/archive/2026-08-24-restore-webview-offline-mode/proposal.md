# Proposal: restore-webview-offline-mode

## Why

The `[Chromium] OfflineMode=1` key (block all external resource fetches, render documents offline) was dropped during the `[Chromium]` → `[WebView]` rename on the cross-platform port and is currently silently ignored (`plugin-config` spec). It is the last untracked feature drop of that rename (#58 restored `Switches`/`BrowserExecutable*Folder`; the HTML charset override is tracked separately as #57). A user-facing offline-rendering switch is worth restoring now: it is opt-in (`OfflineMode=0` default keeps today's behavior), and both engines already expose the interception machinery needed on each platform.

## What Changes

- Re-introduce `[WebView] OfflineMode` as a functional key on **both** platforms:
  - `OfflineMode=1`: every web request that does not resolve to plugin-local content is blocked before it reaches the network — Windows returns an empty `403 Blocked` response via a `WebResourceRequested` handler; Linux blocks the request in a profile-level `QWebEngineUrlRequestInterceptor`. Documents render fully from virtual-host/scheme-served local content (assets, the rendered file, data/blob-embedded resources).
  - `OfflineMode=0` / absent: unchanged behavior (external fetches allowed).
- Local content stays reachable under OfflineMode=1: the `assets.example` / `local.example` virtual hosts (Windows) and the `ev://` scheme handler + internal bridges (`ev://_close`, `ev://_cmd`) plus `about:`/`data:`/`blob:` URIs (both platforms) are never blocked.
- External navigations are blocked too while OfflineMode=1 — notably `url-files` rendering of remote `URL=` targets shows the engine's load failure instead of the site. This mirrors master's behavior.
- Update the shipped `Resources/edgeviewer.ini` template with a documented `OfflineMode=0` entry.
- Docs/spec bookkeeping: remove the "silently ignored" note, close future-work row 9.

## Capabilities

### New Capabilities

- `offline-mode`: cross-platform observable behavior of `[WebView] OfflineMode` — what counts as local vs external traffic per platform, blocking semantics when enabled, pass-through semantics when disabled/absent.

### Modified Capabilities

- `plugin-config`: the "[WebView] section keys" requirement changes — `OfflineMode` is no longer "removed and silently ignored"; it becomes a read-on-both-platforms key with defined behavior (row added to the key table, scenario updated).

## Impact

- **C++ (Windows)**: `EdgeViewer/WebView/WebViewFactory.cpp` — register the request interceptor alongside the other per-view setup helpers; policy predicate shared with Linux lives in a new small platform-neutral unit (e.g. `EdgeViewer/WebPolicy.{h,cpp}`) so both backends classify URIs identically and EdgeViewer.Tests can cover it.
- **C++ (Linux)**: `EdgeViewer/WebView/QtWebEngineBackend.cpp` — install a `QWebEngineUrlRequestInterceptor` on the default profile once, next to the existing once-only scheme-handler registration.
- **Config**: `Resources/edgeviewer.ini` (+ nothing else; parsing needs no new code paths beyond reading the cached key).
- **Tests**: `EdgeViewer.Tests` gains cases for the URI-classification predicate (no COM/Qt objects involved).
- **Docs**: `Readme.md`, `openspec/notes/future-work.md` row 9, issue #63 closure.
- No new dependencies; `vcpkg.json` untouched. Identical ini semantics for Win32/x64 (32/64-bit config parity requirement unaffected).
