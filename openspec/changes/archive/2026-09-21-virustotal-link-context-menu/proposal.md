# Proposal: Send link to VirusTotal from the link context menu

## Why

Users reviewing rendered documents (HTML, MHT, EML, Markdown, RST, AsciiDoc) frequently want to check a suspicious link before clicking it: `https://www.virustotal.com/gui/url/<link>`. Today they must copy the link address and paste it into a browser tab — an extra round trip that breaks flow. Adding a "Send link to VirusTotal" entry to the engine's built-in link context menu makes link vetting a single right-click away, and it opens in the user's default browser exactly like a normal link launch would.

## What Changes

- Add a **"Send link to VirusTotal"** item to the engine's built-in right-click menu, shown only when the user right-clicks a **web link** (`http:`/`https:`) inside any rendered view.
- The item opens `https://www.virustotal.com/gui/url/<link>` in the user's **default browser** (Windows `ShellExecuteW`, Linux `QDesktopServices::openUrl`) — the same guaranteed-default-browser path, not an in-viewer navigation.
- Applies to **all processor views** that can render a web link, not a fixed processor list: HTML, MHT, EML, Markdown, RST, AsciiDoc, URL/Other pages. Image, directory, and PDF views never surface the item (no link under the cursor; the directory viewer's links are local `local.example`/`ev://` file refs, which are not scannable).
- On Windows the context-menu hook becomes **unconditional** (today it is registered only for `supportsEncodingOverride()` processors, `WebViewFactory.cpp:451`); the Encoding submenu stays gated on encoding-override support exactly as now. Linux already runs its menu code on every view.
- No new ini keys, no configuration, no persistence. The entry is a fixed, always-present menu item gated purely by the link under the cursor.

## Capabilities

### New Capabilities

- `virustotal-link-scan`: Adds a "Send link to VirusTotal" entry to the engine's built-in link context menu on both platforms (WebView2 and Qt Web Engine). The entry appears whenever an `http:`/`https:` link is right-clicked in any rendered view, and opening it launches the service URL in the user's default browser. The item is scoped by the right-clicked link's presence and scheme, not by plugin processor type.

### Modified Capabilities

- `encoding-override`: The Encoding submenu remains HTML/MHT-only and otherwise unchanged; the shared context-menu hook it rides on becomes unconditional (now also installed on non-encoding views). The observable Encoding behavior is identical — no requirement text changes, so **no delta spec for this capability**; the hook change is an implementation detail tracked in `design.md`/`tasks.md`.

## Impact

- `EdgeViewer/WebView/WebViewFactory.cpp` — `AddNativeEncodingMenu`: split out or extend the `ContextMenuRequested` handler so it registers on every view; add the VirusTotal item gated on `get_LinkUri()`, dispatch via a browser-launch helper.
- `EdgeViewer/WebView/QtWebEngineBackend.cpp` — `ContextView::contextMenuEvent`: add the item gated on `lastContextMenuRequest()->linkUrl()`, launch via `QDesktopServices::openUrl`.
- New shared helper (e.g. `EdgeViewer/UrlLauncher.{h,cpp}` or platform files) to build the VirusTotal URL and open the default browser, mirroring how `Platform_*.cpp` already split filesystem work.
- Windows-only (no Linux test suite): builds via MSBuild release Win32/x64; Linux via CMake.