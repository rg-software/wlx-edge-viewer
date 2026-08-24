## MODIFIED Requirements

### Requirement: File loading

When Total Commander asks the plugin to display a file, the plugin MUST locate the processor that claims the file's extension (selected from the `[Extensions]` section of `edgeviewer.ini` by the processor registry in `EdgeViewer/Processors/`); if no processor claims the file it MUST refuse the load and indicate failure by returning `NULL`. On a successful match the plugin MUST read the `lcp_darkmode` bit (`128`) from the `ShowFlags` argument and record it as the current dark-mode state, create a child window inside the parent window Total Commander supplied, create a WebView2 environment bound to that child window, and return the child window handle to Total Commander. If WebView2 environment creation fails the plugin MUST destroy the child window, and — only when `[WebView] ShowErrorBoxes=1` is set — display a Windows error message box describing the failure, then return `NULL`.

#### Scenario: Opened file is claimed by a processor

- **WHEN** Total Commander opens `readme.md` (whose `MD` extension is listed under `[Extensions] Markdown`) with `lcp_darkmode` set in `ShowFlags`
- **THEN** a child window is created inside Total Commander's lister parent, the Markdown processor is selected, dark mode is recorded as active, and the child window handle is returned to Total Commander

#### Scenario: Opened file matches no processor

- **WHEN** Total Commander opens a file whose extension is not listed under any `[Extensions]` type section
- **THEN** no child window is created and the load returns `NULL`

#### Scenario: WebView2 cannot be created with error boxes enabled

- **WHEN** WebView2 environment creation fails and `[WebView] ShowErrorBoxes=1` is set in `edgeviewer.ini`
- **THEN** a Windows error message box is shown, the child window is destroyed, and `NULL` is returned to Total Commander

#### Scenario: WebView2 cannot be created with error boxes disabled

- **WHEN** WebView2 environment creation fails and `[WebView] ShowErrorBoxes` is unset or `0`
- **THEN** no message box is shown, the child window is destroyed, and `NULL` is returned to Total Commander
