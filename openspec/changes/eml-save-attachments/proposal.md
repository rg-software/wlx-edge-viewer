## Why

Issue #65: the EML processor already detects and lists attachments (`Resources/assets/eml/eml.js` renders an `Attachments` list), but they are inert text — the user cannot save an attachment to a folder of their choosing. A WebView alone cannot write to an arbitrary local path: JS is sandboxed and has no folder picker. Saving therefore requires a JS→host round-trip so the plugin's host code (which knows the original file and can show a native folder picker) writes the bytes to disk.

## What Changes

- **Clickable attachment footer**: the EML loader renders each non-inline attachment as a clickable entry (name + size + MIME). Clicking one initiates a save.
- **New JS→host save request**: the EML loader posts a message carrying the attachment bytes (already base64 in the renderer) plus the suggested filename. This traverses the existing per-OS JS→host bridge:
  - Windows: `window.chrome.webview.postMessage` → `WebMessageReceived` (WebViewFactory.cpp:172) → host handler.
  - Linux: the `ev://_cmd/<id>/<message>` shim (QtWebEngineBackend.cpp:173-215) → host `WebEngineUrlRequestJob` handler.
- **Host→JS reply channel**: a new capability so the host can report the folder-picker result back into the rendered view (success → confirmation; cancel → no-op; failure → error message). This is the main new infrastructure: today `IWebView` is one-way JS→host only (`ExecuteScript` is fire-and-forget).
- **Native save surface**: per-OS folder picker + file write, behind the existing `fs` (`Platform.h`) abstraction:
  - Windows: Total Commander folder-selection / `SHBrowseForFolder`-style dialog + `ofstream` write.
  - Linux: `QFileDialog::getExistingDirectory` + `std::ofstream`, routed through the QtWebEngine backend.
- **Payload sizing**: the save message carries a base64 string that may be multi-MB. Sizing/limits are validated; both transports must not truncate at WM_COPYDATA/URL-size ceiling. (Design Decision.)

Out of scope: saving inline (cid-referenced) images as separate files; drag-to-extract; ZIP-of-all. This change is limited to clicking one attachment and saving it to a folder.

## Capabilities

### New Capabilities

- `host-js-bridge/spec`: bidirectional JS↔host command channel — the existing one-way JS→host paths (CMD_KEY/CMD_MENU/CMD_ZOOM) are unchanged, but this change adds the save request direction **and** the host→JS reply path needed to surface the folder-picker result. This is the platform-agnostic contract both backends implement.

### Modified Capabilities

- `eml`: the EML capability gains updated requirements: attachments render as an interactive save footer, and a successful save writes the attachment to a user-chosen folder. The existing `Attachment in multipart mixed` scenario (currently "listed so the user can see it was attached") is extended to "listed and savable".
- `virtual-host-mapping`: no behavior change on its own, but the Linux save path relies on the same `ev://` scheme mapping; referenced by the design, not a requirement change. (Not listed; no delta needed.)

## Impact

- **Affected code** (`EdgeViewer/`): `IWebView.h` gains a host→JS reply method (e.g. `PostJavaScriptResult`/callback registration); `WebView2Backend.{h,cpp}` and `QtWebEngineBackend.{h,cpp}` implement it; new save-command dispatch in `WebViewFactory.cpp` (Windows) and the `ev://_cmd` handler (Linux); a folder-picker + write helper behind `Platform_Win.cpp`/`Platform_Linux.cpp`; `Processors/BaseFileProcessor.cpp`/EML processor wiring if the reply needs to reach the loader.
- **Affected assets** (`/Resources/assets/eml/`): `loader.html`, `eml.js`, `style.css`, `style-dark.css` — attachment footer markup, click handlers, and postMessage integration.
- **APIs**: `IWebView` interface grows; both backend implementations change; no WLX export signature changes.
- **Dependencies**: none new. Bytes come from the existing PostalMime parse (JS) and are written with the standard library (`std::ofstream`) / OS dialog APIs. No vcpkg addition.