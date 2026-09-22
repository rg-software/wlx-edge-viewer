## ADDED Requirements

### Requirement: Archive-backed content source

In addition to the two folder mappings, the plugin SHALL support a third
content source: an **archive-backed source** that serves entries read from an
open CHM archive through the same host-side custom-scheme mechanism the plugin
already uses for folder-mapped content (the `evh://` scheme on Windows, the
`ev://` scheme on Linux). A processor that opens an archive SHALL register its
archive under a process-unique instance token, and the served URLs SHALL carry
that token so the source resolves each request against the correct open archive.
The source SHALL answer text and binary entries with a content type derived from
the entry path, and SHALL return a not-found result for entries that do not
exist. The archive source SHALL be released when its lister view is closed, and
its behavior SHALL be identical on 32-bit and 64-bit builds and on both
platforms.

#### Scenario: Archive entry served through the custom scheme

- **WHEN** the CHM processor has opened an archive and the renderer requests an
  entry URL carrying that archive's instance token
- **THEN** the host-side scheme handler reads the named entry from the open
  archive and returns its bytes with a content type derived from the entry path

#### Scenario: Unknown entry returns not found

- **WHEN** the renderer requests an entry path that does not exist in the open
  archive
- **THEN** the source returns a not-found result instead of serving unrelated
  content or failing the whole view

#### Scenario: Archive released with the view

- **WHEN** the lister view showing a CHM is closed
- **THEN** the archive source registered under that view's instance token is
  released, so a later view cannot resolve against the closed archive

#### Scenario: Folder mappings are unaffected

- **WHEN** a non-CHM processor establishes the `assets.example` and
  `local.example` folder mappings
- **THEN** those mappings continue to resolve to their folders exactly as
  before, and the archive source does not intercept them
