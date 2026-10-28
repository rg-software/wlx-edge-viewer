## Why

The plugin bootstraps its configuration by copying `edgeviewer.ini.sample` to
`edgeviewer.ini` on first run (the #81 fix that stopped plugin updates from
overwriting user customizations). That copy is best-effort in intent but not in
code: `fs::copy_file`'s `std::error_code` is discarded, and when the plugin
directory is not writable the copy fails silently. mINI then finds no
`edgeviewer.ini` to parse, the configuration stays empty, `[Extensions]` is
absent, and the plugin claims **no file types at all** — the detect string
degenerates to the literal `EXT=",,,,,,,,"`, so Total Commander never routes a
single file to it and the user sees a plugin that simply does nothing, with no
error message anywhere.

A read-only plugin directory is a legitimate install layout (TC/DC installed
under `Program Files` or `/usr/share`, a system-wide Linux install, a
read-only mount), so the plugin must work there.

## What Changes

- `GlobalSettings()` (`EdgeViewer/Globals.cpp`) gains an explicit **config
  source resolution** instead of "parse `edgeviewer.ini`, hope it exists":
  1. `edgeviewer.ini` next to the plugin DLL (the user's file),
  2. `edgeviewer.ini.sample` next to the plugin DLL (the shipped template,
     already deployed by every release path on both platforms),
  3. no configuration (today's behavior: the plugin claims no file types).
- The sample→ini bootstrap copy stays, but its failure becomes explicitly
  non-fatal: it is a convenience that gives the user an editable ini on a
  writable install, never a precondition for the plugin working.
- An `edgeviewer.ini` that parses into **no sections** (empty file, comments
  only) is treated as absent and the sample is used instead.
- A present `edgeviewer.ini` still replaces the sample wholesale — the sample
  is a fallback, never an overlay — so no existing user's key semantics change
  (e.g. `OfflineMode`, `ShowErrorBoxes`, `KeepZoom` keep today's values).
- The resolved configuration is what the `[WebView] UserDir` fallback applies
  to. Because the shipped sample sets `UserDir=%USERPROFILE%`, a read-only
  install now also gets a writable web-engine profile directory instead of
  defaulting to the (unwritable) plugin directory.
- The `plugin-config` spec is brought back in line with the code: the
  "MUST NOT write to this file at runtime" clause becomes "MUST NOT modify an
  existing ini; MAY create one from the sample, best-effort", and the
  "ini file is absent → empty configuration" scenario becomes the documented
  resolution order.

## Capabilities

### New Capabilities

None — this hardens existing configuration behavior rather than adding a new
surface.

### Modified Capabilities

- `plugin-config`: the config-file location/format requirement gains the
  best-effort bootstrap rule, the lazy-load requirement is restated in terms of
  the *resolved* source, and a new requirement defines the
  `edgeviewer.ini` → `edgeviewer.ini.sample` → no-configuration resolution
  order and its observable consequences.

## Impact

- **Modified code**: `EdgeViewer/Globals.cpp` (`GlobalSettings()` +
  new helpers `ResolveConfigSource()` / `BootstrapUserIni()`),
  `EdgeViewer/Globals.h` (helper declarations).
- **Tests**: `EdgeViewer.Tests/tier2_config.cpp` gains coverage for the
  resolution order (user ini wins, empty ini treated as absent, sample used
  when the ini is absent, no-source case) via the existing `TempDir` fixture.
- **Docs**: `Readme.md` (Configuration section: where the config comes from
  and what to do when the plugin directory is read-only),
  `EdgeViewer.Tests/readme.md` (tier table), `openspec/notes/future-work.md`
  (the `[WebView] UserDir` default still points at the plugin directory, which
  is unwritable in a read-only install when the user supplies their own ini
  without a `UserDir` key).
- **No packaging change**: `edgeviewer.ini.sample` is already shipped next to
  the DLL by `BuildMakeSetup.bat`, `BuildMakeSetup.sh`, `CMakeLists.txt` and
  `.github/workflows/release.yml`.
- **No dependency change**: `vcpkg.json` untouched.
