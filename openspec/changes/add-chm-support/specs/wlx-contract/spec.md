## MODIFIED Requirements

### Requirement: Detect string generation

When Total Commander queries the plugin's detect string, the plugin MUST build a Total Commander detect expression of the form `EXT="ext1"|EXT="ext2"|...` from the `[Extensions]` section of `edgeviewer.ini`. The extension lists SHALL be read in the fixed type-section order `HTML`, `Markdown`, `AsciiDoc`, `URL`, `MHTML`, `CHM`, `EML`, `RST`, `Images`, `Other` (hardcoded in `EdgeViewer/WlxDetect.cpp`), and within each section the extensions SHALL appear in the order written in the ini. When `[Extensions] Dirs=1` is set the plugin MUST append a trailing empty-extension term (`EXT=""`) so directory paths match the detect string; when `Dirs=0` or absent no trailing term is produced. The finished string MUST be copied into the caller-supplied buffer truncated to its maximum length, and the result SHALL be byte-identical on 32-bit and 64-bit builds.

#### Scenario: Detect string reflects all configured extensions

- **WHEN** `edgeviewer.ini` has `[Extensions]` with `HTML=HTM,HTML,XHTML,XML`, `Images=PNG,GIF`, `Other=PDF`, and `Dirs=0`
- **THEN** Total Commander receives `EXT="HTM"|EXT="HTML"|EXT="XHTML"|EXT="XML"|...|EXT="PNG"|EXT="GIF"|...|EXT="PDF"` with no trailing empty-extension term

#### Scenario: Directories match when Dirs=1

- **WHEN** `[Extensions] Dirs=1` is set
- **THEN** the detect string ends with `|EXT=""` so that directory paths are claimed by the plugin

#### Scenario: Type-section order is fixed regardless of ini order

- **WHEN** the `[Extensions]` section lists `Other=PDF` before `HTML=...` in the file
- **THEN** the detect string still emits the `HTML` extensions before the `Other` extensions, because the section iteration order is hardcoded

#### Scenario: CHM term is emitted in the fixed order

- **WHEN** `[Extensions] CHM=CHM` is present and Total Commander queries the detect string
- **THEN** the `EXT="CHM"` term is emitted between the `MHTML` and `EML` extension terms, and the result is byte-identical on the 32-bit and 64-bit builds
