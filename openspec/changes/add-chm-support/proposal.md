## Why

Microsoft Compiled HTML Help (`.chm`) files are the standard offline
documentation format for Windows software and are common in a TC user's
file panel, but the lister cannot open them. The shipped `Readme.md` already
tells users to install a separate plugin (`wlx-multidoc-viewer`) for CHM;
folding CHM into EdgeViewer lets the same WebView2 / Qt Web Engine stack
render help files with full HTML/CSS fidelity, in-archive navigation, and a
JavaScript table-of-contents sidebar instead of a second plugin.

CHM is the first format that cannot follow the project's "thin C++ + vendored
JS parser" pattern: its content lives in an LZX-compressed binary archive
(`ITSF`/`ITSP` + directory + `/#SYSTEM`, `/#WINDOWS`, `.hhc`) that no browser
library can decode. The archive layer is therefore native C++ (`chmlib`),
while everything user-visible stays in static assets and the existing
web-engine pipeline.

## What Changes

- Add `chmlib` (LGPL-2.1-or-later, static) as a pinned vcpkg dependency via a
  local overlay port, because the builtin `chmlib` port's download URL
  (`jedrea.com`) now returns 404. The overlay pins the maintained
  `jedwing/CHMLib` mirror at a fixed commit and ships the missing CMake
  package config.
- Add a new CHM processor and a host-side archive wrapper that open the
  archive, read `/#SYSTEM` (default topic, TOC path, title, code page) and
  `/#WINDOWS`, enumerate entries, and retrieve individual entries on demand.
- Serve archive-internal content through an **archive-backed custom-scheme
  source**: the existing `evh://` (Windows) / `ev://` (Linux) handlers gain a
  per-view source that reads an entry out of the open CHM instead of a mapped
  folder, so relative links, images, CSS, and `#anchor` fragments resolve
  inside the archive. No bulk extraction to disk; no temp directory.
- Add `Resources/assets/chm/loader.html` plus JS/CSS assets that render the
  default topic in a content frame and build a collapsible TOC sidebar by
  parsing the archive's `.hhc` file (tolerant scanner, no new JS dependency).
- Reuse the existing charset machinery for CHM pages (declared code page,
  `<meta>` sniffing, the manual **Encoding** submenu) rather than a new
  decode path.
- Wire detection: `[Extensions] CHM=CHM`, the `CHM` entry in the detect-string
  section order, the new `[CHM]` stylesheet section, and the new source files
  in `EdgeViewer.vcxproj` / `CMakeLists.txt`.
- Windows narrow-path handling: stage a temp copy only when the CHM's *file
  name* is not representable in the system code page (chmlib's ANSI
  `chm_open`), reusing the existing temp-file tracking and cleanup.
- Full cross-platform parity (Windows WebView2 + Linux Qt Web Engine).

## Capabilities

### New Capabilities

- `chm`: CHM detection, native archive decoding (chmlib), archive-backed
  resource serving, default-topic rendering, `.hhc` sidebar table of
  contents, in-archive link/anchor navigation, page charset handling,
  malformed-archive fallback, and 32/64-bit plus Windows/Linux parity.

### Modified Capabilities

- `plugin-config`: `[Extensions]` gains the `CHM=CHM` token and the shipped
  section list gains `[CHM] CSS`/`CSSDark`; the detect-string section order
  and the per-type stylesheet requirement are extended.
- `wlx-contract`: the hardcoded detect-string type-section order gains `CHM`
  (and the "same detect string on both builds" requirement now covers it).
- `virtual-host-mapping`: introduces a third content source alongside the two
  folder mappings — an archive-backed custom-scheme source bound per view.
- `temp-file-management`: a CHM whose file name is not representable in the
  system code page is staged to a tracked temp copy for `chm_open`, and
  archive-backed serving creates no bulk extraction.
- `linux-parity`: adds a CHM rendering parity row (same behavior on Windows
  and Linux, with any divergence recorded).

## Impact

- **New code**: `EdgeViewer/Processors/ChmProcessor.{h,cpp}` (thin, mirrors
  `MhtProcessor`), a host-side `EdgeViewer/ChmArchive.{h,cpp}` (chmlib
  wrapper: open/close, `#SYSTEM`/`#WINDOWS` parsing, enumerate, retrieve,
  code page), and a per-view archive-source registration on `IWebView` /
  both backends.
- **New assets**: `Resources/assets/chm/loader.html`, sidebar + stylesheet
  JS/CSS.
- **Modified code**: `EdgeViewer/WebView/WebView2Backend.cpp`,
  `EdgeViewer/WebView/QtWebEngineBackend.cpp` (archive-backed scheme branch),
  `EdgeViewer/IWebView.h` (source registration), `EdgeViewer/WlxDetect.cpp`
  and `EdgeViewer/DllMain.cpp` (detect string),
  `EdgeViewer/Platform_{Win,Linux}.cpp` (narrow-path staging on Windows).
- **Build/deps**: `vcpkg.json` (`chmlib`), new `overlay-ports/chmlib/`
  (portfile, manifest, CMakeLists, `strings_h.patch`),
  `vcpkg-configuration.json` (`overlay-ports` entry), `CMakeLists.txt`
  (`find_path`/`find_library` fallback + link), `EdgeViewer.vcxproj`
  (+`.filters`).
- **Config**: `Resources/edgeviewer.ini` (`CHM=CHM`, `[CHM]`).
- **Packaging**: the new `Resources/assets/chm/` tree is copied by the
  existing `Resources\` → `assets` deployment (no packaging change beyond the
  dependency DLL/static link).
- **Known deferred**: `$FIftiMain` full-text index decoding (search stays the
  engine's find over the current topic); `.hhk` index UI; non-HTML CHM
  payloads (scripts that require ActiveX/`hhctrl` APIs).
