# other-fallback Specification

## Purpose
Provide a catch-all renderer in the Total Commander Lister pane for file
types the dedicated processors do not own (notably PDF) by navigating the
WebView2 engine directly to the file through the local virtual host so
the engine's own built-in viewers - such as the browser PDF viewer -
handle the content natively.
## Requirements
### Requirement: Fallback file detection

The plugin MUST detect files whose extension matches the token mapped
under the `[Extensions]` section token `Other` of edgeviewer.ini
(default: `PDF`). Extension matching SHALL be case-insensitive and MUST
produce identical ownership decisions on the 32-bit and 64-bit builds.
The fallback processor lives under EdgeViewer/Processors/ and takes only
extensions explicitly listed under the configured `Other` token; an
extension not listed under any token is not claimed by this processor.

#### Scenario: Detection accepts configured Other extensions

- **WHEN** a file named `REPORT.PDF` or `whitepaper.pdf` is opened in
  the Lister and the `[Extensions]` token `Other=PDF` is present in
  edgeviewer.ini
- **THEN** the fallback processor MUST take ownership of the file
  regardless of letter case and MUST make identical ownership decisions
  on the 32-bit and 64-bit builds

#### Scenario: Detection leaves unconfigured extensions alone

- **WHEN** a file with an extension not listed under the `Other` token,
  or under any other configured token, is opened
- **THEN** the fallback processor MUST NOT claim the file

### Requirement: Fallback rendering via the local virtual host

The fallback processor MUST render a file by navigating the WebView2
engine via `Navigate` to a URL of the form
`http://local.example/<urlPath>` where `<urlPath>` encodes the file's
path. The processor MUST NOT use `NavigateToString` to inline any
template; the file itself is loaded by the web engine through the
`local.example` virtual host. The engine therefore streams the file
directly and is responsible for sniffing and rendering its format. This
behavior MUST be identical on the 32-bit and 64-bit builds.

#### Scenario: Files are loaded through local.example

- **WHEN** a file `D:\docs\report.pdf` is opened in the Lister
- **THEN** the fallback processor MUST call `Navigate` on the web view
  with the `http://local.example/` URL whose path encodes
  `D:\docs\report.pdf`, so the engine loads the file from the host the
  plugin mapped to that path

#### Scenario: NavigateToString is not used for the body

- **WHEN** any file covered by the `Other` token is opened
- **THEN** the fallback processor MUST NOT call `NavigateToString` on
  the file content; the page body MUST be loaded by the web engine
  through the `local.example` virtual host instead

### Requirement: PDF rendering via the browser's built-in PDF viewer

For files whose extension is `PDF`, the WebView2 engine's built-in PDF
viewer MUST be allowed to render the document. The fallback processor
MUST NOT ship or invoke a separate PDF rendering library; it MUST rely on
the engine's built-in PDF support triggered by the `Navigate` to
`http://local.example/<urlPath>`. Any companion CSS that the plugin
applies MUST be applied through a DOMContentLoaded listener that checks
for the `local.example` or `html.example` hosts so styling reaches the
loaded PDF only when appropriate. This behavior MUST be identical on
the 32-bit and 64-bit builds.

#### Scenario: A PDF is shown by the engine's built-in viewer

- **WHEN** a `.pdf` file is opened in the Lister
- **THEN** the fallback processor MUST let the WebView2 engine's
  built-in PDF viewer render the document, after `Navigate` to
  `http://local.example/<urlPath>`, and MUST NOT load any additional
  PDF library

#### Scenario: Companion CSS attaches only on matching hosts

- **WHEN** the fallback processor loads a PDF and the plugin applies a
  companion stylesheet
- **THEN** the styling MUST be applied via a DOMContentLoaded listener
  that verifies the host is `local.example` or `html.example`, so the
  CSS reaches the loaded document only when those hosts are in use on
  either the 32-bit or 64-bit build

### Requirement: Fallback virtual host mapping

The fallback processor MUST register virtual hosts so the web view can
satisfy asset and local-file references during rendering. The
`assets.example` host MUST map onto the plugin's installed directory
under `Resources/assets/` so any plugin-side stylesheets or scripts can
be loaded. The `local.example` host MUST map onto the root directory of
the file being viewed so the file navigated to through
`http://local.example/<urlPath>` resolves correctly inside the web
view. This mapping MUST be identical on the 32-bit and 64-bit builds.

#### Scenario: Local host points to the viewed file's root directory

- **WHEN** a file `D:\docs\report.pdf` is opened in the Lister
- **THEN** the fallback processor MUST map `local.example` to
  `D:\docs\` so the path part of `http://local.example/<urlPath>`
  resolves to `D:\docs\report.pdf` inside the web view on both the
  32-bit and 64-bit builds

#### Scenario: Assets host points to the plugin's assets directory

- **WHEN** the fallback processor invokes any companion styling or
  script assets
- **THEN** the `assets.example` host MUST resolve to the plugin's
  installed directory under `Resources/assets/` on both the 32-bit and
  64-bit builds

