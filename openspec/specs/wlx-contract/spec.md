# wlx-contract Specification

## Purpose
Exposes the Total Commander lister plugin (WLX) entry points from `EdgeViewer/DllMain.cpp` and the export table in `EdgeViewer/EdgeViewer.def`, defining how Total Commander loads files, navigates between them, closes views, searches, prints, and resolves which extensions the plugin claims — and how the 32-bit and 64-bit builds stay contract-compatible.
## Requirements
### Requirement: WLX export set

The plugin MUST export the complete set of Total Commander lister entry points declared in `EdgeViewer/EdgeViewer.def`: file loading (`ListLoadW`, `ListLoad`), in-place navigation (`ListLoadNextW`, `ListLoadNext`), view lifecycle (`ListCloseWindow`), extension detection (`ListGetDetectString`), in-view search (`ListSearchTextW`, `ListSearchText`), printing (`ListPrintW`, `ListPrint`), and host integration (`ListSendCommand`, `ListSetDefaultParams`). Both the wide (`W`) and ANSI variants of the load/navigate/search/print entry points MUST be present so Total Commander can call whichever variant its configuration selects. The set of exported symbols SHALL be identical on 32-bit and 64-bit builds.

#### Scenario: Total Commander resolves an exported entry point by name

- **WHEN** Total Commander calls `GetProcAddress` for `ListLoadW` on the loaded plugin DLL
- **THEN** the symbol resolves to the plugin's implementation in `EdgeViewer/DllMain.cpp`

#### Scenario: Both ANSI and wide variants are available

- **WHEN** Total Commander is configured to use the ANSI lister interface and resolves `ListLoad`
- **THEN** the plugin returns a valid function pointer, because the ANSI variant is exported alongside `ListLoadW`

### Requirement: File loading

When Total Commander asks the plugin to display a file, the plugin MUST locate the processor that claims the file's extension (selected from the `[Extensions]` section of `edgeviewer.ini` by the processor registry in `EdgeViewer/Processors/`); if no processor claims the file it MUST refuse the load and indicate failure by returning `NULL`. On a successful match the plugin MUST read the `lcp_darkmode` bit (`128`) from the `ShowFlags` argument and record it as the current dark-mode state, create a child window inside the parent window Total Commander supplied, create a WebView2 environment bound to that child window, and return the child window handle to Total Commander. If WebView2 environment creation fails the plugin MUST destroy the child window, and — only when `[Chromium] ShowErrorBoxes=1` is set — display a Windows error message box describing the failure, then return `NULL`.

#### Scenario: Opened file is claimed by a processor

- **WHEN** Total Commander opens `readme.md` (whose `MD` extension is listed under `[Extensions] Markdown`) with `lcp_darkmode` set in `ShowFlags`
- **THEN** a child window is created inside Total Commander's lister parent, the Markdown processor is selected, dark mode is recorded as active, and the child window handle is returned to Total Commander

#### Scenario: Opened file matches no processor

- **WHEN** Total Commander opens a file whose extension is not listed under any `[Extensions]` type section
- **THEN** no child window is created and the load returns `NULL`

#### Scenario: WebView2 cannot be created with error boxes enabled

- **WHEN** WebView2 environment creation fails and `[Chromium] ShowErrorBoxes=1` is set in `edgeviewer.ini`
- **THEN** a Windows error message box is shown, the child window is destroyed, and `NULL` is returned to Total Commander

#### Scenario: WebView2 cannot be created with error boxes disabled

- **WHEN** WebView2 environment creation fails and `[Chromium] ShowErrorBoxes` is unset or `0`
- **THEN** no message box is shown, the child window is destroyed, and `NULL` is returned to Total Commander

### Requirement: File navigation

When Total Commander loads a new file into an already-open lister view, the plugin MUST first confirm that some processor claims the new file's extension; if none does it MUST refuse the navigation by returning `LISTPLUGIN_ERROR`. When a processor claims the file the plugin MUST update the dark-mode state from the `lcp_darkmode` bit of the supplied `ShowFlags`, then dispatch the new file path to the existing lister window so the current processor re-renders the new content, and return `LISTPLUGIN_OK`.

#### Scenario: Navigate to a supported file in an open view

- **WHEN** Total Commander loads `notes.md` into a lister window already showing another Markdown file
- **THEN** the dark-mode flag is refreshed from `ShowFlags`, the new path is delivered to the existing view, and the lister returns `LISTPLUGIN_OK`

#### Scenario: Navigate to an unsupported file

- **WHEN** Total Commander loads a file whose extension no `[Extensions]` type section claims into an existing view
- **THEN** the navigation is refused with `LISTPLUGIN_ERROR` and the existing view is unaffected

### Requirement: Window closing

When Total Commander closes a lister view, the plugin MUST release the resources bound to that view (closing the WebView2 controller associated with the window handle), remove the view from its internal view table, then post a `WM_CLOSE` message to the window so the child window terminates. Closing an unknown handle SHALL leave the view table untouched but MUST still post `WM_CLOSE` to the supplied handle.

#### Scenario: Closing a known view

- **WHEN** Total Commander closes a lister window that the plugin has an open view for
- **THEN** the WebView2 controller for that window is closed, the view is forgotten by the plugin, and `WM_CLOSE` is posted to the window

#### Scenario: Closing an unknown handle

- **WHEN** Total Commander closes a window handle the plugin has no view for
- **THEN** no view cleanup happens but `WM_CLOSE` is still posted to the handle

### Requirement: Detect string generation

When Total Commander queries the plugin's detect string, the plugin MUST build a Total Commander detect expression of the form `EXT="ext1"|EXT="ext2"|...` from the `[Extensions]` section of `edgeviewer.ini`. The extension lists SHALL be read in the fixed type-section order `HTML`, `Markdown`, `AsciiDoc`, `URL`, `MHTML`, `EML`, `RST`, `Images`, `Other` (hardcoded in `EdgeViewer/DllMain.cpp`), and within each section the extensions SHALL appear in the order written in the ini. When `[Extensions] Dirs=1` is set the plugin MUST append a trailing empty-extension term (`EXT=""`) so directory paths match the detect string; when `Dirs=0` or absent no trailing term is produced. The finished string MUST be copied into the caller-supplied buffer truncated to its maximum length, and the result SHALL be byte-identical on 32-bit and 64-bit builds.

#### Scenario: Detect string reflects all configured extensions

- **WHEN** `edgeviewer.ini` has `[Extensions]` with `HTML=HTM,HTML,XHTML,XML`, `Images=PNG,GIF`, `Other=PDF`, and `Dirs=0`
- **THEN** Total Commander receives `EXT="HTM"|EXT="HTML"|EXT="XHTML"|EXT="XML"|...|EXT="PNG"|EXT="GIF"|...|EXT="PDF"` with no trailing empty-extension term

#### Scenario: Directories match when Dirs=1

- **WHEN** `[Extensions] Dirs=1` is set
- **THEN** the detect string ends with `|EXT=""` so that directory paths are claimed by the plugin

#### Scenario: Type-section order is fixed regardless of ini order

- **WHEN** the `[Extensions]` section lists `Other=PDF` before `HTML=...` in the file
- **THEN** the detect string still emits the `HTML` extensions before the `Other` extensions, because the section iteration order is hardcoded

### Requirement: In-view text search

When Total Commander requests a text search in an open view, the plugin MUST format the search parameters as a single wide string of the form `<SearchParameter> <SearchString>` (the numeric parameter followed by a space and the search text) and dispatch it to the lister window, then return `LISTPLUGIN_OK`. The dispatched message is handled by the lister's search handler, which drives the web view's find behavior using the parameter bits for case sensitivity, whole-word, backwards, and find-first options.

#### Scenario: Searching for text forwards

- **WHEN** Total Commander searches for `example` with a forward-search parameter
- **THEN** the parameter and text are delivered to the lister window as a single `<param> example` string and the view searches the rendered content

#### Scenario: Case-sensitive search

- **WHEN** Total Commander searches with the case-sensitive parameter bit set
- **THEN** the dispatched parameter preserves that bit and the view's find operation is case-sensitive

### Requirement: Printing

When Total Commander requests printing of the current view, the plugin MUST dispatch a print command to the lister window with an empty data payload and return `LISTPLUGIN_OK`. The lister's print handler SHALL invoke the web view's print path on the currently rendered document. The plugin MUST ignore the `FileToPrint`, `DefPrinter`, `PrintFlags`, and `Margins` arguments passed by Total Commander and print only the content currently loaded in the view.

#### Scenario: Printing the current view

- **WHEN** Total Commander calls the print entry point on an open view
- **THEN** a print command with empty data is delivered to the lister window and the web view's print dialog is invoked for the rendered document

### Requirement: ANSI wrapper parity

For each lister entry point that has both a wide (`W`) and an ANSI variant, the ANSI variant MUST convert its incoming character arguments from the system ANSI encoding to UTF-16 and delegate to the wide variant, then return whatever the wide variant returns. This SHALL apply to file loading, in-place navigation, in-view search, and printing (including the `FileToPrint` and `DefPrinter` arguments of the print path). The conversion SHALL produce results identical to calling the wide variant directly with the equivalent UTF-16 input, and SHALL be byte-for-byte equivalent on 32-bit and 64-bit builds.

#### Scenario: Opening a file via the ANSI entry point

- **WHEN** Total Commander calls the ANSI load entry point with an ANSI-encoded path `C:\docs\readme.md`
- **THEN** the path is converted to its UTF-16 equivalent and the wide load entry point renders the file identically to a direct wide call

#### Scenario: Searching via the ANSI entry point

- **WHEN** Total Commander calls the ANSI search entry point with ANSI search text
- **THEN** the text is converted to UTF-16 and the wide search entry point is invoked with the converted text

### Requirement: No-op host integration entry points

The plugin MUST implement the command-sending entry point (`ListSendCommand`) as a no-op that returns `0` for all command and parameter values, and MUST implement the default-parameters entry point (`ListSetDefaultParams`) as an empty no-op that performs no action regardless of the parameter struct supplied (the supplied default-ini path inside the struct is not consumed by this entry point). These behaviors SHALL be identical on 32-bit and 64-bit builds.

#### Scenario: Total Commander sends a custom command

- **WHEN** Total Commander calls `ListSendCommand` with any command and parameter
- **THEN** the plugin returns `0` and performs no action on the view

#### Scenario: Total Commander sets default parameters

- **WHEN** Total Commander calls `ListSetDefaultParams` with its default-parameter struct
- **THEN** the plugin performs no action and consumes none of the struct's fields from this entry point

### Requirement: 32-bit and 64-bit export parity

The plugin MUST ship as two native DLLs from the same source: a 32-bit DLL (deployed as `EdgeViewer.wlx`) and a 64-bit DLL (deployed as `EdgeViewer.wlx64`), both produced from `EdgeViewer/DllMain.cpp` with the same export table in `EdgeViewer/EdgeViewer.def`. The set of exported symbols, their observable behavior, the detect-string output, and the ANSI/wide wrapper semantics SHALL be identical across the two builds. The only sanctioned difference SHALL be the calling convention: 32-bit exports use the `__stdcall` convention required by the 32-bit Total Commander lister ABI, while 64-bit exports use the platform default calling convention.

#### Scenario: Same symbol set on both builds

- **WHEN** the 32-bit and 64-bit plugin DLLs are inspected with a PE export viewer
- **THEN** both expose the same 12 exported symbols named in `EdgeViewer/EdgeViewer.def`

#### Scenario: Same detect string on both builds

- **WHEN** `ListGetDetectString` is called on identical `edgeviewer.ini` contents on both the 32-bit and 64-bit DLLs
- **THEN** the returned detect string is byte-identical

### Requirement: ESC closes the lister on Linux

On Linux, when the user presses the ESC key while the embedded web view
has focus, the plugin SHALL close the lister (destroy the embedded
`QWebEngineView`, erase the `gs_Views` entry) and return focus to the
file panel. The mechanism is an in-page JS `keydown` listener injected
by `QtWebEngineBackend`'s constructor via
`AddScriptToExecuteOnDocumentCreated`; the listener triggers a request
to `ev://_close/<id>` (via `new Image().src = url`, since Chromium's JS
`fetch()` API has a server-side allowlist that rejects custom schemes
even when registered via `QWebEngineUrlScheme::registerScheme`) which
the global `EvSchemeHandler` routes back to the registered container
`QWidget*` and posts a **synthetic `Q` keypress** to the container's
parent widget (DC's viewer panel). DC's hotkey handler processes the
`Q` identically to a physical press, invoking `cm_ExitViewer` which
closes the lister through DC's normal machinery (calling
`ListCloseWindow` on this plugin as it does for any other lister close).

- Windows behavior is unchanged: Total Commander's panel-level keymap
  posts `WM_CLOSE` to the lister HWND when ESC is pressed; the lister's
  `WNDPROC` (`EdgeViewer/EdgeLister_Win.cpp`) falls through to
  `DefWindowProc`.
- The Linux JS bridge is parented to the embedded `QWebEngineView`'s
  `QWebEngineScriptCollection`; the per-instance `<id>` token is
  allocated by `AllocateContainerId()` and unregistered by the
  container's `QObject::destroyed` signal via `UnregisterContainer(id)`.
- The JS listener SHALL ignore events with `e.defaultPrevented` true,
  so loaders that consume ESC themselves (none today) are not affected.
- **Why synthetic Q and not calling ListCloseWindow directly**:
  `hide()`/`close()` on the container from within the ev:// scheme
  handler either exits DC entirely (synchronous) or has no effect
  (deferred via QTimer). `QCloseEvent` posted to the parent widget is
  ignored. The synthetic `Q` follows the exact same dispatch path DC
  uses for its built-in `Q` shortcut, which is known to work reliably.
- **Why a JS bridge and not a `QObject::eventFilter`**: a
  `QObject::eventFilter` installed on `QWebEngineView` does NOT see
  key events — Chromium intercepts keyboard input at a level below
  Qt's event system (confirmed by instrumentation: the filter logged
  38 events of varying types over a session but none were
  `QEvent::KeyPress` (type 6) or `QEvent::KeyRelease` (type 7)).
- **No interference with image fullscreen**: the image viewer's
  fullscreen toggle uses `F` (its own keydown listener), not ESC.

#### Scenario: ESC closes an open lister

- **WHEN** the user opens any file in the lister pane (F3) and presses
  ESC while focus is in the rendered view
- **THEN** the lister pane is dismissed and the file panel regains focus

#### Scenario: ESC in image fullscreen closes the lister

- **WHEN** the user has pressed `F` to enter the image-viewer's
  fullscreen class and then presses ESC
- **THEN** the lister closes (the loader's fullscreen toggle uses
  `F`, so ESC is not consumed by the loader; the bridge dispatches
  the close request)

