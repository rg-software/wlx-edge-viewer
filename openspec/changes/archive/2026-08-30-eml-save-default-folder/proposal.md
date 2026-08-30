## Why

When a user saves an attachment from an `.eml` message (issue #69), the native folder picker opens at the OS/session default location, so the user has to manually navigate to the folder where the email was opened — a needless step in a file manager where the user almost always saves next to the `.eml` they are viewing. Opening the picker at the directory containing the currently-viewed `.eml` file removes that friction.

## What Changes

- The folder picker used when saving an EML attachment SHALL open pre-selected on the directory that contains the `.eml` file currently being viewed (its "current folder"), instead of the OS/session default.
- The change applies to both backends: Windows (WebView2) and Linux (Qt Web Engine), and to both 32-bit and 64-bit Windows builds.
- The default only affects the picker's initial directory; the user can still navigate anywhere and cancel exactly as before. When the folder is cancelled or the write fails, behavior is unchanged.
- Always-on: no new `edgeviewer.ini` key. The picker's initial directory is derived from the file the plugin is already rendering, so no config is needed.

## Capabilities

### New Capabilities

- none

### Modified Capabilities

- `eml`: "Save attachment to chosen folder" — the save flow SHALL open the folder picker defaulted to the directory containing the `.eml` file being viewed rather than the OS default.

## Impact

- `EdgeViewer/IWebView.h` — add methods to carry the current file directory (set by the processor during `OpenIn`, read back by the save flow).
- `EdgeViewer/Processors/BaseFileProcessor.{h,cpp}` — set the current file directory on the view from `mPath` during `OpenIn` (covers the EML loader-based processor).
- `EdgeViewer/WebView/WebView2Backend.{h,cpp}` (Windows) and `EdgeViewer/WebView/QtWebEngineBackend.{h,cpp}` (Linux) — store the directory; the save handlers read it.
- `EdgeViewer/WebView/WebViewFactory.cpp` (`HandleSaveAttachment`) and `EdgeViewer/WebView/QtWebEngineBackend.cpp` (`HandleLinuxSave`) — read the current directory and pass it as the picker default.
- `EdgeViewer/Platform.h`, `EdgeViewer/Platform_Win.cpp`, `EdgeViewer/Platform_Linux.cpp` — `PickFolder` gains an optional default-directory parameter; each native dialog honors it.
- No vcpkg.json, no new dependency, no JS/CSS/loader change.
