# Spec Delta

## MODIFIED Requirements

### Requirement: Config file location and format

The plugin MUST read its configuration from a file named `edgeviewer.ini` located in the same directory as the loaded plugin DLL, using the mINI INI parser. The file MUST use standard INI syntax (section headers in `[brackets]`, `Key=Value` lines, `;` line comments) and is consumed as plain ASCII/UTF-8 (values are converted to UTF-16 for internal use). The shipped template lives at `Resources/edgeviewer.ini` and is deployed next to the DLL by the release packaging as `edgeviewer.ini.sample`. The plugin MUST NOT modify an existing `edgeviewer.ini` at runtime and MUST NOT overwrite it on a plugin update; it MAY create one from `edgeviewer.ini.sample` when no `edgeviewer.ini` exists, and that creation MUST be best-effort — a failure to create the file MUST NOT prevent the plugin from starting or from resolving its configuration from another source.

#### Scenario: Ini located next to the DLL

- **WHEN** the plugin DLL is installed at `C:\TC\Plugins\EdgeViewer\EdgeViewer.wlx`
- **THEN** the plugin reads its configuration from `C:\TC\Plugins\EdgeViewer\edgeviewer.ini`

#### Scenario: Ini file is absent

- **WHEN** the plugin directory contains no `edgeviewer.ini`
- **THEN** the configuration is resolved from `edgeviewer.ini.sample` when that file is present, and from no source at all when it is not

#### Scenario: Ini file is absent and no sample is present

- **WHEN** the plugin directory contains neither `edgeviewer.ini` nor `edgeviewer.ini.sample`
- **THEN** the parsed configuration is empty (type sections, extensions, and WebView keys are all absent), and downstream behavior degrades to the code-level defaults for each missing key

### Requirement: Config lazy loading and caching

The plugin MUST resolve and parse its configuration exactly once, on the first access to the configuration, and MUST cache the parsed result in a process-lifetime static structure so that subsequent accesses read the cached parse without re-reading any file. The cache SHALL be populated no later than the first call to any lister entry point that needs configuration (detect-string generation, file loading, or DLL detach), and no configuration file SHALL be re-read even if it changes on disk while the plugin is loaded.

#### Scenario: First access parses the file

- **WHEN** Total Commander calls `ListGetDetectString` (the first configuration-touching entry point) before any other access
- **THEN** the resolved configuration is parsed once and the extensions table is available to build the detect string

#### Scenario: Edits after load are not observed

- **WHEN** the configuration file is edited while the plugin is loaded in Total Commander
- **THEN** the running plugin keeps using the cached parse until the plugin is reloaded

## ADDED Requirements

### Requirement: Config source resolution order

The plugin MUST resolve its configuration from the first source that yields a usable configuration, in this order:

1. `edgeviewer.ini` in the plugin DLL's directory — the user's own file.
2. `edgeviewer.ini.sample` in the plugin DLL's directory — the shipped template.
3. No configuration.

A file counts as a usable source when it parses into at least one section. A file that is absent, unreadable, or parses into no sections (an empty file, or one that contains only comments) is skipped and the next source is tried. The plugin MUST NOT merge sources: a usable `edgeviewer.ini` replaces the sample wholesale, and the sample is never used to supply individual missing keys. The resolution MUST be identical on 32-bit and 64-bit builds and on Windows and Linux.

#### Scenario: Read-only plugin directory without an ini

- **WHEN** the plugin directory contains `edgeviewer.ini.sample` but no `edgeviewer.ini`, and the directory cannot be written to
- **THEN** the plugin resolves its configuration from `edgeviewer.ini.sample` and claims the file types listed in the sample's `[Extensions]` section, with no error and no missing file types

#### Scenario: Writable plugin directory without an ini

- **WHEN** the plugin directory contains `edgeviewer.ini.sample` but no `edgeviewer.ini`, and the directory is writable
- **THEN** the plugin creates `edgeviewer.ini` from the sample and resolves its configuration from `edgeviewer.ini`

#### Scenario: Existing ini wins over the sample

- **WHEN** the plugin directory contains both `edgeviewer.ini` and `edgeviewer.ini.sample`
- **THEN** the configuration is read from `edgeviewer.ini` and the sample is ignored, including for keys the user's ini does not define

#### Scenario: Ini without sections is treated as absent

- **WHEN** `edgeviewer.ini` exists but contains no section (an empty file, or comments only)
- **THEN** the configuration is resolved from `edgeviewer.ini.sample`

#### Scenario: Neither file present

- **WHEN** the plugin directory contains neither `edgeviewer.ini` nor `edgeviewer.ini.sample`
- **THEN** the plugin starts with an empty configuration and claims no file types

### Requirement: Sample-derived configuration keeps the template's UserDir

When the configuration is resolved from `edgeviewer.ini.sample`, the plugin MUST behave exactly as it would with an `edgeviewer.ini` copied from that sample, including the `[WebView] UserDir` value the sample specifies. The shipped sample sets `UserDir=%USERPROFILE%`, so a read-only plugin installation MUST NOT default the web-engine user-data directory to the (unwritable) plugin directory.

#### Scenario: Web-engine profile is not created inside a read-only plugin directory

- **WHEN** the configuration is resolved from the shipped `edgeviewer.ini.sample`
- **THEN** `[WebView] UserDir` resolves to the value the sample specifies (the user's profile directory by default), so the web-engine profile is created outside the plugin directory
