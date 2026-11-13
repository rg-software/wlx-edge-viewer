# Design

## Context

See `proposal.md` for motivation. Current state: `AddAccleratorKeyHandler` (`EdgeViewer/WebView/WebViewFactory.cpp`) relays every non-zoom `KEY_DOWN` to the TC parent via `WM_WEBVIEW_KEYDOWN` (`EdgeViewer/EdgeLister_Win.cpp`). All changes are C++ in `EdgeViewer/` plus the document keydown bridge script registered from C++; no `Resources/assets/` loader changes, no `vcpkg.json` dependency changes. Linux build untouched.

## Goals / Non-Goals

**Goals:**

- Quick-view `Tab`/`Shift+Tab` reaches TC (panel switch) when `[QuickView] PassTabToTC=1`; page keeps `Tab` navigation in F3 mode and when the option is off.
- Per-view mode (F3 and quick view open simultaneously behave independently); identical behavior on Win32/x64.

**Non-Goals:**

- `Ctrl+Tab` handling (explicitly unchanged); Qt/Linux behavior; keyboard-accessibility redesign of the loaders.

## Decisions

- **Mode detection: `WS_CHILD` bit on `ParentWin` at `ListLoadW` time.** `GetWindowLong(ParentWin, GWL_STYLE) & WS_CHILD` — set means embedded quick-view panel, clear means top-level F3 lister. Rationale: no `ShowFlags` bit exists for this (WLX SDK confirms); proven by predecessor `HTMLView/main.cpp` (`qiuck_view` flag drove per-mode toolbar/statusbar). Alternative (parent-window class-name sniffing, e.g. `TLister`) rejected: hardcoded class names break across TC versions; the style bit is version-agnostic.
- **Per-view store, recorded synchronously.** Mode goes into a `map<HWND/view, bool>` (same lifetime discipline as `gs_Views` / `gs_ZoomFactor` in `EdgeViewer/Globals.h`): written in `ListLoadW` before the async `QueueConfigureWebView2` callback runs, refreshed in `ListLoadNextW`, erased in `ListCloseWindow`. Alternative (global bool) rejected: multiple listers can be open (SDK remarks). The per-view document script below bakes the flag in at webview creation, so there is no init race.
- **Intercept point: page-side document keydown bridge, NOT `AcceleratorKeyPressed`.** Empirically confirmed via `%TEMP%\edgeviewer_qvtab.log`: WebView2 never fires `AcceleratorKeyPressed` for `Tab` (focus-navigation keys are consumed inside Chromium without notifying the host), so a host-side `put_Handled(TRUE)` branch is dead code. Instead the existing `AddScriptToExecuteOnDocumentCreated` keydown bridge (today `Q`/`1-8`) gains a `Tab` branch: when the per-view `window.__evTabForward` flag is set and no `Ctrl`/`Alt`/`Meta` is held, it calls `preventDefault()` (suppresses link navigation) and posts `CMD_TAB`, which the host turns into a genuine `SendInput` Tab press after moving focus out of the Chromium child (a posted `WM_KEYDOWN` is ignored by TC for Tab — verified live), so TC's own loop navigates exactly like a physical key. `Shift` is allowed through (TC treats `Shift+Tab` as panel switch; the page reports both as `code === 'Tab'`); `Ctrl`/`Alt`/`Meta`-held presses are skipped so `Ctrl+Tab` stays a page no-op like peers. Alternative (thread `WH_KEYBOARD` hook a la predecessor `HookKeybProc`) rejected: heavier and riskier than reusing the proven `Q`/`1-8` message channel; alternative (CSS `tabindex=-1`) rejected: destroys F3 keyboard accessibility. No loader claims `Tab` (verified by grep over `Resources/assets/`), so the bridge listener cannot fight page scripts.
- **Config: new `[QuickView] PassTabToTC=1`, shipped default `1`.** Read via the cached mINI parse (`to_int(...)==1` check, so absent/garbage = off). Rationale: view-mode policy does not belong in the engine-runtime `[WebView]` section; a dedicated section leaves room for future quick-view keys without disturbing the `[WebView]` platform-scope table. Shipped `Resources/edgeviewer.ini` (+ sample deployment) carries `1` so upgrades get forwarding; an empty configuration degrades to off (safe = today's behavior).

## Risks / Trade-offs

- [Risk] `WS_CHILD` heuristic is undocumented TC behavior → Mitigation: failure mode is benign (misdetected view just keeps today's in-page `Tab`); confirmed live via debug log (`style=0x46800400`, `isQuick=1`) on TC 64-bit quick view.
- [Risk] Synthetic input must not loop back into the page → Mitigation: focus is moved out synchronously before injecting, so the page can never see the synthetic press; plus a 300 ms host-side guard against echo/repeat.
- [Risk] Page scripts calling `stopPropagation` on `Tab` before the bridge listener → Mitigation: no loader handles `Tab` (verified); bridge runs at document start before page scripts register theirs.

## Migration Plan

Ini-only feature, default-on via shipped sample. Rollout: ship sample with `[QuickView] PassTabToTC=1`; existing user inis without the section keep in-page `Tab` until the user adds it (no forced migration, no code fallback writes to the ini). Rollback: set `0` or remove the key.

## Open Questions

- `PostMessage` vs `SendMessage` for the synthetic `Tab` (ordering vs re-entrancy under TC's subclassed parent): answer during implementation testing; does not change specs or task breakdown.
