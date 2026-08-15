# mhtml Specification

## Purpose
Render MHTML archive files (single-file web archives combining HTML and
embedded resources into one document) in the Total Commander Lister pane,
delegating parsing to the bundled mhtml2html library loaded from static
assets.
## Requirements
### Requirement: MHTML file detection

The plugin MUST detect files with an `.mht` or `.mhtml` extension as
MHTML archives, as declared by the `[Extensions]` section token
`MHTML=MHT,MHTML` of edgeviewer.ini. Extension matching SHALL be
case-insensitive and MUST behave identically on the 32-bit and 64-bit
builds of the plugin. The MHTML processor's path initializer picks the
file when its extension matches the configured MHTML token; any other
extension is left for a different processor. The affected processor lives
under EdgeViewer/Processors/ and serves only this file type.

#### Scenario: Detection accepts .mht and .mhtml case-insensitively

- **WHEN** a file named `ARCHIVE.MHT` or `report.Mhtml` is opened in
  the Lister and the `[Extensions]` token `MHTML=MHT,MHTML` is present
  in edgeviewer.ini
- **THEN** the MHTML processor MUST take ownership of the file regardless
  of letter case and MUST produce identical ownership decisions on the
  32-bit and 64-bit builds

#### Scenario: Detection rejects unconfigured extensions

- **WHEN** a `.mhtml` file is opened but the `[Extensions]` section does
  not map the `MHTML` token, or the file has an unrelated extension such
  as `.html`
- **THEN** the MHTML processor MUST NOT claim the file and another
  processor (or none) handles it

### Requirement: MHTML rendering via the loader template

To render an MHTML archive, the plugin MUST load the static loader
template located at `Resources/assets/mhtml/loader.html`, replace its
`__MHTML_FILENAME__` placeholder with a file-relative-path URL of the
archive, and hand the filled HTML to the WebView2 engine via
`NavigateToString`. The MHTML archive itself MUST be parsed and its
embedded parts (HTML, images, stylesheets) reconstructed for display by
the static mhtml2html library bundled under `Resources/assets/mhtml/`.
The loader template SHALL bootstrap mhtml2html so it can locate the
archive through the injected filename. This rendering behavior SHALL be
identical on the 32-bit and 64-bit builds.

#### Scenario: Loading an MHTML archive

- **WHEN** an `.mht` or `.mhtml` archive is opened in the Lister
- **THEN** the MHTML processor MUST load `Resources/assets/mhtml/loader.html`,
  substitute `__MHTML_FILENAME__` with the archive's path, and render the
  result with `NavigateToString`, letting the bundled mhtml2html library
  decode and display the archived content

#### Scenario: Embedded resources inside the archive

- **WHEN** an MHTML archive contains inline images, stylesheets, or
  scripts packaged according to the MHTML format
- **THEN** the mhtml2html library MUST reconstruct those embedded parts
  for display inside the Lister without leaving the web view to fetch
  external URLs

### Requirement: MHTML virtual host mapping

The MHTML processor MUST register the `local.example` virtual host so the
web view maps it onto the root directory of the file being viewed. This
mapping SHALL allow any relative references resolved during rendering to
be satisfied against the archive's own location. Because rendering uses
`NavigateToString` on the loader template, the virtual host mapping is
established as a supporting step rather than the primary load path. The
mapping behavior MUST be identical on the 32-bit and 64-bit builds.

#### Scenario: Local example host points to the archive's root directory

- **WHEN** an `.mhtml` file located at `D:\docs\portfolios\report.mhtml`
  is opened in the Lister
- **THEN** the MHTML processor MUST map `local.example` to
  `D:\docs\portfolios\` so relative or sibling-file references inside the
  archive resolve against that directory inside the web view

#### Scenario: Assets host points to the plugin directory

- **WHEN** the loader template references renderer assets such as the
  mhtml2html library
- **THEN** the `assets.example` virtual host MUST resolve to the
  plugin's installed directory under `Resources/assets/mhtml/` on both
  the 32-bit and 64-bit builds

