# Design: Serve UNC/SMB files in place (issue #77)

## Problem

`Platform_Win.cpp::GetPhysicalPath` copies any UNC-rooted file to `%TEMP%` (v1.0.8 UNC support, issue #29) as a workaround for WebView2's inability to read shares:

- Chromium blocks `file://` access to network paths.
- `ICoreWebView2::SetVirtualHostNameToFolderMapping` (our `http://local.example` virtual host) only maps **local** folders.
- The sandboxed renderer process has no credentials for the share.

The copy means relative references (`<img>`, CSS, sibling links) inside HTML/Markdown resolve against the temp folder, breaking them on SMB (issue #77). VSCode works because it serves from the real folder.

## Observation

The bytes are only unreadable *by the renderer*. The plugin's host process (Total Commander) has the user's credentials, and the codebase already ships a host-side file server for forced `ForcedHtmlExt` files: the **`evh://` custom scheme** (`WebViewFactory.cpp:386-392`) whose `WebResourceRequested` handler (`WebView2Backend.cpp:189-290`, `BuildForcedHtmlResponse`) maps `evh://<host>/<rel>` → `<registered-folder>/<rel>` and reads each file with `std::ifstream` in-process, answering with MIME from `MimeForPath()`. Linux's `ev://` handler is the sibling pattern and has never needed a temp copy (`Platform_Linux.cpp::GetPhysicalPath` is symlink-resolution only).

## Approach

### 1. Path layer: `IsNetworkPath()` + stop the temp copy

- New platform helper `bool IsNetworkPath(const fs::path&)`. Windows recognizes UC-stripped `\\server\share\...` and the raw `\\?\UNC\...` form, stripping `\\?\` first so `\\?\C:\...` extended local paths are not misread as network. Linux returns false (mounts are local paths).
- `GetPhysicalPath` (Windows) drops the `GenTempFile` UNC branch and returns the plain `\\server\share\...` path for both UNC files and directories (which were already returned as-is).
- `GenTempFile`/`gs_tempFiles`/`RemoveTempFiles` stay: `RemoveTempFiles` still clears the oversized-loader temp files (`WebView2Backend::NavigateToString` past its 2 MB wchar cap pushes `lister.example` temp files onto the same list). No call sites for `GenTempFile` remain; it is left as a tested helper.

### 2. Routing: processors use `evh://` for network roots (Windows only)

`mapDomains(webView, mPath.root_path())` already registers `local.example → <root>` and stores it in the backend's host map (`m_hostFolders`) that the `evh://` handler reads. For a UNC file, `root_path()` is `\\server\share\`, so the existing handler resolves `evh://local.example/<rel>` → `<share>/<rel>`.

- **`BaseFileProcessor`** (Markdown, RST, AsciiDoc, MHTML, EML, image loader): after placeholder substitution, if `IsNetworkPath(mPath)` rewrite `http://local.example` → `evh://local.example` in the generated loader (all loader templates reference the host literally; the `#ifdef _WIN32`-guarded rewrite is a plain string replace).
- **`HtmlProcessor` / `OtherProcessor`**: compute `scheme = (forced || IsNetworkPath(mPath)) ? evh:// : http://` on Windows; the `evh://` main-document navigation is the same code path ForcedHtmlExt already uses (proven in production).

### 3. Policy: `evh:` is local

`WebPolicy::IsLocalUri` gains `evh` next to `ev`, so `[WebView] OfflineMode` and the navigation policy treat plugin-hosted scheme content as local (the ≤2 MB loader pages and UNC docs would otherwise be 403-blocked).

### 4. Why loaders work over `evh://`

- The file bytes themselves are base64-inlined into the loader (pre-fetch), so the renderer never touches the share for the document text.
- Relative subresources (images, CSS) resolve against the loader's `<base href>` → `evh://local.example/…` and are served by the host-side handler. Image/CSS loads are non-CORS subresource fetches, so the `about:blank` document origin does not gate them.
- In-viewer cross-file navigation (md/rst loader XHR) may be origin-restricted from an `about:blank` document on WebView2; that is pre-existing behavior for the old temp path too and is out of scope for the reported bug (images/CSS).

### 5. Linux

No change: `IsNetworkPath` is false, the `#ifdef _WIN32` guards skip the routing, and the existing `http:// → ev://` rewrite in `QtWebEngineBackend` feeds the host-side handler.

## Alternatives considered

- **Serve the HTML/Other processors through a virtual host on the UNC root**: impossible — `SetVirtualHostNameToFolderMapping` does not map share folders.
- **Copy the whole relative closure**: heavy and fragile; VSCode avoids it by serving from the folder, which the `evh://` handler now does.
- **Keep the temp copy, document the limitation**: rejected per issue #77 request; the plumbing makes the proper fix cheap.