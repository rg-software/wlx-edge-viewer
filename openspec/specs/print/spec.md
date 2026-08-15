# print Specification

## Purpose
Characterizes the existing print capability of the Total Commander WLX Lister plugin: how a print request from Total Commander is dispatched into the rendered WebView2 document, how the plugin deliberately ignores Total Commander's printer and margin parameters in favor of the browser's own print dialog, and how the legacy ANSI entry point wraps the Unicode entry point. The behavior is identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin.
## Requirements
### Requirement: Print dispatch chain

When Total Commander asks the lister to print the currently rendered document, the plugin SHALL relay the request to its lister window and that window SHALL invoke the browser's print dialog. The Unicode print entry point SHALL send a print command message to the lister window with an empty data payload. The lister window SHALL ask the navigator to print, and the navigator SHALL run a script that calls `window.print()`. The print entry point SHALL return a success indicator to Total Commander.

#### Scenario: TC triggers a print

- **WHEN** Total Commander invokes the Unicode print entry point for the currently rendered document
- **THEN** the plugin sends a print command to its lister window, which runs `window.print()` and so opens the browser's print dialog

#### Scenario: print returns success to TC

- **WHEN** a print request has been dispatched and the `window.print()` script has been invoked
- **THEN** the Unicode print entry point returns a success indicator to Total Commander, regardless of whether the user actually confirmed or cancelled the browser's print dialog

### Requirement: TC print parameters are ignored

The Unicode print entry point SHALL NOT use any of the printer-related parameters it receives from Total Commander (the file to print, the default printer, the print flags and the page margins). The plugin SHALL NOT select a printer, SHALL NOT apply print flags, and SHALL NOT set page margins. The plugin SHALL only invoke the browser's own print dialog, which in turn lets the user choose the printer, the page size, the orientation and the margins.

#### Scenario: printer selection is left to the browser dialog

- **WHEN** Total Commander invokes the print entry point with a default printer name supplied
- **THEN** the plugin does not pass that name anywhere; the user selects the printer from the browser's print dialog when it opens

#### Scenario: print flags are not applied

- **WHEN** Total Commander invokes the print entry point with print flags supplied
- **THEN** the plugin does not interpret the flags; only the user's choices in the browser's print dialog affect the output

#### Scenario: margins are not applied

- **WHEN** Total Commander invokes the print entry point with page margins supplied
- **THEN** the plugin does not pass the margins to the browser; the user chooses the margins from the browser's print dialog

### Requirement: ANSI wrapper parity

The plugin SHALL export an ANSI print entry point in addition to the Unicode print entry point. The ANSI entry point SHALL convert the ANSI path arguments to UTF-16 and SHALL delegate to the Unicode entry point so that both entry points exercise the same dispatch chain and the same `window.print()` invocation. No separate ANSI print path SHALL exist.

#### Scenario: ANSI print path

- **WHEN** Total Commander invokes the ANSI print entry point with ANSI path arguments
- **THEN** the plugin converts the paths to UTF-16 and runs the same dispatch chain as the Unicode entry point, ultimately invoking `window.print()`

#### Scenario: Unicode print path

- **WHEN** Total Commander invokes the Unicode print entry point directly
- **THEN** the plugin uses the UTF-16 path arguments as-is and runs the same dispatch chain that the ANSI entry point delegates to

### Requirement: 32-bit and 64-bit print parity

The print dispatch chain, the deliberate ignoring of Total Commander's printer and margin parameters, the ANSI-to-Unicode delegation and the success return value SHALL be identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin. Both builds SHALL export both the ANSI and the Unicode print entry points, and both builds SHALL open the same browser print dialog for the same rendered document.

#### Scenario: Win32 build print

- **WHEN** the 32-bit plugin is loaded and Total Commander invokes the print entry point
- **THEN** the plugin opens the browser's print dialog via `window.print()` and ignores the printer and margin parameters, matching the 64-bit build's behavior

#### Scenario: x64 build print

- **WHEN** the 64-bit plugin is loaded and Total Commander invokes the print entry point
- **THEN** the plugin opens the browser's print dialog via `window.print()` and ignores the printer and margin parameters, matching the 32-bit build's behavior

