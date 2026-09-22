## Context

See `proposal.md` — Why. Current state that shapes the approach:

- Every existing format follows "thin C++ processor + vendored JS parser":
  `MhtProcessor`/`EmProcessor` are ~28-line subclasses of `BaseFileProcessor`
  and all parsing lives in `Resources/assets/<type>/`. CHM cannot follow this
  — no browser library decodes LZX/ITSF, so the archive layer must be native.
- `BaseFileProcessor::OpenIn` reads the file, base64-inlines it at
  `__FILE_CONTENT__`, and `NavigateToString`s a loader. That works for a single
  self-contained document but **not** for a CHM, whose topics reference each
  other and their images/CSS by relative path inside the archive.
- Two custom-scheme mechanisms already exist for host-side file serving:
  `evh://` on Windows (per-`WebView2Backend` `WebResourceRequested` handler that
  captures `this`, currently translating `evh://<host>/<rel>` to a mapped
  folder) and `ev://` on Linux (a process-wide `EvSchemeHandler` with a
  host→folder map plus `_close`/`_cmd` bridges). Both are extension points, not
  new plumbing.
- The archive format: `ITSF`/`ITSP` directory, LZX-compressed content blocks,
  a small set of control streams (`/#SYSTEM` metadata + code page, `/#WINDOWS`
  window/home/TOC paths, `/#STRINGS` offset table, `.hhc` table-of-contents
  HTML, optional `/#IDX` and `$FIftiMain`). A prior sibling project
  (`rg-software/wlx-multidoc-viewer`) shipped CHM via libchm and documented the
  traps this design reuses.
- Rules: new deps MUST be pinned in `vcpkg.json`; renderer behavior belongs in
  `Resources/assets/`, not C++.

## Goals / Non-Goals

**Goals:**

- A native archive layer that stays small and behind a narrow interface, added
  to `vcpkg.json` as a pinned dependency.
- Render CHM topics with the real engine (WebView2 / Qt Web Engine) so HTML,
  CSS, images, in-page scripts, text selection, find, and links all work
  natively — no rasterization, no hand-rolled text layer.
- Resolve archive-internal resources without bulk-extracting to disk.
- Keep CHM-specific behavior out of the shared processors.

**Non-Goals:**

- Decoding `$FIftiMain` (whole-archive full-text search) or presenting the
  `.hhk` index. Search is the engine's find over the current topic, like
  HTML/MHT.
- Supporting content that depends on `hhctrl.ocx` / ActiveX help APIs.
- Writing/editing CHM, or building a generic archive browser.
- A per-view manual **Encoding** submenu for CHM (the scheme handler's
  `charset` response is the v1 mechanism; adding the menu is a later change).

## Decisions

### 1. `chmlib` for archive access, via a local overlay port — C++ + vcpkg.json

Add `chmlib` to `vcpkg.json` and link it into the plugin. The builtin vcpkg
`chmlib` port downloads from `jedrea.com`, which now **returns 404**, so we add
a local `overlay-ports/chmlib/` (manifest, `portfile.cmake` pinning the
maintained `jedwing/CHMLib` mirror at a fixed commit, the missing
`CMakeLists.txt`, and `strings_h.patch`) and register `overlay-ports` in
`vcpkg-configuration.json`. On Linux, `CMakeLists.txt` locates the library with
`find_path`/`find_library` (the port ships no package config) and links it.

- *Alternatives considered:*
  - **Hand-rolled LZX/ITSF decoder** — weeks of work and a high-risk
    bit-stream implementation, for a format with a mature library. Rejected.
  - **Windows `ITStorage`/`itsst.dll` COM** — zero new dep but Windows-only
    and dependent on a deprecated IE-era API, breaking the cross-platform goal.
    Rejected.
  - **Shell out to `hh.exe`/`7z`** — external dependency, not embeddable.
    Rejected.
- *Licensing:* `chmlib` is `LGPL-2.1-or-later`, `ONLY_STATIC_LIBRARY`; statically
  linking it into the plugin is accepted (the same author already did this in
  the sibling project). Recorded in the proposal, not a code concern.

### 2. Archive-backed custom scheme, not temp extraction — C++, not assets

Serve topic HTML and every referenced resource through the existing scheme
mechanism, extended with an **archive source**. Each open CHM gets a process-
unique instance id; the loader references resources as
`<scheme>://chm.example/<instance>/<entry-path>`. The scheme handler resolves
`<instance>` to the live archive handle and answers with the entry's bytes and
a MIME type guessed from the entry path.

- Windows: the existing per-backend `evh://` handler gains a branch — when the
  authority is `chm.example`, read from the backend's registry instead of a
  mapped folder.
- Linux: the global `EvSchemeHandler` gains the same branch against a
  process-wide `instance id → archive` registry (the registry, not the view,
  owns the lifetime; removed on `ListCloseWindow`).
- The instance id (not the file path) is what appears in URLs, so two views of
  the same or different CHMs never collide and no file path leaks into URLs.

- *Alternatives considered:*
  - **Extract all entries to a temp dir, then `RegisterVirtualHost`** — simplest
    integration and relative links "just work", but pays extraction cost on
    every open, writes archive contents to disk, and adds a temp lifecycle. The
    user asked for archive-backed **ideally**, so this is the fallback if the
    scheme route proves problematic.
  - **Inline every entry as a data URL** — explodes size, breaks relative links
    and scripts. Rejected.
- *Effect:* lazy reads, no disk footprint, no bulk cleanup; links and anchors
  work because the topic URL is a real same-origin URL.

### 3. Sidebar TOC: parse `.hhc` in JS — static assets, not C++

The host resolves the TOC path from `/#SYSTEM` (falling back to `/#WINDOWS`),
exposes it to the loader (placeholders), and the loader fetches the `.hhc`
entry through the scheme and builds a collapsible tree in the DOM.
`.hhc` is nested `<ul>` with `<OBJECT type="text/sitemap">` blocks carrying
`<param name="Name"|"Local">`; a tolerant, case-insensitive scanner suffices
(the sibling proved this — no Gumbo/DOM dependency).

- Fetching uses **XHR, not `fetch()`**: on Linux, Chromium's fetch allowlist
  ignores custom schemes and the `ev://` handler is never invoked (project-wide
  constraint — see `rendering-pipeline.md`); the RST and Markdown loaders
  already use an XHR-based `evFetch` for exactly this reason.
- *Alternatives considered:* parse `.hhc` in C++ and inject a JSON tree —
  pushes renderer behavior into C++, against the project rule, and gives no
  benefit. Rejected.

### 4. Two-pane loader with the topic in a content frame — static assets

`Resources/assets/chm/loader.html` renders a sidebar pane and a content pane;
the content pane is an `<iframe>` whose `src` is the default topic URL in the
archive scheme. TOC clicks set the frame's URL; in-topic links (relative or
`#anchor`) resolve natively inside the frame and need no rewriting. `ms-its:` /
`mk:@MSITStore:` absolute forms are rewritten to the scheme URL before display.

- *Alternatives considered:* single navigable document that re-injects the
  sidebar on every navigation — loses sidebar scroll/expansion state and needs
  link rewriting on every load. Rejected.
- An iframe keeps the sidebar persistent and makes same-origin resource
  resolution the engine's job.

### 5. Charset: resolve from the archive, serve an explicit `charset` — C++

`chmlib`/`#SYSTEM` expose the help file's language and code page. The scheme
handler emits `Content-Type: text/html; charset=<mapped codepage>` for text
entries, with the engine's own `<meta>`/BOM sniffing as the fallback when the
code page is unknown. This covers the common non-Latin CHMs (e.g. Windows-1251
Russian help) without a new decode path.

- The declared code page comes from an LCID→codepage table (a fixed
  ~70-entry table, mirroring the sibling's implementation).
- *Alternatives considered:* reuse the page-side `charset-autodetect` +
  `encoding-override` machinery — valuable but more surface than v1 needs, and
  a per-view manual Encoding menu is explicitly a non-goal here.

### 6. Windows narrow-path staging — C++

`chmlib`'s `chm_open` takes a narrow (`char*`) path on Windows, so a CHM whose
*file name* is not representable in the system code page cannot be opened
directly. When the path is not ANSI-representable, copy it to a tracked temp
file (reusing the existing `GenTempFile` / temp-tracking and `CleanupOnExit`
machinery) and open that. Paths that are representable are opened in place.

- *Alternatives considered:* patch chmlib in the overlay port to add a wide
  open — new patch surface and divergence from upstream. Rejected for v1.

### 7. Processor shape stays thin — C++

`ChmProcessor` mirrors `MhtProcessor` (namespace-scope self-registration,
`InitPath` matches the `CHM` token). It does **not** inherit
`BaseFileProcessor`, because CHM is not a single inlined document: it opens the
archive, resolves the home topic and TOC path, registers the archive instance,
and navigates the loader. The archive wrapper (`ChmArchive`) is the only new
non-trivial C++ and stays behind a small interface (open/close, metadata,
enumerate, retrieve).

### 8. Wiring

- `Resources/edgeviewer.ini`: `[Extensions] CHM=CHM`, new `[CHM] CSS`/`CSSDark`.
- `EdgeViewer/WlxDetect.cpp`: add `"CHM"` to the section order list.
- `EdgeViewer/EdgeViewer.vcxproj` (+ `.filters`): register the new sources.
- `CMakeLists.txt`: add the new shared sources and the `chmlib` link/`find_*`.
- `vcpkg.json` + `overlay-ports/chmlib/*` + `vcpkg-configuration.json`.

## Risks / Trade-offs

- **[Risk] The builtin `chmlib` port is broken (404) and the overlay is
  project-maintained.** → Pinned commit + SHA512 in the overlay; the port is a
  few files and rarely changes.
- **[Risk] LGPL static linking.** → Accepted by the project (precedent in the
  sibling repo); only `chmlib`'s compiled objects are linked, source is
  upstream.
- **[Risk] Custom-scheme iframes/subresources may be blocked by Chromium on
  Linux** (the same class of restriction that broke `fetch()` on `ev://`). → A
  spike must confirm `<iframe src="ev://...">`, XHR to `ev://chm.example`, and
  `<img>`/`<link>` subresource loads before committing to Decision 2; if iframe
  navigation fails, fall back to temp extraction (Decision 2 alternative) or a
  single-document rewrite strategy.
- **[Risk] Non-Latin CHMs render as mojibake when the code page is unknown.** →
  The charset response covers declared cases; unknown cases fall back to engine
  sniffing and are documented, with the Encoding submenu queued as future work.
- **[Risk] Exotic `.hhc` markup defeats the scanner.** → Unparseable TOC yields
  an empty sidebar and the home topic still renders; the tree is a progressive
  enhancement, never a hard failure.
- **[Risk] Malformed/truncated archives.** → `chm_open` failure or a bad
  `#SYSTEM` produces an in-viewer error page and a failed load, never a crash.
- **[Risk] Detect-string 260-char budget.** → Verify `BuildDetectString` still
  fits after adding `EXT="CHM"` and surface truncation through the existing
  warning path.

## Migration Plan

1. Add the overlay port + `vcpkg.json` + CMake/vcxproj wiring; confirm both
   platforms build and link `chmlib`.
2. Implement `ChmArchive` (open/close, `#SYSTEM`/`#WINDOWS`, enumerate,
   retrieve, code page) and unit-testable pure helpers in `EdgeViewer.Tests`.
3. **Spike the scheme mechanism** on Linux (iframe + XHR + subresources over
   `ev://`); pick Decision 2 or its temp-extraction fallback based on the
   result, and record the outcome in this design.
4. Implement the archive-source branch in both backends + the instance
   registry and lifecycle.
5. Implement `ChmProcessor`, the loader, and the JS sidebar; wire ini, detect
   string, and dark mode.
6. Build Release for Win32 and x64 (and the Linux build), then verify manually
   in TC/DC against representative CHMs.

Rollback: remove the new sources/assets and the two wiring lines; `chmlib` and
the overlay port are additive and can be dropped from `vcpkg.json` independently.

## Open Questions

- Whether Chromium on Linux permits an iframe to navigate to the custom scheme
  (Decision 2) — resolved by the task 3 spike; if not, use temp extraction.
- Whether `chmlib`'s Windows build needs the temp staging for *any* non-ASCII
  path or only for names outside the ANSI code page — resolved empirically in
  task 2 with a non-ASCII fixture.
