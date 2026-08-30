## Context

`eml-save-attachments` (archived) gave each attachment a clickable save that runs `PickFolder()` (a per-OS native folder dialog) then writes the bytes host-side. Today the picker opens at the OS/session default. Issue #69 asks that it instead open at the "current folder" — the directory containing the `.eml` being viewed.

The save flow today:
- Windows: `HandleSaveAttachment` (`WebViewFactory.cpp:328`) → `PickFolder(hWnd)`.
- Linux: `HandleLinuxSave` (`QtWebEngineBackend.cpp:124`) → `PickFolder(info.container)`.

Neither handler knows the opened file's directory. Both backends, however, are reachable from the handler (Windows via `gs_Views[hWnd]`, Linux via `gs_Views[(void*)info.container]`, exactly as the existing `CMD_AUTO_ENCODING` dispatch does), and the EML processor (a `BaseFileProcessor`) already holds `mPath` from `InitPath`.

## Goals / Non-Goals

**Goals:**
- The folder picker opens pre-positioned on the directory containing the `.eml` file being viewed.
- Works on Windows (Win32 + x64) and Linux.
- Always-on, no config key.

**Non-Goals:**
- Remembering a last-saved folder in `edgeviewer.ini`.
- Changing the transport (`CMD_SAVE` payload / result callback) at all.
- New JS/CSS/loader work.

## Decisions

### Decision 1: Carry the current file directory across `IWebView` (set in `OpenIn`, read at save time)

Add two methods to the shared `IWebView`:
- `virtual void SetCurrentFileDirectory(const std::filesystem::path&) {}` — the processor reports the opened file's full path during `OpenIn`.
- `virtual std::filesystem::path GetCurrentFileDirectory() const { return {}; }` — the save handler reads it back.

Both backends (`WebView2Backend`, `QtWebEngineBackend`) store the path in existing members (Windows: alongside `m_baseUri`/`m_lastNavigateUri`; Linux: in `Impl`). Defaults keep the interface backward-compatible (a backend that forgot to override just returns empty → picker falls back to OS default, no crash).

- **Why**: this is the same pattern already used for encoding state (processors call `SetXXX` in `OpenIn`; the host reads `GetXXX` at dispatch time, e.g. `gs_Views[hWnd]`/`gs_Views[(void*)container]` lookups). The save handlers are free functions that already do exactly those map lookups, so no threading of a new parameter through `NavigateToString`/`Navigate` plumbing is needed.
- **Alternative rejected**: passing the directory inside the `CMD_SAVE` message. The rendered document does not reliably know its own physical path (rendered from pre-fetched bytes), and encoding it into the JS→host payload would leak filesystem layout into the renderer. Host-side is strictly simpler.

### Decision 2: `BaseFileProcessor` reports the directory; only loader-based views (incl. EML) are affected

`BaseFileProcessor::OpenIn` calls `webView.SetCurrentFileDirectory(mPath)` once, alongside the existing `SetEncodingOverrideSupported`/`SetEncodingOverrideHtml` calls. EML is a `BaseFileProcessor`, so the save flow gets the directory for free. Other loader processors also set a path, but only EML exposes a save action, so nothing else changes behavior.

- **Why**: `mPath` is authoritative (set by `InitPath` → `GetPhysicalPath`). Its `parent_path()` is the "current folder". Note this is the original EML file's path, never the `%TEMP%` fallback file that `NavigateToString` may create for >2MB documents — the directory is read from the processor, not from the page URL.

### Decision 3: `PickFolder` gains an optional default-directory parameter

Change `Platform.h`:
```
std::wstring PickFolder(const void* parentWindow, const std::wstring& defaultFolder = L"");
```
An empty `defaultFolder` preserves today's behavior exactly.

- **Windows** (`Platform_Win.cpp`): `SHBrowseForFolder` takes no initial folder argument, so set it via the `BFFM_SETSELECTION` message from the `lpfn` callback on `BFFM_INITIALIZED`: `SendMessageW(hwnd, BFFM_SETSELECTIONW, FALSE, (LPARAM)defaultFolder.c_str())`. The wide-path overload avoids pidl construction.
- **Linux** (`Platform_Linux.cpp`): pass `defaultFolder` as the `dir` argument to `QFileDialog::getExistingDirectory`.

The two existing call sites keep working unmodified (they can pass the looked-up directory; see Decision 4). The 2 MB / 1 MB size guards, base64 decode, and write logic are untouched.

### Decision 4: Save handlers pass the current directory as the default

- Windows `HandleSaveAttachment`: read `gs_Views[(void*)hWnd]->GetCurrentFileDirectory()` and pass `dir.parent_path()` (with a trailing separator normalization compatible with the existing folder format) into `PickFolder(hWnd, dir)`.
- Linux `HandleLinuxSave`: read `gs_Views[(void*)info.container]->GetCurrentFileDirectory()` and pass its `parent_path()` into `PickFolder(info.container, dir)`.

Both handlers already perform the identical `gs_Views` lookup for encoding commands, so this adds no new lookups or locking concerns. If the lookup misses or returns empty (defensive), the empty default degrades to today's OS-default behavior.

## Risks / Trade-offs

- [Windows `SHBrowseForFolder` initial folder] → `BFFM_SETSELECTIONW` with a path string is the documented mechanism; if the path is invalid the dialog ignores it and opens at the OS default. No regression risk.
- [Directory read returns empty (backend not overridden)] → Degrades to current behavior because `PickFolder` treats an empty default as "no default". Purely additive.
- [Behavior change for other loader processors] → Only EML has a save action today; other `BaseFileProcessor` views set a directory but never consume it. No visible change.
- [Modal dialog still blocks its thread] → Unchanged from the existing save flow (saving is an explicit user action).

## Migration Plan

Single change. The `IWebView` additions are additive with defaults; the processor call is one line; both native pickers and both save handlers are small diffs. No config keys, no vcpkg change, no loader/JS change. Rollback = revert; callers that pass no default keep compiling and behave as before.
