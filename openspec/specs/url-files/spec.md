# url-files Specification

## Purpose
Open Windows Internet Shortcut files (`.url`) in the Total Commander
Lister pane by parsing their `URL=` target and either re-dispatching a
local `file:///` destination back through the processor pipeline or
navigating the web view directly to an external web page.
## Requirements
### Requirement: URL file detection

The plugin MUST detect files with a `.url` extension as Internet Shortcuts
when the `[Extensions]` section token `URL=URL` of edgeviewer.ini is
present. Extension matching SHALL be case-insensitive and MUST produce
identical ownership decisions on the 32-bit and 64-bit builds. The
affected processor lives under EdgeViewer/Processors/ and handles only
this single file type.

#### Scenario: Detection accepts the .url extension

- **WHEN** a file named `link.URL` or `BOOKMARK.url` is opened in the
  Lister and the `[Extensions]` token `URL=URL` is present in
  edgeviewer.ini
- **THEN** the URL processor MUST take ownership of the file regardless
  of letter case and MUST make identical ownership decisions on the
  32-bit and 64-bit builds

#### Scenario: Detection leaves unconfigured extensions to other processors

- **WHEN** a `.url` file is opened but the `[Extensions]` section does
  not declare the `URL` token, or a non-`.url` file is opened
- **THEN** the URL processor MUST NOT claim the file

### Requirement: URL file parsing

The URL processor MUST read the Internet Shortcut file as INI-style text
content, line by line, and locate the first line whose text begins with
`URL=`. The remainder of that line, after the `URL=` prefix, SHALL be
treated as the destination URL. The processor MUST NOT interpret the
file as a binary format or require a specific encoding beyond plain text
line reading. This parsing behavior MUST be identical on the 32-bit and
64-bit builds.

#### Scenario: Extracting the target from a URL= line

- **WHEN** a `.url` file contains a line of the form
  `URL=https://example.com/path` (possibly with surrounding INI sections
  such as `[InternetShortcut]`)
- **THEN** the URL processor MUST extract `https://example.com/path` as
  the destination URL by reading the file line by line and selecting the
  first line beginning with `URL=`

#### Scenario: Tolerating surrounding INI decoration

- **WHEN** a `.url` file contains additional INI-style key/value pairs
  before or after the `URL=` line
- **THEN** the processor MUST ignore those other pairs and use only the
  text following `URL=` as the destination

### Requirement: Local file URL handling

When the parsed destination URL begins with the `file:///` prefix, the
URL processor MUST strip that eight-character prefix and re-dispatch the
remaining local path back through the processor pipeline (the call into
the processor registry that loads and opens a path). The re-dispatched
target is then handled by whichever processor matches it - typically the
HTML processor for local `.html` files. The URL processor itself MUST
NOT render the local file directly; it MUST delegate by re-entering the
pipeline. This behavior MUST be identical on the 32-bit and 64-bit
builds.

#### Scenario: A file:/// URL re-enters the processor pipeline

- **WHEN** a `.url` file contains `URL=file:///D:/site/index.html`
- **THEN** the URL processor MUST strip the `file:///` prefix, yielding
  `D:/site/index.html`, and MUST re-dispatch that path into the
  processor registry so the matching processor (typically the HTML
  processor) renders it instead

#### Scenario: Nested pipeline selection after re-dispatch

- **WHEN** the local path produced after stripping `file:///` has an
  extension governed by a different configured token (for example `.html`
  controlled by the HTML token)
- **THEN** the re-dispatch MUST select the processor matched by that
  extension rather than the URL processor itself

### Requirement: External URL handling

When the parsed destination URL does not begin with `file:///`, the URL
processor MUST treat it as an external URL and navigate the WebView2
engine directly to that URL using `Navigate` rather than
`NavigateToString`. External `http://` and `https://` URLs SHALL load
inside the web view, throwing the responsibility for fetching resources
and rendering the page onto the WebView2 engine itself. The processor
MUST NOT attempt to parse, cache, or transform the external page. This
behavior MUST be identical on the 32-bit and 64-bit builds.

#### Scenario: Navigating to an external HTTP URL

- **WHEN** a `.url` file contains `URL=http://example.com/article`
- **THEN** the URL processor MUST call `Navigate` on the web view with
  `http://example.com/article` so the WebView2 engine loads and renders
  the external page directly inside the Lister pane

#### Scenario: Navigating to an external HTTPS URL

- **WHEN** a `.url` file contains `URL=https://example.com/login`
- **THEN** the URL processor MUST call `Navigate` with that HTTPS URL
  and MUST leave the engine to perform certificate validation and page
  loading without intercepting either step

### Requirement: URL virtual host mapping

For external URL handling only, the URL processor MUST register the
`local.example` virtual host mapped onto the root directory of the
`.url` file being viewed. The mapping is established as a supporting
step before the external `Navigate` call, even though the destination is
not itself a local file. For the local `file:///` re-dispatch path,
no per-file virtual host mapping is installed by the URL processor; the
re-dispatched target processor performs its own mapping. This behavior
MUST be identical on the 32-bit and 64-bit builds.

#### Scenario: External URL path maps local.example to the shortcut's root

- **WHEN** an external `.url` file located at
  `E:\bookmarks\news.url` is opened and its destination is an `http(s)`
  URL
- **THEN** the URL processor MUST map `local.example` to
  `E:\bookmarks\` before navigating to the external URL, so any local
  fallback references inside the loaded page resolve there

#### Scenario: Local file:/// handling defers mapping to the re-dispatched processor

- **WHEN** a `.url` file contains `URL=file:///D:/site/index.html`
- **THEN** the URL processor MUST NOT install a per-file virtual host
  mapping of its own and MUST let the re-dispatched processor (typically
  the HTML processor) perform whatever mapping it needs

