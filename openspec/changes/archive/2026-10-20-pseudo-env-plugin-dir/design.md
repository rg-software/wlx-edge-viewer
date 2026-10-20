# Design

## Context

`ExpandEnv` is the single choke point for resolving `%ENV%` placeholders in
ini values: `Platform_Win.cpp` (via `ExpandEnvironmentStringsW`) and
`Platform_Linux.cpp` (via `$VAR`/`${VAR}` getenv). Path-valued consumers
(`UserDir`, `BrowserExecutable*XFolder`, EML save default folder) all call
it. The plugin directory is available through `GetModulePath()` (already
used in `Globals.cpp` to default `UserDir`). No external dependency is
involved.

## Goals / Non-Goals

**Goals:**
- Resolve `%PluginDir%`/`$PluginDir` to `GetModulePath()` in every value
  that passes through `ExpandEnv`.
- Resolve `%UserDir%`/`$UserDir` to the configured UserDir (after its own
  expansion), falling back to the plugin directory when unset.
- Prevent infinite expansion on self-referential values.

**Non-Goals:**
- No new ini keys or sections; no changes to the mINI parsing or caching.
- No case-insensitive matching of pseudo-variable names beyond what the OS
  already provides for real env vars (Windows `ExpandEnvironmentStringsW`
  is case-insensitive; Linux getenv is case-sensitive, so pseudo vars
  there follow the same case-sensitive rule as real vars).
- No general macro/templating engine beyond the two named pseudo-vars.

## Decisions

- **Where to inject pseudo-vars.** Resolve them inside `ExpandEnv` before
  delegating to the OS expander. This keeps every consumer (UserDir,
  BrowserExecutable folders, PickFolder) covered without touching each call
  site. Alternative considered: resolving only in `GlobalSettings()` at
  parse time, but that would bake expanded values into the cached ini and
  break the "ini is read-only at runtime" contract and the
  case-insensitive OS expansion semantics for real env vars.

- **Order of resolution.** First substitute `%PluginDir%`/`$PluginDir`
  (a constant, no ini dependency), then `%UserDir%`/`$UserDir` using the
  configured (and already `%PluginDir%`-expanded) UserDir, then let the OS
  expander handle real env vars. This ordering lets `UserDir=%PluginDir%\Profile`
  and `BrowserExecutableX64Folder=%UserDir%\Edge` chain correctly.

- **Recursion guard.** Run the pseudo-substitution in a bounded loop
  (max 4 passes) and stop when a pass produces no change. A
  self-referential value (`UserDir=%UserDir%`) therefore stabilizes after
  the first pass (no net change) and is left as-is. The raw UserDir is
  read from the cached ini (with its own `%PluginDir%` already expanded
  conceptually by re-running expansion on it), not from a pre-expanded
  cache, to avoid stale values.

- **Linux form.** `$PluginDir`/`${PluginDir}` and `$UserDir`/`${UserDir}`
  are added to the existing `$VAR`/`${VAR}` scanner in
  `Platform_Linux.cpp`, checked before falling through to `getenv`, so an
  unset real env var of the same name cannot shadow the pseudo-var.

## Risks / Trade-offs

- [Risk] `%UserDir%` resolution reads `GlobalSettings()["WebView"]["UserDir"]`,
  which itself may be `%UserDir%` → handled by the bounded loop and left
  unexpanded. → Mitigation: the loop cap plus "stop on no change".
- [Risk] `GetModulePath()` on Linux is the `.wlx64` location, matching
  how the plugin resolves `assets/` and `edgeviewer.ini`. → Consistent
  with existing code paths.
- [Trade-off] Pseudo-vars are case-sensitive on Linux (matching real env
  vars there) but the Windows OS expander makes real env vars
  case-insensitive; pseudo-vars on Windows are matched case-insensitively
  by `ExpandEnvironmentStringsW` only if we leave them for the OS, but we
  substitute them ourselves before. We will match the literal spellings
  `%PluginDir%`/`%UserDir%` plus their common casings is out of scope;
  documented as case-sensitive to match the simplest implementation. Actually
  to be safe, do a case-insensitive replace on Windows for the pseudo-vars
  to match user expectations of Windows env vars. (Decision: case-insensitive
  match on Windows, case-sensitive on Linux, matching each platform's native
  env-var semantics.)

## Migration Plan

No migration needed: existing ini files keep working; the new variables are
opt-in. Shipped template gains a commented example.

## Open Questions

None.