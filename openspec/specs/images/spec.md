# images Specification

## Purpose
Display standalone image files (PNG, GIF, BMP, JPG, JPEG, ICO, WEBP, SVG)
in the Total Commander Lister pane using the bundled thumbnail-viewer
library to provide zoom and pan interactions, with a configurable
fit-to-screen behavior and light/dark CSS theme.
## Requirements
### Requirement: Image file detection

The plugin MUST detect files whose extension matches the token mapped
under the `[Extensions]` section token `Images` of edgeviewer.ini
(default: `PNG,GIF,BMP,JPG,JPEG,ICO,WEBP,SVG`). Extension matching SHALL
be case-insensitive and MUST produce identical ownership decisions on
the 32-bit and 64-bit builds. The image processor lives under
EdgeViewer/Processors/ and handles only configured image types.

#### Scenario: Detection accepts configured image extensions

- **WHEN** a file named `photo.JPG` or `icon.svg` is opened in the Lister
  and the `[Extensions]` token `Images=PNG,GIF,BMP,JPG,JPEG,ICO,WEBP,SVG`
  is present in edgeviewer.ini
- **THEN** the image processor MUST take ownership of the file regardless
  of letter case and MUST make identical ownership decisions on the
  32-bit and 64-bit builds

#### Scenario: Detection rejects unconfigured image extensions

- **WHEN** a file with an extension not listed under the `Images` token
  (for example `.tiff`) is opened
- **THEN** the image processor MUST NOT claim the file

### Requirement: Image rendering via the loader template

To render an image, the plugin MUST register the `local.example` virtual
host, load the static loader template at `Resources/assets/imgview/loader.html`,
substitute its placeholders, and render the result via
`NavigateToString`. The static thumbnail-viewer library bundled under
`Resources/assets/imgview/` MUST be used to display the image and to
provide zoom and pan interactions inside the web view. The template
placeholders substituted are `__CSS_NAME__`, `__SCREEN_CLASS__`,
`__IS_FULSCREEN__` and `__IMG_FILENAME__`. This rendering behavior MUST
be identical on the 32-bit and 64-bit builds.

#### Scenario: Loading an image through the loader template

- **WHEN** an image file `D:\photos\cat.png` is opened in the Lister
- **THEN** the image processor MUST load
  `Resources/assets/imgview/loader.html`, substitute `__IMG_FILENAME__`
  with the image's path, and render the result with `NavigateToString`
  while bootstrapping the bundled thumbnail-viewer library for image
  display

#### Scenario: Interaction support via the thumbnail-viewer library

- **WHEN** the rendered image is displayed
- **THEN** the bundled thumbnail-viewer library MUST provide zoom and
  pan interactions usable from inside the Lister pane on both the
  32-bit and 64-bit builds

### Requirement: FitToScreen mode

The image processor MUST read the `FitToScreen` value under the
`[Images]` section of edgeviewer.ini and select the loader template's
screen class accordingly. A `FitToScreen` value of `1` MUST produce the
CSS class `full-screen`; a value of `0` MUST produce the CSS class
`real-size`. The raw integer value MUST also be injected as the
`__IS_FULSCREEN__` placeholder. The `__SCREEN_CLASS__` placeholder MUST
receive exactly one of the two class names. This behavior MUST be
identical on the 32-bit and 64-bit builds.

#### Scenario: FitToScreen=1 yields the full-screen class

- **WHEN** `[Images]` contains `FitToScreen=1` and an image file is
  opened
- **THEN** the image processor MUST set `__SCREEN_CLASS__` to
  `full-screen` and `__IS_FULSCREEN__` to `1` before rendering

#### Scenario: FitToScreen=0 yields the real-size class

- **WHEN** `[Images]` contains `FitToScreen=0` and an image file is
  opened
- **THEN** the image processor MUST set `__SCREEN_CLASS__` to
  `real-size` and `__IS_FULSCREEN__` to `0` before rendering, leaving
  the image at its natural size

### Requirement: Image CSS theme selection

The image processor MUST read the `CSS` and `CSSDark` values under the
`[Images]` section of edgeviewer.ini and select which one to inject into
the loader template as `__CSS_NAME__`. When dark mode is active, the
`CSSDark` value MUST be used; otherwise the `CSS` value MUST be used.
Dark mode is set by Total Commander (the `lcp_darkmode` flag) and
selects CSSDark ini values wherever applicable. The default `CSS` value
of `none.css` MUST produce no extra styling. This behavior MUST be
identical on the 32-bit and 64-bit builds.

#### Scenario: Light mode selects the CSS value

- **WHEN** dark mode is off and `[Images]` contains `CSS=none.css`
  and `CSSDark=style-dark.css`
- **THEN** the image processor MUST inject `none.css` into the
  `__CSS_NAME__` placeholder so no extra styling is applied

#### Scenario: Dark mode selects the CSSDark value

- **WHEN** dark mode is on and `[Images]` contains `CSS=none.css` and
  `CSSDark=style-dark.css`
- **THEN** the image processor MUST inject `style-dark.css` into the
  `__CSS_NAME__` placeholder so the dark theme is applied

### Requirement: Image virtual host mapping

The image processor MUST register virtual hosts so the web view can
satisfy asset and file references during rendering. The
`assets.example` host MUST map onto the plugin's installed directory
under `Resources/assets/imgview/` so the loader template and the
thumbnail-viewer library can be loaded. The `local.example` host MUST
map onto the root directory of the file being viewed so the image's
own path can be resolved. This mapping MUST be identical on the 32-bit
and 64-bit builds.

#### Scenario: Assets host points to the plugin's image assets

- **WHEN** the loader template references the bundled thumbnail-viewer
  library or accompanying CSS
- **THEN** the image processor MUST have mapped `assets.example` to
  `Resources/assets/imgview/` in the plugin's installed directory on
  both the 32-bit and 64-bit builds

#### Scenario: Local host points to the image's root directory

- **WHEN** an image file `D:\photos\cat.png` is opened in the Lister
- **THEN** the image processor MUST map `local.example` to
  `D:\photos\` so the image resolves inside the web view

