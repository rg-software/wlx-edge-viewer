## Purpose

Characterizes the existing dark-mode capability of the Total Commander WLX Lister plugin: how the dark mode flag is derived from the host, how each processor selects its styling, and how the underlying web engine is told about the preferred color scheme. The plugin renders Markdown, AsciiDoc, RST, HTML, MHT, images, directories and PDF via WebView2, and most processors ship a pair of CSS overrides keyed off a single global dark-mode flag. This spec documents the observable behavior so future changes preserve the existing parity across the 32-bit (Win32) and 64-bit (x64) builds.

## ADDED Requirements

### Requirement: Dark mode flag source

The plugin SHALL derive its dark mode flag from the host's show flags. When Total Commander calls the lister with `lcp_darkmode` (bit value 128) set in the show flags, the plugin SHALL record that dark mode is active in a single global flag. The flag SHALL be consulted by every processor to pick between two sets of style overrides that live in the per-type sections of `edgeviewer.ini` (for example the `[Markdown]`, `[RST]`, `[EML]`, `[Images]` and `[Directory]` sections, and the `[HTML]` section used by the HTML/MHT processor and the AddApplyStyleScript mechanism). Because the flag SHALL be updated on every load and next-file call, the same plugin instance MAY switch modes between consecutive files without being reloaded.

#### Scenario: host reports dark mode on initial load

- **WHEN** Total Commander invokes the lister's load entry point with `lcp_darkmode` (128) present in the show flags
- **THEN** the plugin's global dark mode flag is set to true before any rendering begins, and processors will use their CSSDark overrides for the rendered document

#### Scenario: host reports light mode on initial load

- **WHEN** Total Commander invokes the lister's load entry point with `lcp_darkmode` (128) absent from the show flags
- **THEN** the plugin's global dark mode flag is set to false and processors will use their CSS overrides for the rendered document

#### Scenario: flag carryover across files

- **WHEN** the user navigates from one file to the next through the lister's next-file entry point and the new call still carries `lcp_darkmode` (128)
- **THEN** the plugin's global dark mode flag is re-evaluated from the new show flags and remains true, so dark styling persists across consecutive files

### Requirement: CSS selection per processor

Each processor that renders structured content SHALL read style overrides from its own section of `edgeviewer.ini`. When dark mode is active, processors that declare a `CSSDark` key SHALL switch to that override; processors that only declare `CSS` SHALL keep their styling unchanged. The Markdown, RST, EML, Images, Directory and HTML processors SHALL support the `CSS`/`CSSDark` pair (the HTML processor applies its styles through an injected script rather than through a loader placeholder, but the observable effect is the same: the style sheet swap happens when the dark mode flag is set). The AsciiDoc processor only declares a `CSS` key and has no `CSSDark` entry, so toggling dark mode SHALL NOT change the AsciiDoc document styling.

#### Scenario: Markdown document in dark mode

- **WHEN** a Markdown file is loaded and the global dark mode flag is true
- **THEN** the Markdown processor reads the `CSSDark` value from the `[Markdown]` section of `edgeviewer.ini` and uses it to style the rendered HTML

#### Scenario: AsciiDoc document in dark mode

- **WHEN** an AsciiDoc file is loaded and the global dark mode flag is true
- **THEN** the AsciiDoc processor uses its `CSS` value from the `[AsciiDoc]` section because no `CSSDark` key is defined, so AsciiDoc styling is identical in light and dark mode

#### Scenario: HTML document in dark mode

- **WHEN** an HTML or MHT file is loaded and the global dark mode flag is true
- **THEN** the HTML processor injects a style block using the `CSSDark` value from the `[HTML]` section, applied after the document has been loaded so it overrides the document's own inline styles

#### Scenario: directory listing in dark mode

- **WHEN** a directory is loaded and the global dark mode flag is true
- **THEN** the Directory processor uses the `CSSDark` value from the `[Directory]` section to render the file listing with dark-themed colors

### Requirement: Web engine color scheme

In addition to swapping the document's CSS, the plugin SHALL tell the underlying WebView2 engine about the preferred color scheme. The engine's profile SHALL be set to a DARK or LIGHT preferred color scheme matching the plugin's global dark mode flag. This SHALL affect engine defaults that are not covered by the injected style sheet, such as the default background used before the document is painted, form controls and scrollbars that follow the `prefers-color-scheme` media query, and the default canvas color.

#### Scenario: dark mode sets engine profile to dark

- **WHEN** the global dark mode flag is true and a new WebView2 control is created for rendering
- **THEN** the engine's preferred color scheme is set to DARK, so engine-default UI matches the dark document styling

#### Scenario: light mode sets engine profile to light

- **WHEN** the global dark mode flag is false and a new WebView2 control is created for rendering
- **THEN** the engine's preferred color scheme is set to LIGHT, so engine-default UI matches the light document styling

### Requirement: Dark mode is global and per-file

There SHALL be a single global dark mode flag for the plugin instance. It SHALL be read at the start of every load and next-file operation, so the mode tracked by the plugin always matches the most recent show flags reported by Total Commander. When two consecutive calls report different show flags, the plugin SHALL switch mode between the two files; the previously rendered file SHALL NOT be retroactively re-styled.

#### Scenario: switching from dark to light between two files

- **WHEN** file A is rendered with `lcp_darkmode` set and the user then navigates to file B which is opened with `lcp_darkmode` absent
- **THEN** the global dark mode flag is set to false while loading file B and file B is rendered with light styling, while file A keeps its dark styling

#### Scenario: switching from light to dark between two files

- **WHEN** file A is rendered without `lcp_darkmode` and the user then navigates to file B which is opened with `lcp_darkmode` set
- **THEN** the global dark mode flag is set to true while loading file B and file B is rendered with dark styling, while file A keeps its light styling

### Requirement: 32-bit and 64-bit dark mode parity

The dark mode flag, the per-processor CSS selection rules and the WebView2 preferred color scheme behavior SHALL be identical between the 32-bit (Win32) and 64-bit (x64) builds of the plugin. Both builds SHALL read `edgeviewer.ini`, both builds SHALL use the same `[<Processor>]` sections, and both builds SHALL apply the same `CSSDark`/`CSS` selection rules. A user running the Win32 build under a 32-bit Total Commander and a user running the x64 build under a 64-bit Total Commander SHALL see dark mode activate on the same set of file types and with the same styling.

#### Scenario: Win32 build dark mode

- **WHEN** the 32-bit plugin is loaded in a 32-bit Total Commander and `lcp_darkmode` is set
- **THEN** the active processors read their `CSSDark` overrides from `edgeviewer.ini` and the engine profile is set to DARK, matching the 64-bit build's behavior

#### Scenario: x64 build dark mode

- **WHEN** the 64-bit plugin is loaded in a 64-bit Total Commander and `lcp_darkmode` is set
- **THEN** the active processors read their `CSSDark` overrides from `edgeviewer.ini` and the engine profile is set to DARK, matching the 32-bit build's behavior