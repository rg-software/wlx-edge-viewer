## Why

Issue #77: opening a Markdown or HTML file on an SMB/UNC share (`\\server\share\...`) renders the text but breaks relative references — `Platform_Win.cpp::GetPhysicalPath` copies the file to `%TEMP%` (the UNC-support workaround from v1.0.8 / issue #29, whose own text accepted "without relative links via copy to a temp location"), so `<img>`, CSS and sibling links resolve against the temp directory instead of the share. VSCode has no such problem because it serves from the real folder.

The copy exists because WebView2 cannot load a share directly: Chromium blocks `file://` to network paths and `SetVirtualHostNameToFolderMapping` only maps local folders, while the sandboxed renderer carries no credentials for the share. But the plugin's own host process (Total Commander) does carry them. The Windows backend already ships a host-side file server for exactly this purpose — the `evh://` custom scheme whose `WebResourceRequested` handler reads the mapped folder with `std::ifstream` in the plugin process (built for ForcedHtmlExt). Linux needs nothing: its `ev://` handler already reads every file host-side and `Platform_Linux.cpp::GetPhysicalPath` never temp-copies, so SMB mounts already work there.

## What Changes

- **Stop copying UNC files to temp.** `Platform_Win.cpp::GetPhysicalPath` returns the plain `\\server\share\...` path (prefix-stripped) for UNC files instead of `GenTempFile()`. `GenTempFile`/`gs_tempFiles` remain for the only remaining caller path (oversized loader HTML past `NavigateToString`'s 2 MB cap).
- **New `IsNetworkPath()` platform helper** (`Platform.h` + `Platform_Win.cpp` / `Platform_Linux.cpp`). Windows: true for `\\server\share\...` and `\\?\UNC\...`, robust against `\\?\C:\...` extended local paths. Linux: always false (mounts are local paths).
- **Processors route network-rooted content through the host-side scheme on Windows.** `BaseFileProcessor` (Markdown/RST/AsciiDoc/MHTML/EML + image loader) rewrites the loader's `http://local.example` references to `evh://local.example` for network roots before `NavigateToString`; `HtmlProcessor` and `OtherProcessor` navigate through `evh://` (the same scheme ForcedHtmlExt already uses). The existing `WebResourceRequested` handler then serves the file and its relative subresources from the share, resolving against the real directory.
- **`WebPolicy::IsLocalUri` accepts `evh:`** alongside `ev:`, so OfflineMode and the navigation policy keep treating plugin-served content as local.
- **No change on Linux.** Processors guard the routing behind `#ifdef _WIN32`; the Linux `NavigateToString`/`Navigate` `http:// → ev://` rewrite already covers everything.

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities
- `temp-file-management`: The "UNC path temp-copy" requirement becomes "UNC path served in place": a UNC file is not copied; it is rendered from the share's path through the `evh://` scheme (host-side read), so relative references resolve against the share. The "ForcedHtmlExt temp-copy" requirement becomes "ForcedHtmlExt served in place" (this was already the live behavior after the `load-html-via-navigate` change; the spec text is reconciled). The temp-file-generation scenario and the 32/64-bit parity scenario are updated accordingly.
- `plugin-config`: The `ForcedHtmlExt` requirement text is reconciled to match the current in-place serving model (no `.html` temp copy, no per-file cleanup).

## Impact

- **Code**: `Platform.{h,cpp}` (`IsNetworkPath`, `GetPhysicalPath`), `BaseFileProcessor.cpp`, `HtmlProcessor.cpp`, `OtherProcessor.cpp`, `WebPolicy.cpp`. `WebView2Backend.cpp` / `WebViewFactory.cpp` unchanged — the `evh://` handler and scheme registration already exist for ForcedHtmlExt.
- **Static assets**: none.
- **Specs**: `openspec/specs/temp-file-management/spec.md`, `openspec/specs/plugin-config/spec.md` via delta specs; `linux-parity` and notes updated to match.
- **Behavioral risk to verify manually**: on a real SMB share, confirm (a) HTML files with relative subresources render (same-origin `evh://`; already proven by ForcedHtmlExt) and (b) loader-based views' relative images/CSS load (non-CORS subresource fetches from `about:blank`). In-viewer cross-file XHR from `about:blank` to `evh://` may be origin-restricted; that is a pre-existing secondary feature and not the reported bug.