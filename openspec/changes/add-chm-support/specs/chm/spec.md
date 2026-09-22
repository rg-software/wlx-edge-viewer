## Purpose

Lets Total Commander and Double Commander users view Microsoft Compiled HTML
Help (`.chm`) files inline in the lister pane, rendering the archive's HTML
topics with the real web engine and providing a table-of-contents sidebar for
navigating the help file, identically on 32-bit and 64-bit builds and on
Windows and Linux.

## ADDED Requirements

### Requirement: CHM file detection

The plugin SHALL route files whose extension matches the `CHM` extension set
from the `[Extensions]` section of `edgeviewer.ini` (shipped as `CHM=CHM`) to
the CHM processor instead of any fallback processor. Detection SHALL be
case-insensitive and SHALL behave identically on 32-bit and 64-bit builds and
on both platforms. The CHM processor lives under `EdgeViewer/Processors/` and
serves only this file type.

#### Scenario: Opening a .chm file

- **WHEN** the user opens a file named `manual.chm` in the lister and
  `[Extensions]` contains `CHM=CHM`
- **THEN** the CHM processor renders the help file instead of the generic
  fallback processor

#### Scenario: Non-matching extension

- **WHEN** the user opens a file that does not match the `CHM` extension set
  (e.g. `.mht`)
- **THEN** the CHM processor is not selected and other processors handle the
  file as before

### Requirement: CHM archive decoding

The plugin SHALL decode the CHM container natively (the `ITSF`/`ITSP` directory
and its LZX-compressed content) using the bundled archive decoder, exposing the
archive's entries by path. The decoder SHALL read the archive's control streams
to resolve the help file's title, its default (home) topic, its table-of-contents
entry path, and its declared code page. Entries SHALL be read on demand rather
than extracted wholesale to disk. This SHALL behave identically on 32-bit and
64-bit builds and on both platforms.

#### Scenario: Home topic and TOC are resolved from the archive

- **WHEN** a CHM declares a default topic and a table-of-contents file in its
  control streams
- **THEN** the plugin resolves those paths from the archive and uses them for
  the initial view and the sidebar

#### Scenario: Entry read on demand

- **WHEN** a topic references an image or stylesheet stored in the archive
- **THEN** the plugin reads that entry's bytes from the archive when it is
  requested, without extracting the whole archive to a temporary directory

### Requirement: Default topic rendering

The CHM view SHALL render the archive's default topic as the initial document
using the web engine, so the topic's HTML, CSS, images, and in-page scripts are
rendered as a web document rather than rasterized to a bitmap. When the archive
does not declare a usable default topic, the plugin SHALL fall back to the first
HTML entry in the archive. The rendering behavior SHALL be identical on 32-bit
and 64-bit builds and on both platforms.

#### Scenario: Opening a CHM lands on the home topic

- **WHEN** the user opens a CHM whose declared default topic is `overview.htm`
- **THEN** the lister shows `overview.htm` rendered as a document

#### Scenario: Missing default topic falls back

- **WHEN** a CHM declares no default topic or the declared topic is not present
  in the archive
- **THEN** the lister renders the first HTML entry in the archive instead of
  showing an empty view

### Requirement: Archive-backed resource serving

The plugin SHALL serve archive-internal resources (topic documents, images,
stylesheets, scripts, and other referenced entries) to the web engine through a
host-side content source bound to the open archive, so that relative references
inside a topic resolve within the archive. Each open CHM SHALL be identified by
a per-view instance token that appears in the served URLs, so that two open
archives never resolve each other's resources. Served text entries SHALL carry a
content type derived from the entry's extension. This SHALL behave identically on
32-bit and 64-bit builds and on both platforms.

#### Scenario: Relative image resolves inside the archive

- **WHEN** a topic contains `<img src="images/logo.png">` and that entry exists
  in the archive
- **THEN** the web engine loads the image from the archive rather than reporting
  a missing resource

#### Scenario: Two views do not share resources

- **WHEN** two listers show two different CHM archives at the same time
- **THEN** each view's resources resolve against its own archive, because each
  view's URLs carry its own instance token

### Requirement: Table-of-contents sidebar

The CHM view SHALL display a table-of-contents sidebar built from the archive's
table-of-contents entry (the `.hhc` file), preserving the nested list structure.
Activating a sidebar entry SHALL navigate the content pane to the entry's target
topic. When the table-of-contents entry is missing or cannot be parsed, the
sidebar SHALL be empty and the content pane SHALL still render the default topic.
The sidebar's appearance SHALL follow the plugin's light/dark stylesheet
selection.

#### Scenario: Nested table of contents is shown

- **WHEN** a CHM's `.hhc` contains nested `<ul>` lists of topics
- **THEN** the sidebar shows a collapsible tree matching that nesting

#### Scenario: Sidebar entry navigates

- **WHEN** the user activates a sidebar entry whose target is `topics/setup.htm`
- **THEN** the content pane navigates to that topic

#### Scenario: Missing or unparseable TOC

- **WHEN** the archive has no `.hhc` entry or the entry cannot be parsed
- **THEN** the sidebar is empty and the default topic still renders in the
  content pane

### Requirement: In-archive navigation

Links inside a rendered topic that point to other archive entries SHALL navigate
within the archive, and links carrying a `#fragment` SHALL scroll to the named
anchor in the target topic. Archive-absolute link forms (the `ms-its:` and
`mk:@MSITStore:` protocols) SHALL be resolved to the corresponding archive entry.
Links to external URLs SHALL NOT be followed inside the lister view.

#### Scenario: Relative topic link navigates

- **WHEN** a rendered topic contains a link to `../guide/intro.htm` which exists
  in the archive
- **THEN** activating the link shows that topic in the content pane

#### Scenario: Anchor link scrolls to the target

- **WHEN** a rendered topic contains a link to `intro.htm#install` and
  `intro.htm` contains an element named `install`
- **THEN** activating the link shows `intro.htm` scrolled to that element

### Requirement: Topic charset handling

The plugin SHALL decode topic text using the archive's declared code page when
it is known, so help files authored in a non-Latin single-byte code page render
their text correctly. When the declared code page is unknown or absent, the web
engine's own charset sniffing (BOM and in-document declaration) SHALL apply.
This SHALL behave identically on 32-bit and 64-bit builds and on both platforms.

#### Scenario: Declared code page renders correctly

- **WHEN** a CHM declares a Cyrillic code page and its topic bytes are in that
  code page
- **THEN** the topic text renders as readable Cyrillic rather than mojibake

#### Scenario: Unknown code page falls back to engine sniffing

- **WHEN** a CHM declares no code page but its topic declares one in a
  `<meta>` tag
- **THEN** the topic renders using the engine's sniffing of the document

### Requirement: Malformed archive handling

The CHM view SHALL NOT crash or hang on a malformed, truncated, or non-CHM
file. When the archive cannot be opened or its control streams cannot be read,
the plugin SHALL fail the load gracefully or show an in-viewer error message,
without leaving a broken view or a leaked archive handle. This SHALL hold for
both 32-bit and 64-bit builds.

#### Scenario: File that is not a valid CHM

- **WHEN** a file with a `.chm` extension is not a valid CHM archive
- **THEN** the view shows an error message (or the load fails cleanly) without
  crashing the lister

#### Scenario: Truncated archive

- **WHEN** a CHM is truncated so an entry cannot be read
- **THEN** the plugin reports the failure in the view without crashing and
  releases the archive handle

### Requirement: CHM platform and build parity

The CHM detection, archive decoding, resource serving, default-topic rendering,
table-of-contents sidebar, in-archive navigation, and charset behavior SHALL be
identical between the 32-bit (Win32) and 64-bit (x64) Windows builds and between
Windows (WebView2) and Linux (Qt Web Engine), except where a platform limitation
is explicitly documented. The archive handle SHALL be released when the lister
view is closed on both platforms.

#### Scenario: Same behavior on both builds

- **WHEN** the same CHM is opened in the 32-bit and 64-bit plugins
- **THEN** both render the same home topic and sidebar, and both release the
  archive when the view closes

#### Scenario: Same behavior on both platforms

- **WHEN** the same CHM is opened on Windows and on Linux
- **THEN** both render the home topic with a table-of-contents sidebar and
  resolve relative resources inside the archive
