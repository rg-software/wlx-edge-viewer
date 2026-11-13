# Proposal

## Why

In Total Commander quick view (Ctrl+Q), the WebView2 control consumes `Tab` for in-page link navigation, trapping focus and breaking TC's panel-switching shortcut. Standalone lister (F3) has no such conflict — `Tab` link navigation is desirable there. There is no `ShowFlags` quick-view bit, so the plugin currently cannot distinguish the modes.

## What Changes

- Detect quick-view mode per lister view at `ListLoadW` time via `WS_CHILD` on `ParentWin` (child panel = quick view, top-level = F3 lister), and retain the mode per view for the view's lifetime.
- Add an ini-gated `Tab` policy: in quick-view mode with the option enabled, intercept `Tab`/`Shift+Tab` in the WebView2 accelerator handler, suppress in-page handling, and forward the key to the TC parent window; in F3 mode (or with the option disabled) keep current in-page navigation.
- Explicitly out of scope: `Ctrl+Tab` handling (unchanged — stays swallowed by the page, matching peer viewers).

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `accelerator-keys`: Tab handling becomes view-mode dependent (forward in quick view when enabled, navigate in page otherwise).
- `plugin-config`: new `[QuickView] PassTabToTC` key driving the Tab policy.
- `wlx-contract`: `ListLoadW` records per-view quick-view mode from the parent window style.

## Impact

- Affected code (Windows-only, identical on Win32/x64): `EdgeViewer/DllMain.cpp` (`ListLoadW` detection + per-view store), `EdgeViewer/WebView/WebViewFactory.cpp` (`AddAccleratorKeyHandler` Tab intercept + `put_Handled`), `EdgeViewer/EdgeLister_Win.cpp` (existing `WM_WEBVIEW_KEYDOWN` relay path reuse), `EdgeViewer/Globals.{h,cpp}` (per-view mode table), `Resources/edgeviewer.ini` (+ shipped sample) for the new key.
- No dependency changes (`vcpkg.json` untouched); no Linux behavior change (Qt Web Engine focus handling unchanged).
- Docs: user-facing `Readme.md` quick-view note; `openspec/notes/future-work.md` Tab entry if one exists.
