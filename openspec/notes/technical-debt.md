# Technical debt backlog

Technical debt is tracked here so it can be deliberately scheduled instead of surfacing as
unfocused branch-wide refactoring. Each item is a bounded unit of work with a status.
Items already tracked in the OpenSpec pipeline (`openspec/notes/future-work.md`, archived
change records) are cross-referenced rather than duplicated.

Status legend: `open` (not addressed) / `in-progress` / `done` / `won't-fix`.

---

## C++ layer (EdgeViewer/)

### TDB-01 — Hardcoded type-section list in `WlxDetect.cpp`
- **Status:** open · **Priority:** medium
- `WlxDetect.cpp:12` `BuildDetectString` keeps a hardcoded
  `static const std::vector<std::string> secs = { "HTML", "Markdown", ... }` that must stay
  in sync with the `[Extensions]` section of `Resources/edgeviewer.ini` **and** the registry of
  processor `InitPath` matchers (`DllMain.cpp`, `ProcessorRegistry.h`). Adding or removing a
  file type requires touching three places; nothing enforces the correspondence.
- **Direction:** derive the section list from the registry (each processor already declares its
  `cssSection()`), or assert in a debug/tests build that every `[Extensions]` key has a matching
  source section. Not urgent — only fails silently when a new type is mis-registered.

### TDB-02 — Dead `wcsicmp()` in `ProcessorInterface.cpp`
- **Status:** open · **Priority:** low
- `EdgeViewer/Processors/ProcessorInterface.cpp:38` defines a cross-platform
  `int wcsicmp(const std::wstring&, const std::wstring&)` that is never called anywhere.
  It is a leftover from the Win32 `_wcsicmp` port.
- **Direction:** delete the function (grep confirms zero call sites).

### TDB-03 — Duplicate null-check in `EdgeLister_Linux.cpp`
- **Status:** open · **Priority:** low
- `EdgeViewer/EdgeLister_Linux.cpp:169-170` repeats
  `if (!impl->backend) { delete impl; return nullptr; }` twice (copy-paste). The second is dead.
- **Direction:** remove one occurrence.

### TDB-04 — Dual delivery contract: `filenamePlaceholder()` + pre-fetch
- **Status:** open · **Priority:** medium
- `BaseFileProcessor` carries **both** the base64/`__FILE_CONTENT__` pre-fetch path
  **(the primary path)** and a per-subclass `filenamePlaceholder()` getter feeding a parallel
  `{{`**`__{TYPE}_FILENAME__`**`}}` → `urlPathW(...)` substitution and a loader-side `fetch()`
  fallback. The pre-fetch always fills `__FILE_CONTENT__` on both builds, so the fetch branch is
  effectively dead, but every one of the 5 text loaders (markdown/rst/asciidoctor/mhtml/eml)
  still ships both code paths and every processor declares the extra getter.
- **Direction:** if the fetch fallback is truly unreachable, drop `filenamePlaceholder()`,
  the `urlPathW(relative_path())` substitution, and the loader `fetch(...)` branches to remove
  the parallel contract. Confirm the `imgview` exclusion (uses `<img src>` directly) stays.

### TDB-05 — Per-processor three-getter boilerplate
- **Status:** won't-triage · **Priority:** low
- Each of the 5 (soon 6) `BaseFileProcessor` subclasses repeats the identical
  `cssSection()` / `loaderDirectory()` / `filenamePlaceholder()` static-string shape
  (`MdProcessor.h:11-25`, etc.). This is the documented, intentional design (a base class that
  owns `OpenIn`) so it is **not** treated as debt worth restructuring; the churn/risk of a
  CRTP/templated base outweighs the saving.

### TDB-06 — `ZoomHotkey.h` leaks `<windows.h>` into a shared header
- **Status:** open · **Priority:** low
- `ZoomHotkey.h:3` includes `<windows.h>` for `UINT`/`VK_*`, but the function is Windows-only in
  practice (Linux routes imgview zoom through `CMD_ZOOM` → `QWebEnginePage::setZoomFactor` and
  never compiles `ZoomHotkeyHandled`). A shared header that drags in a Win32 type looks
  cross-platform but is not.
- **Direction:** move `ZoomHotkey.{h,cpp}` into a Windows-only translation unit or define the
  key constants without `<windows.h>`, and document the Linux zoom path explicitly.

### TDB-07 — Nested async `WebViewFactory` callbacks / duplicated `put_Bounds`
- **Status:** open · **Priority:** low
- `WebViewFactory.cpp` sets controller bounds twice (line ~198 in the callback and again
  ~215-221 after building the backend). The second is the authoritative one (guards zero-size);
  the first is redundant. The whole setup is one deeply nested two-level completion-callback
  chain, which is the correct WebView2 shape but is hard to read/extend.
- **Direction:** drop the first `put_Bounds`; optionally extract the inner controller-completion
  body into a named helper for readability. Do not flatten the async structure.

### TDB-08 — Missing guard around `try_query` COM casts
- **Status:** open · **Priority:** low · **robustness**
- `WebViewFactory.cpp`: `SetColorProfile` (`wv13`, line 25), `DisableBrowserHotkeys`
  (`settings23`, line 35) call `.try_query<...>()` and use the result with no null check.
  `WebView2Backend.cpp:36` does the same for `webview23` in `RegisterVirtualHost`. A failed
  qualitys check yields a null COM pointer and a late crash on older WebView2 runtimes.
- **Direction:** null-guard the `try_query` results (drop into the no-op/fallback branch) like
  the existing `if (Settings)` guards.

### TDB-09 — `ListSendCommand` is a non-functional stub
- **Status:** won't-triage · **Priority:** low
- Both platforms' `ListSendCommand` returns `0` without acting. This is an inheritance of the
  original master behavior; TC does not rely on it for the documented feature set. If a future
  feature needs a TC command channel, revisit then.

### TDB-10 — `BuildPrintScript()` is production-dead (test-only extraction)
- **Status:** open · **Priority:** low
- `Navigator.cpp:37` defines `BuildPrintScript()` but `Navigator::Print()` calls
  `mWebView.Print()` (the `IWebView` default → `ExecuteScript("window.print()")`) instead, so the
  free function is exercised only by the tier-4 unit test (`tier4_extractions.cpp:171`). It
  exists to satisfy the "pure extraction" spec (`test-harness`), which is a reasonable reason to
  keep it — but it is production-dead and worth a note / a decision to use it in `Print()`.

### TDB-11 — `gs_Views` locking is inconsistent across platforms
- **Status:** open · **Priority:** medium-low
- `gs_Views` is a process-global `std::map` protected by `g_viewCreateLock` in
  `WebViewFactory.cpp` but by a separate `g_viewsMutex` in `EdgeLister_Linux.cpp`; the Win32
  `EdgeLister_Win.cpp` / `DllMain.cpp` `FindBackend` reads it lock-free. Decision 7 makes all
  WLX callbacks same-thread, so the mutex is belt-and-braces — but the discipline is not uniform
  across the two backends, which is a latent inconsistency for any future multi-threaded caller.
- **Direction:** centralize the accessor (a single lock-bound `FindBackend`/`EraseBackend`) and
  route both platforms through it.

### TDB-12 — `DllMain.cpp` dual-chunk `#ifdef _WIN32` export sets
- **File:** `EdgeViewer/DllMain.cpp` (lines 64-361)
- **Status:** open · **Priority:** medium
- The file carries two parallel WLX export implementations (Win32 → `HWND`+WM_COPYDATA era,
  Linux → `extern "C"` + `FromDcWide`) guarded only by `#ifdef`. `ComputeDarkMode` and
  `FromDcWide` live inline in the Linux chunk. The two halves have already drifted
  (Linux dark-mode samples `QGuiApplication`, Windows samples `lcp_darkmode`; Windows list
  search rebuilt the parse inline, Linux passes through). This is the intentional single-file
  contract but is easy to drift further.
- **Level:** not a blocker; keep the WLX contract in one file but consider extracting
  `ComputeDarkMode`/`FromDcWide` into `Platform_Linux.cpp` so the shared file only holds export
  plumbing.

## Renderer / assets layer (Resources/assets/)

### TDB-13 — MIME map is an inline `if/else` chain in `QtWebEngineBackend.cpp`
- **File:** `EdgeViewer/WebView/QtWebEngineBackend.cpp:252-262`
- **Status:** open · **Priority:** low
- `EvSchemeHandler` maps extensions to MIME types with a growing if/else chain. Every new
  deliverable file type adds a branch here.
- **Direction:** move to a static `map<std::string,std::string>` keyed by extension (with an
  explicit fallback default) so it is data, not control flow.

### TDB-14 — Fragile `http://` → `ev://` string rewrite in `NavigateToString`
- **File:** `EdgeViewer/WebView/QtWebEngineBackend.cpp:406-418`
- **Status:** open · **Priority:** low
- `NavigateToString` text-replaces every `http://` in the loader HTML (including inside JS
  strings) so the Linux `ev://` scheme handles the assets host. It works because base64 content
  and current loaders never embed a literal `http://` in a way that breaks, but it is
  correctness-by-assumption over a whole page payload.
- **Direction:** keep it (decision documented, `design.md` Decision 3 Fallback A), but prefer
  rewriting only the known `assets.example`/`local.example` host patterns in `Navigate()`, and
  document the invariant.

### TDB-15 — `imgview` excluded from the pre-fetch path
- **File:** `Resources/assets/imgview/loader.html` (+ `BaseFileProcessor`)
- **Status:** open · **Priority:** low · — flagged in the port notes as deliberate
  (`imgview` uses `<img src>` directly, never JS `fetch`). Keep as a conscious exclusion; add a
  comment in `BaseFileProcessor.h` naming it so the next reader doesn't re-add it.

### TDB-16 — Vendored assets have no manifest / version pin
- **Files:** `Resources/assets/**/*.min.js`, `detect-charset.js`, `asciidoctor`, `mermaid`,
  `mathjax`, `postal-mime`, `thumbnailViewer`, `mhtml2html`
- **Status:** open · **Priority:** low
- Third-party JS/CSS (marked, asciidoctor, mermaid, mathjax, highlight.js, detect-charset,
  thumbnailViewer) is vendored without a central manifest of source URL + version. Upgrading or
  tracing a regression requires digging through minified files.
- **Direction:** add a `Resources/assets/VENDORED.md` (or manifest) listing origin + version per
  vendored file. Non-functional, pure-provenance.

---

## Out of scan scope / cross-reference

Items already tracked in `openspec/notes/future-work.md` and not re-listed here:
`[HTML] DetectEncoding` override removal, Wayland Ctrl+Q jump, Qt WebEngine process overhead,
Linux dir-thumbnails / right-click / per-processor zoom. Those live in the OpenSpec pipeline,
not this backlog.