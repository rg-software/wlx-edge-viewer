# Design

## Context

`GlobalSettings()` (`EdgeViewer/Globals.cpp`) is the single choke point for
the plugin's configuration. Today it does three things in one block:

1. `fs::copy_file(edgeviewer.ini.sample → edgeviewer.ini)` when the ini is
   missing — the #81 fix, with the `std::error_code` discarded.
2. `mINI::INIFile::read()` on `edgeviewer.ini`, return value discarded.
3. The `[WebView] UserDir` fallback (plugin directory) when the key is
   absent.

The failure mode this change addresses: in a read-only plugin directory step 1
fails, step 2 reads nothing, and the configuration stays empty. Every
consumer then reads an empty string / `0`:

- `ProcessorInterface::isType` (`ProcessorInterface.cpp:92`) matches nothing →
  `FindProcessor` returns null → `ListLoadW` returns `nullptr`.
- `BuildDetectString` (`WlxDetect.cpp:16`) emits the literal
  `EXT=",,,,,,,,"` → Total Commander never routes a file to the plugin.

The plugin is inert with no diagnostic. The same empty configuration also
means `[WebView] ShowErrorBoxes` is `0`, so even the *next* failure (WebView2
environment creation) would be silent.

## Goals / Non-Goals

**Goals:**

- The plugin MUST work (claim its file types, render) when it is installed in
  a directory it cannot write to.
- Keep the #81 guarantee: an existing `edgeviewer.ini` is never modified,
  never overwritten, never re-created.
- Keep one source of truth for the shipped defaults. `Resources/edgeviewer.ini`
  is already deployed as `edgeviewer.ini.sample` next to the DLL by every
  release path (Windows: `BuildMakeSetup.bat` + `release.yml`; Linux:
  `BuildMakeSetup.sh`, `CMakeLists.txt` `install(... RENAME
  edgeviewer.ini.sample)`, `release.yml`) — it can serve as the fallback
  configuration without a second copy of the settings in C++.

**Non-Goals:**

- No new ini keys, no change to the ini format, no change to which keys the
  processors read.
- No relocation of the configuration into a per-user location
  (`%APPDATA%` / `~/.config`). That would be the "proper" fix for read-only
  installs but it changes where every existing user's config lives — a
  migration question, not a bug fix. Deferred (see `future-work.md`).
- No change to the shipped `Resources/edgeviewer.ini` content.

## Decisions

### D1 — Resolution order: `edgeviewer.ini` → `edgeviewer.ini.sample` → none

`GlobalSettings()` resolves the configuration from the first source that
yields a usable configuration.

- *Alternatives considered:*
  - **Hardcode the defaults in C++** and overlay the user's ini on top.
    Rejected: it duplicates the whole ini in a second place. The extension
    lists are already duplicated between `Resources/edgeviewer.ini`,
    `WlxDetect.cpp`'s hardcoded section list and the processor registry
    (tracked as TDB-01); adding a fourth copy would grow that debt instead
    of shrinking it.
  - **Revert to shipping `edgeviewer.ini` and overwriting it on update.**
    Rejected: that is exactly issue #81 — user customizations would be lost
    on every plugin update.
  - **Write the generated ini into a per-user location.** Rejected here (see
    Non-Goals), but it is the natural follow-up if the sample ever stops being
    a sufficient fallback.

### D2 — The bootstrap copy stays, but it is a convenience, not a precondition

`BootstrapUserIni()` still copies `edgeviewer.ini.sample` →
`edgeviewer.ini` when no ini exists, so a writable install keeps giving the
user an editable, commented ini next to the DLL. Its failure is swallowed
(deliberately: there is nothing the user can do about a read-only install
directory, and the fallback in D1 already covers it). It never touches an
existing ini.

### D3 — "Usable" means "parses into at least one section"

A file is a usable configuration source when it parses into ≥ 1 section. An
absent, unreadable, or section-less file (empty file, comments only) is
skipped and the next source is tried. Rationale: an `edgeviewer.ini` with no
sections is indistinguishable from no configuration — it makes the plugin
claim nothing — so treating it as "absent" and falling back to the sample is
strictly more useful, and it cannot change the behavior of any ini that has
actual content.

### D4 — Replace, not merge

A user-provided `edgeviewer.ini` replaces the sample wholesale; the sample is
never used to fill in individual missing keys. Rationale: the ini's keys have
"absent" semantics that users rely on (`OfflineMode` absent = network
allowed, `ShowErrorBoxes` absent = silent, `KeepZoom` absent = reset per
file). Merging the sample underneath would silently flip all three for every
user whose ini predates those keys. The sample is a *fallback source*, not an
overlay.

### D5 — The resolution is a separate, testable helper

`ResolveConfigSource(pluginDir)` returns the path of the file the plugin will
parse (or an empty path when there is none). `GlobalSettings()` then reads
that one file. Rationale: the global singleton is initialized once per
process, so the fallback path cannot be exercised by a test that runs after
the ini-seeded T2 fixture has already loaded it. Extracting the decision
makes it testable with a `TempDir` fixture (an ini, an empty ini, a sample,
nothing).

### D6 — The `UserDir` fallback applies to the *resolved* configuration

The existing "no `UserDir` key → plugin directory" fallback is applied after
the source is resolved, so it applies to the sample-derived configuration
too. Because the shipped sample sets `UserDir=%USERPROFILE%`, a read-only
install resolves to a writable web-engine profile directory. (Without the
sample, e.g. a user-supplied ini without a `UserDir` key, the fallback still
points into the plugin directory — see the follow-up in `future-work.md`.)

## Risks / Trade-offs

- [Risk] The plugin now behaves *differently* between a writable install (an
  `edgeviewer.ini` is created) and a read-only one (the sample is parsed
  directly) — the effective configuration is the same, but the files on disk
  differ. → Acceptable: the observable plugin behavior is identical, and the
  user can always create the ini by hand.
- [Risk] A user with an *empty* `edgeviewer.ini` (e.g. created by a script)
  now gets the sample's configuration instead of an inert plugin. → Intended
  (D3); the inert state was never useful.
- [Trade-off] The configuration now has two possible on-disk sources, so
  "which file am I editing?" needs a line in the Readme. → Documented.
- [Trade-off] No hardcoded defaults means a *broken* install (neither file
  present) stays inert rather than claiming file types it cannot render
  (the `assets/` tree would be missing too). → Preferred over a blank
  lister window.

## Migration Plan

None. No file is renamed, moved or rewritten. Users who already have an
`edgeviewer.ini` keep it and it keeps winning over the sample. The only
visible difference is in installs where the plugin could not create the ini —
those now work instead of being inert.
