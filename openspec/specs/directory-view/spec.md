# directory-view Specification

## Purpose
Render a file-system directory as an interactive thumbnail grid inside
the Total Commander Lister pane, listing subdirectories (with optional
shell-extracted live thumbnails) and a configurable subset of files
whose extensions match configured filter lists, using a static loader
template under Resources/assets/dirviewer/ to drive the layout.
## Requirements
### Requirement: Directory detection

The plugin MUST treat a path as a directory listing only when BOTH the
`[Extensions]` section of edgeviewer.ini contains the `Dirs=1` token AND
the path resolves to an actual directory. The directory processor's path
initializer MUST normalize the incoming path by stripping any trailing
`..\` sequences before resolving it to its physical path, so Relative
Total Commander navigation strings do not confuse detection. Detection
SHALL be case-insensitive where the filesystem is case-insensitive and
MUST produce identical ownership decisions on the 32-bit and 64-bit
builds. The directory processor lives under EdgeViewer/Processors/.

#### Scenario: Dirs=1 enables directory ownership

- **WHEN** `[Extensions]` contains `Dirs=1` and a path that resolves to
  an existing directory is opened in the Lister
- **THEN** the directory processor MUST take ownership of the path and
  render a directory listing on both the 32-bit and 64-bit builds

#### Scenario: Dirs=0 disables directory ownership

- **WHEN** `[Extensions]` contains `Dirs=0` (or no `Dirs` token at all)
  and a directory path is opened
- **THEN** the directory processor MUST NOT claim the path, regardless
  of whether the directory exists on disk

#### Scenario: Trailing back-segments are normalized before detection

- **WHEN** Total Commander passes a path with a trailing `..\` sequence
  such as `D:\docs\sub\..\..\` and `Dirs=1` is configured
- **THEN** the directory processor MUST strip the trailing `..\` and
  resolve the resulting normalized path to its physical path before
  deciding ownership

### Requirement: Directory listing generation

The directory processor MUST iterate the directory's entries and render
them in a fixed sort order: subdirectories MUST be listed before files,
and within each group entries MUST be ordered alphabetically by
filename. Only entries that match either the configured image extension
filter or the configured other-extension filter SHALL be shown for
files; entries that match neither filter MUST be skipped entirely (see
the hiding requirement below). Directories always appear subject to the
`ShowFolders` option. The generated listing MUST be inserted into the
loader template as the body placeholder and MUST be identical on the
32-bit and 64-bit builds.

#### Scenario: Subdirectories precede files alphabetically

- **WHEN** a directory containing files `apple.txt`, `banana.txt` and
  subdirectories `Zeta`, `Alpha` is opened with `ShowFolders=1`
- **THEN** the rendered listing MUST order subdirectories first as
  `Alpha`, `Zeta`, followed by the files `apple.txt`, `banana.txt`

#### Scenario: Only filtered entries are listed

- **WHEN** a directory contains `photo.jpg`, `clip.mp4`, `notes.docx`
  and the configured filters are `DirImageExt=jpg` and
  `DirOtherExt=mp4`
- **THEN** the listing MUST include `photo.jpg` and `clip.mp4` and MUST
  omit `notes.docx` because it matches neither filter

### Requirement: Directory thumbnail for subdirectories

For each subdirectory entry, the directory processor MUST produce a
thumbnail that anchors the entry. When the `[Directory]` option
`GenDirThumbs=1` is active, the processor MUST request a shell thumbnail
for that directory by creating a shell item from the parsing name and
using the shell item image factory to obtain a thumbnail of the size
configured by `DirThumbSize` (default `256`) with the
bigger-size-allowed thumbnail-only flags, then MUST encode that bitmap
to PNG via GDI+, base64-encode it, and embed it as a `data:` URI directly
in the generated HTML. When `GenDirThumbs=0`, the processor MUST instead
use the static `folder.png` icon bundled under
`Resources/assets/dirviewer/`. The thumbnail generation behavior SHALL
be identical on the 32-bit and 64-bit builds, even where the underlying
shell apis differ between bitnesses.

#### Scenario: GenDirThumbs=1 produces a live shell thumbnail

- **WHEN** `GenDirThumbs=1` and `DirThumbSize=256` are configured and a
  subdirectory `Images` is being listed
- **THEN** the directory processor MUST request a shell thumbnail of
  size 256x256 for `Images`, encode it as a base64 PNG embedded through
  a `data:` URI, and use that as the entry's thumbnail in the rendered
  HTML

#### Scenario: GenDirThumbs=0 uses the static folder icon

- **WHEN** `GenDirThumbs=0` is configured and a subdirectory `Images`
  is being listed
- **THEN** the directory processor MUST use the static `folder.png`
  icon under `Resources/assets/dirviewer/` as the entry's thumbnail
  instead of calling the shell item image factory

### Requirement: Image file display in directory view

For each file whose extension matches the `DirImageExt` filter
(default: `jpg|jpeg|png|gif|svg|bmp|webp`) of the `[Directory]`
section, the directory processor MUST render the file as a clickable
anchor element whose thumbnail is the image file itself (so activating
the entry opens that image file in the Lister). The image MUST be
loaded through the configured virtual hosts and displayed as the
thumbnail tile. This behavior MUST be identical on the 32-bit and
64-bit builds.

#### Scenario: Matching image files become clickable thumbnails

- **WHEN** `DirImageExt=jpg|png|svg` is configured and a directory
  contains `cover.jpg`
- **THEN** the directory processor MUST render `cover.jpg` as an
  anchor element whose thumbnail is `cover.jpg` itself, and activating
  that anchor MUST open `cover.jpg` inside the Lister

#### Scenario: Non-matching image files fall through

- **WHEN** `DirImageExt=png` only is configured and a directory
  contains `archive.zip` and `flag.png`
- **THEN** the file `flag.png` MUST be rendered as an image thumbnail
  tile while `archive.zip` is handled by whichever filter (if any)
  matches it

### Requirement: Other file display in directory view

For each file whose extension matches the `DirOtherExt` filter
(default: `mp4|avi|txt`) of the `[Directory]` section, the directory
processor MUST render the file as an entry that uses the static
`file.png` icon bundled under `Resources/assets/dirviewer/` as its
thumbnail. The entry MUST be displayed without producing a per-file
thumbnail extraction call. This behavior MUST be identical on the
32-bit and 64-bit builds.

#### Scenario: Matching non-image files use the static file icon

- **WHEN** `DirOtherExt=mp4|avi|txt` is configured and a directory
  contains `notes.txt`
- **THEN** the directory processor MUST render `notes.txt` as an entry
  showing the static `file.png` icon and MUST NOT attempt to extract a
  per-file shell thumbnail for it

#### Scenario: Activating an other-extension file opens it in the Lister

- **WHEN** a user activates the entry for a `.txt` file rendered with
  the static `file.png` icon
- **THEN** the Lister MUST load that file through whatever processor
  matches its extension, since the entry anchors that file path

### Requirement: Files not matching any extension filter are hidden

The directory processor MUST NOT list any file whose extension matches
neither the `DirImageExt` filter nor the `DirOtherExt` filter of the
`[Directory]` section. Such files SHALL be omitted entirely from the
generated listing and MUST NOT be rendered as visible empty tiles. This
hiding behavior MUST be identical on the 32-bit and 64-bit builds.

#### Scenario: Files matching neither filter are omitted

- **WHEN** a directory contains `archive.zip` and `spreadsheet.xlsx` and
  the configured filters are `DirImageExt=png|jpg` and
  `DirOtherExt=mp4|txt`
- **THEN** neither `archive.zip` nor `spreadsheet.xlsx` MUST appear in
  the rendered listing on either the 32-bit or the 64-bit build

#### Scenario: Subdirectories are not affected by file filters

- **WHEN** a directory contains a subdirectory whose name ends with a
  file-like extension (for example `archive.zip`) and `ShowFolders=1`
  is configured
- **THEN** the subdirectory MUST still be listed as a directory entry
  because the `DirImageExt` and `DirOtherExt` filters apply only to
  files, not to subdirectories

### Requirement: Directory CSS theme selection

The directory processor MUST read the `CSS` and `CSSDark` values under
the `[Directory]` section of edgeviewer.ini and inject one of them into
the loader template as the `__CSS_NAME__` placeholder. When dark mode is
active, the `CSSDark` value MUST be used; otherwise the `CSS` value MUST
be used. Dark mode is signalled by Total Commander's `lcp_darkmode`
flag. This selection MUST be identical on the 32-bit and 64-bit builds.

#### Scenario: Light mode selects the CSS value

- **WHEN** dark mode is off and `[Directory]` contains `CSS=light.css`
  and `CSSDark=dark.css`
- **THEN** the directory processor MUST inject `light.css` into
  `__CSS_NAME__` so the light theme is applied

#### Scenario: Dark mode selects the CSSDark value

- **WHEN** dark mode is on and `[Directory]` contains `CSS=light.css`
  and `CSSDark=dark.css`
- **THEN** the directory processor MUST inject `dark.css` into
  `__CSS_NAME__` so the dark theme is applied on both the 32-bit and
  64-bit builds

### Requirement: Directory view configuration options

The directory processor MUST read the following options from the
`[Directory]` section of edgeviewer.ini and inject each one into a
corresponding placeholder of the loader template at
`Resources/assets/dirviewer/loader.html`: `ShowNames` into
`__SHOW_NAMES__`, `ShowFolders` into `__SHOW_FOLDERS__`, `FitToScreen`
into `__FIT_TO_SCREEN__`, `TruncateNames` into `__TRUNCATE_NAMES__`,
and `NamesUnderThumbnails` into `__NAMES_UNDER_THUMBS__`. The base
location placeholders `__BASE_URL__` and `__BASE_PATH__` MUST also be
set so the template can resolve relative references. The processor MUST
forward the configured values verbatim and MUST NOT coerce them beyond
the integer-or-string form already present in edgeviewer.ini. This
behavior MUST be identical on the 32-bit and 64-bit builds.

#### Scenario: Each option is wired to its placeholder

- **WHEN** `[Directory]` contains `ShowNames=1`, `ShowFolders=1`,
  `FitToScreen=1`, `TruncateNames=1`, `NamesUnderThumbnails=1` and a
  directory is opened
- **THEN** the directory processor MUST inject `1` into each of the
  `__SHOW_NAMES__`, `__SHOW_FOLDERS__`, `__FIT_TO_SCREEN__`,
  `__TRUNCATE_NAMES__` and `__NAMES_UNDER_THUMBS__` placeholders of the
  loaded `Resources/assets/dirviewer/loader.html` template before
  rendering

#### Scenario: Base url and base path are populated

- **WHEN** a directory `D:\docs\research` is opened
- **THEN** the directory processor MUST populate `__BASE_URL__` and
  `__BASE_PATH__` so the template's relative references resolve against
  that directory inside the web view

### Requirement: Directory virtual host mapping

The directory processor MUST register virtual hosts so the web view can
satisfy asset and file references during rendering. The `assets.example`
host MUST map onto the plugin's installed directory under
`Resources/assets/dirviewer/` so the loader template, the static
`folder.png` and `file.png` icons, and any stylesheets can be loaded.
The `local.example` host MUST map onto the directory being viewed so
file and subdirectory references resolve inside the web view. This
mapping MUST be identical on the 32-bit and 64-bit builds.

#### Scenario: Assets host points to the directory viewer assets

- **WHEN** the loader template references the bundled `folder.png`,
  `file.png` or its CSS
- **THEN** the directory processor MUST have mapped `assets.example`
  to `Resources/assets/dirviewer/` in the plugin's installed directory
  on both the 32-bit and 64-bit builds

#### Scenario: Local host points to the directory being viewed

- **WHEN** the directory `D:\docs\research` is opened in the Lister
  on either the 32-bit or 64-bit build
- **THEN** the directory processor MUST map `local.example` to
  `D:\docs\research\` so file and subdirectory references resolve
  correctly inside the web view

### Requirement: Right-click context menu in directory view

The directory view MUST offer the shell context menu for entries it
renders. When the user right-clicks inside the directory view, the
rendered page MUST signal a menu command (the command referred to in the
renderer assets as `CMD_MENU`) back to the plugin host via a
`WM_COPYDATA` message, and the plugin MUST request from the operating
system the shell context menu appropriate to the clicked entry. The shell
context menu MUST then be shown by the plugin host. This behavior MUST
be identical on the 32-bit and 64-bit builds, modulo the bitness-specific
shell apis the operating system exposes for obtaining the menu.

#### Scenario: Right-click triggers a shell context menu

- **WHEN** a user right-clicks an entry inside the directory view
- **THEN** the rendered page MUST send a `CMD_MENU` notification to the
  plugin host via `WM_COPYDATA`, and the plugin MUST display the shell
  context menu for the affected entry on both the 32-bit and 64-bit
  builds

#### Scenario: Non-menu commands remain unchanged

- **WHEN** a right-click does not occur and other renderer features
  produce command notifications
- **THEN** those notifications MUST be handled according to their own
  semantics independent of the `CMD_MENU` path

### Requirement: UNC path limitation

The directory processor MUST NOT attempt to render directory listings for
UNC paths such as `\\localhost\c$\dir`. The header of the directory
processor source notes explicitly that the module does not support UNC
paths. When a UNC path is opened in the Lister, the plugin SHALL leave
the UNC directory either unrendered by the directory processor or
unowned outright, consistent with that explicit limitation. This
limitation MUST be observed identically on the 32-bit and 64-bit
builds.

#### Scenario: A UNC directory is not rendered by the directory processor

- **WHEN** a path of the form `\\localhost\c$\dir` is opened in the
  Lister on either the 32-bit or the 64-bit build
- **THEN** the directory processor MUST NOT render that UNC directory
  listing, in accordance with its documented non-support for UNC paths

#### Scenario: Local drive directories remain supported

- **WHEN** a path of the form `D:\docs\research` on a local drive is
  opened in the Lister
- **THEN** the directory processor MUST render that directory listing
  normally, so the UNC limitation affects only UNC-styled paths and not
  ordinary drive paths

