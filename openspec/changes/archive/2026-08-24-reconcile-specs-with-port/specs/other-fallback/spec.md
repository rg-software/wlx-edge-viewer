## MODIFIED Requirements

### Requirement: PDF rendering via the browser's built-in PDF viewer

For files whose extension is `PDF`, the WebView2 engine's built-in PDF
viewer MUST be allowed to render the document. The fallback processor
MUST NOT ship or invoke a separate PDF rendering library; it MUST rely on
the engine's built-in PDF support triggered by the `Navigate` to
`http://local.example/<urlPath>`. Any companion CSS that the plugin
applies MUST be applied through a DOMContentLoaded listener that checks
for the `local.example` host so styling reaches the
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
  that verifies the host is `local.example`, so the
  CSS reaches the loaded document only when that host is in use on
  either the 32-bit or 64-bit build
