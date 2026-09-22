## ADDED Requirements

### Requirement: CHM narrow-path staging

The native CHM archive decoder opens archives through a narrow (system-code-page)
path API. When the CHM file's resolved path is representable in the system code
page, the plugin SHALL open the archive in place. When the path is not
representable, the plugin SHALL stage a tracked temporary copy of the CHM (using
the plugin's existing temp-file generation and tracking) and open the archive
from that copy, so the view still works. The staged copy SHALL be subject to the
plugin's existing temp-file cleanup rules (including `[WebView] CleanupOnExit`).
Opening and serving a CHM SHALL NOT extract the archive's contents to temporary
files — only this narrow-path staging copy is created, and only when needed.
This SHALL behave identically on 32-bit and 64-bit builds.

#### Scenario: Representable path opened in place

- **WHEN** a CHM at a path representable in the system code page is opened
- **THEN** the plugin opens the archive directly from that path and creates no
  temporary copy

#### Scenario: Non-representable path staged to temp

- **WHEN** a CHM whose file name cannot be represented in the system code page is
  opened
- **THEN** the plugin creates a tracked temporary copy, opens the archive from
  it, and records it for cleanup under the existing rules

#### Scenario: Archive contents are not extracted

- **WHEN** a CHM is opened and its topics and images are viewed
- **THEN** no per-entry temporary files are created (entries are read from the
  archive on demand); only a narrow-path staging copy may exist

#### Scenario: Staged copy cleaned up when configured

- **WHEN** `[WebView] CleanupOnExit=1` and the plugin is unloaded after staging a
  CHM copy
- **THEN** the staged copy is removed together with the plugin's other tracked
  temp files
