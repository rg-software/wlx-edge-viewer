## 1. Configuration resolution (C++)

- [x] 1.1 `EdgeViewer/Globals.h`: declare `ResolveConfigSource()` (returns the file the plugin will parse, empty when there is none) and `BootstrapUserIni()` (best-effort sample→ini copy)
- [x] 1.2 `EdgeViewer/Globals.cpp`: implement both helpers — a source is "usable" when it parses into at least one section; absent / unreadable / section-less files are skipped
- [x] 1.3 `EdgeViewer/Globals.cpp`: rewire `GlobalSettings()` — explicit "loaded" flag (parse exactly once), resolution order `edgeviewer.ini` → `edgeviewer.ini.sample` → none, best-effort bootstrap copy whose failure is swallowed
- [x] 1.4 Confirm the `[WebView] UserDir` fallback still applies to the *resolved* configuration (so a sample-derived config keeps the sample's `%USERPROFILE%`)

## 2. Tests

- [x] 2.1 `EdgeViewer.Tests/tier2_config.cpp`: cover the resolution order with a `TempDir` fixture — user ini wins over the sample, section-less ini is treated as absent, sample is used when the ini is absent, no source yields an empty path
- [x] 2.2 `EdgeViewer.Tests/readme.md`: update the T2 tier row (the tier now also covers the config-source resolution)

## 3. Documentation

- [x] 3.1 `Readme.md` (Configuration): document the resolution order, that the plugin works in a read-only plugin directory, and that the user can copy `edgeviewer.ini.sample` to `edgeviewer.ini` to customize
- [x] 3.2 `openspec/notes/future-work.md`: record the remaining gap — a user-supplied `edgeviewer.ini` without a `[WebView] UserDir` key still defaults the web-engine profile to the (possibly unwritable) plugin directory; a per-user config location (`%APPDATA%` / `~/.config`) is the natural follow-up

## 4. Verification

- [x] 4.1 Build Release for Win32 and x64 (`msbuild EdgeViewer.sln /p:Configuration=Release /p:Platform=…`)
- [x] 4.2 Run both test binaries (`EdgeViewer.Tests.exe`) — all cases pass, identical set on Win32 and x64 (68 test cases / 280 assertions on each)
- [ ] 4.3 Build the Linux target (`cmake -B build -S . && cmake --build build -j`) and confirm the shared `Globals.cpp` compiles unchanged
- [x] 4.4 Windows, writable plugin dir: `edgeviewer.ini` is created from `edgeviewer.ini.sample` on first run and the plugin claims every extension of the sample's `[Extensions]` — verified against the built DLL through the exported `ListGetDetectString` (22 non-empty `EXT` tokens plus the `Dirs=1` marker)
- [x] 4.5 Windows, read-only plugin dir (ACL-locked, no `edgeviewer.ini`): no `edgeviewer.ini` is created and the plugin still claims every extension — the regression this change fixes. Same harness; before the fix the detect string was `EXT=""|EXT=""|…` (nothing claimed at all)
- [ ] 4.6 Manual (Windows): repeat 4.4/4.5 inside Total Commander — open a Markdown file and confirm it renders
- [ ] 4.7 Manual (Linux): repeat 4.4/4.5 in Double Commander
