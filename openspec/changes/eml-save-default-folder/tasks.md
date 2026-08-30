## 1. `IWebView` surface for the current file directory

- [x] 1.1 Add to `EdgeViewer/IWebView.h`: `virtual void SetCurrentFileDirectory(const std::filesystem::path&) {}` and `virtual std::filesystem::path GetCurrentFileDirectory() const { return {}; }`, documented as the view's directory reported by the processor in `OpenIn` and read back by the host at save time (empty default = no default folder).
- [x] 1.2 Store/return the directory in `EdgeViewer/WebView/WebView2Backend.{h,cpp}` (Windows: add `m_currentFileDir` member; implement both methods).
- [x] 1.3 Store/return the directory in `EdgeViewer/WebView/QtWebEngineBackend.{h,cpp}` (Linux: add a member to `Impl`; implement both methods).

## 2. Processor reports the directory

- [x] 2.1 In `EdgeViewer/Processors/BaseFileProcessor.cpp` `OpenIn`, call `webView.SetCurrentFileDirectory(mPath.parent_path())` (stores the opened file's directory) once, next to the call site for the other per-load resets. This covers EML (and all loader-based processors); only EML exposes a save action, so no other view changes behavior.

## 3. `PickFolder` default directory

- [x] 3.1 Change `EdgeViewer/Platform.h`: `std::wstring PickFolder(const void* parentWindow, const std::wstring& defaultFolder = L"");` (empty default preserves current behavior).
- [x] 3.2 `EdgeViewer/Platform_Win.cpp`: rework `PickFolder` onto the modern `IFileOpenDialog` (`FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM`); when `defaultFolder` is non-empty, resolve it (`ExpandEnv` + `GetFullPathNameW` + `SHCreateItemFromParsingName`) and set it as the initial view via `IFileDialog::SetFolder`. (The initial `SHBrowseForFolder` + `BFFM_SETSELECTIONW` attempt was replaced — it could not reliably preselect the folder.)
- [x] 3.3 `EdgeViewer/Platform_Linux.cpp`: pass `to_utf8(defaultFolder)` as the `dir` argument of `QFileDialog::getExistingDirectory` when non-empty.

## 4. Save handlers pass the default

- [x] 4.1 Windows `HandleSaveAttachment` (`EdgeViewer/WebView/WebViewFactory.cpp`): look up `gs_Views[(void*)hWnd]`, read `GetCurrentFileDirectory()`, and if non-empty pass it as the second argument to `PickFolder`.
- [x] 4.2 Linux `HandleLinuxSave` (`EdgeViewer/WebView/QtWebEngineBackend.cpp`): look up `gs_Views[(void*)info.container]`, read `GetCurrentFileDirectory()`, and if non-empty pass it to `PickFolder(info.container, dir)`.

## 5. Verify: build Release for both Win32 and x64 and load in Total Commander

- [x] 5.1 Build Release for Win32 and x64 (MSVS 2022 Developer Command Prompt, per AGENTS.md); both DLLs build clean.
- [x] 5.2 Both test suites pass on x64 and Win32 (60 test cases / 263 assertions each, including the new `EmProcessor::OpenIn` assertions that the picker default is reported).
- [x] 5.3 Load in Total Commander, open an `.eml` with an attachment, click the attachment, and confirm the folder picker opens pre-selected on the directory containing the `.eml` file (user-verified on Windows).
- [x] 5.4 Confirm the cancel path (folder dialog dismissed) still leaves the view unchanged, and that saving to any other folder still works as before (user-verified on Windows).
- [x] 5.5 On Linux (Double Commander + Qt Web Engine), confirm the picker defaults to the `.eml` file's directory for an attachment save up to 1 MB. (Code path symmetric with Windows; not yet manually verified.) — **user-verified on Linux**: attachments saved into the correct folder.
