# Manual testing checklist

Human verification matrix for the parts of the plugin that the autotests do not
reach. The Catch2 suite (`EdgeViewer.Tests`, see `EdgeViewer.Tests/readme.md`)
covers only pure/mock-testable units — character conversion, ini parsing, path
helpers, `window.find()`/zoom/print script builders, the `BaseFileProcessor`
loader substitution and the `OfflineMode` URI classifier. **Every row below
requires a real file manager + a real web engine**, so it must be exercised by
hand against a freshly built plugin.

Each row records what to do, the expected result, and (where the platforms
diverge) a **Win** / **Linux** note. Most rows reference a fixture under the
`Examples/` folder that ships with the repo.

- **Windows**: Total Commander (32-bit `EdgeViewer.wlx` **and** 64-bit
  `EdgeViewer.wlx64`); WebView2 runtime required.
- **Linux**: Double Commander (64-bit `EdgeViewer.wlx64`); Qt 6 WebEngine.

Unless stated otherwise, a row SHALL pass on both platforms, and (on Windows)
on both the 32-bit and 64-bit builds. Where the 32/64-bit behaviour is expected
to be identical, run the row once per bitness on Windows.

---

## 1. Core rendering (per format)

Autotests verify the processor → `IWebView` call sequence and loader template
substitution, but NOT the actual rendered pixels. Verify each format actually
displays a correct document.

| # | Action (F3 the file) | Expected | Win | Linux |
|---|----------------------|----------|-----|-------|
| 1.1 | `Examples/tutorial #1.md` | Headings, fenced code blocks highlighted by highlight.js, GitHub-flavoured tables via marked.js, in-viewer same-folder navigation UI if `[Markdown] NavigationUI=1` | covers link nav debug; keep `NavigationUI` at shipped `0` for baseline | same |
| 1.2 | A `.md` with a fenced code block containing `##`-style headings | `highlight.js` does not mis-parse the code as markdown headings | | |
| 1.3 | `Examples/asciidoc.adoc` | Rendered AsciiDoc (Asciidoctor.js) | | |
| 1.4 | `Examples/ReStructuredText.rst` | Rendered RST (rst-compiler) with any in-file `.. _anchor:` links, KaTeX maths, and a link to a sibling `.rst` | | |
| 1.5 | `Examples/RstNavTarget.rst` | The sibling-navigation target from 1.4 opens | | |
| 1.6 | `Examples/encoding-windows1251.html` | Cyrillic **auto-detected** to windows-1251 (see §5 for the full encoding matrix) | | |
| 1.7 | `Examples/Ôàéë-hoedown.html` | Renders; check the declared charset is honoured (see §5.6) | | |
| 1.8 | `Examples/sample.xhtml` | Rendered as a styled HTML page (red `<h1>`, sans-serif body) — **not** an XML tree | served via temp-copy + custom scheme handler | rendered as HTML via `ev://` default MIME (no temp copy) |
| 1.9 | `Examples/sample.xml` | Rendered as HTML, per `[Extensions] ForcedHtmlExt=xml\|xhtml` | | |
| 1.10 | `Examples/fileformatinfo.mht` | Rendered MHT (mhtml2html), layout preserved | | |
| 1.11 | `Examples/encoding-wrong-charset.mht` | Cyrillic auto-corrected over the transfer-decoded payload (see §5.3) | | |
| 1.12 | `Examples/multipart-sample.eml` | EML body rendered (postal-mime); HTML part preferred over plain | | |
| 1.13 | `Examples/encoding-windows1251.eml` | Cyrillic EML body decodes correctly (see §5.5) | | |
| 1.14 | `Examples/google.url` | Browser-safe: generic `.url` targeting a public site; on a network-restricted host the page may not load — the **plugin contract** is "navigates to the URL line, not blank/error of its own doing". See Row 5.7 for the offline interaction | | **Known broken**: Chromium sandbox/DNS/TLS can leave a blank page (see `linux-parity` spec §URL) |
| 1.15 | `Examples/sample-local.url` | Navigates to the local file target | | |
| 1.16 | `Examples/sample.pdf` | Chromium's built-in PDF viewer chrome + rendered first page | | `ev://` MIME map serves `.pdf` as `application/pdf` → same built-in viewer |
| 1.17 | A `.docx`, `.xlsx`, `.odt`, `.epub`, `.zip` | Chromium attempts native treatment (viewer/download) rather than raw binary | | `ev://` MIME map now maps these; verify native activation |

## 2. Images (`imgview`)

| # | Action | Expected | Win | Linux |
|---|--------|----------|-----|-------|
| 2.1 | F3 `Examples/Genaille_division_rods.svg` | SVG renders via thumbnail-viewer, zoom + pan work, `[Images] FitToScreen=1` starts it fitted | | |
| 2.2 | A large `.png`/`.jpg` | Renders fitted; wheel / buttons zoom and pan | | |
| 2.3 | Press `F` while the image has focus | Toggles between `full-screen` and `real-size`, host zoom synchronizes | | **Known limitation**: DC intercepts `F`/`Shift+F` in single-image F3 — toggle never fires; verify **directory** view `F` instead (Row 3.7) |
| 2.4 | Press `Esc` after toggling fullscreen (Windows) | Lister closes, focus back to file panel | | **Linux**: DC intercepts `F` so fullscreen is unreachable here; `Esc` alone still closes §6 |
| 2.5 | Zoom the image then open another image | `[WebView] KeepZoom` restores zoom across files of the same format | | per-origin zoom, not per-processor (see Row 9.3) |

## 3. Directory view (`dirviewer`)

| # | Action | Expected | Win | Linux |
|---|--------|----------|-----|-------|
| 3.1 | F3 a folder with subdirectories and `Examples/images/*.jpg` + a `.txt` | Subdirs listed before files, alphabetical; image files shown as clickable image thumbnails; `.txt` uses the static `file.png`; a `.docx` (matches neither `DirImageExt` nor `DirOtherExt`) is hidden | | |
| 3.2 | Click an image thumbnail | That image opens in the Lister | | |
| 3.3 | `Ctrl+H` | Toggles `ShowNames` (hide/unhide names) | | |
| 3.4 | `Ctrl+G` | Toggles `ShowFolders` (hide/unhide folders) | | |
| 3.5 | `F` | Toggles `FitToScreen` thumbnail sizing | | **works here** (DC does not intercept in directory view) |
| 3.6 | `PageUp`/`PageDown`/`Left`/`Right` | Navigate between thumbnail tiles | | |
| 3.7 | Open a folder containing several `.jpg` with `[Directory] GenDirThumbs=1` | Live shell thumbnail for image files; folder entries get a shell folder thumbnail (256 px) | real shell thumbnails | **No dynamic thumbnails** — `GenDirThumbs` ignored; static `folder.png`/`file.png` always used (future-work #2) |
| 3.8 | A path like `\\server\share\dir` | Directory is **not** rendered by the directory processor (UNC unsupported — documented) | | |
| 3.9 | `[Directory] TruncateNames=1`, `NamesUnderThumbnails=1` | Names are truncated to fit and sit under each thumbnail | | |

## 4. HTML family specifics

| # | Action | Expected | Win | Linux |
|---|--------|----------|-----|-------|
| 4.1 | F3 a plain `.html` with relative links to sibling files | Relative links resolve within the same folder | | |
| 4.2 | F3 an `.html` declaring `<meta charset>` with non-ASCII | Renders correctly (real navigation, no byte-mapping) | | |
| 4.3 | Set `[HTML] CSSDark=style-dark.css`, reopen an `.html` in dark mode | Lister-injected stylesheet applies page chrome; light mode shows `none.css` | | dark mode follows the system scheme, not a DC flag |
| 4.4 | Browser hotkeys on an HTML view: `Ctrl+F`, `F5`, `F12` | Do **not** trigger the browser's own find/reload/devtools | engine accelerators disabled | Qt WebEngine focus handling applies — verify Ctrl+F doesn't open WebEngine find |

## 5. Encoding / charset (HTML, MHT, EML)

The autotests only cover `TranscodeBytes` for a handful of labels (Win32-only
`charset_override.cpp`); **all** of the menu/detection/round-trip behaviour here
is manual.

| # | Action | Expected | Win | Linux |
|---|--------|----------|-----|-------|
| 5.1 | F3 `Examples/encoding-windows1251.html` (no declaration) | jschardet corrects to windows-1251 (no mojibake); Encoding submenu checks "Auto: windows-1251" | host-side `MultiByteToWideChar` | host-side `QTextCodec` |
| 5.2 | F3 `Examples/encoding-windows1251-wrongmeta.html` (declares utf-8) | With shipped `[HTML] ForceDetectEncoding=1`, still auto-corrected to windows-1251; with `ForceDetectEncoding=0`, left at the wrong utf-8 decode | | |
| 5.3 | F3 `Examples/encoding-wrong-charset.mht` | Auto-corrected to windows-1251 over the transfer-decoded payload; "Auto: windows-1251" checked | | |
| 5.4 | Right-click an HTML or MHT view → **Encoding** submenu | Submenu present with "Auto-detect" + curated code pages; picking one re-renders; submenu **absent** on markdown/image/dir/PDF views | | |
| 5.5 | F3 `Examples/encoding-windows1251.eml` | Cyrillic body decodes (single-byte label via `assets/charset/singlebyte.js` tables) | | same tables (avoids Qt ICU `TextDecoder` freeze) |
| 5.6 | F3 `Examples/Ôàéë-hoedown.html` (declared charset) | Renders per its declared charset; auto-detect agrees so no re-render | | |
| 5.7 | With `[WebView] OfflineMode=1`, F3 an `.html` embedding a remote image | Remote image blocked (renders as broken/empty), local content still loads | WebResourceRequested 403 | `QWebEngineUrlRequestInterceptor` profile-level |
| 5.8 | Pick an unappliable encoding ("UTF-16LE") on a UTF-8 `.html` | Pick fails, view reverts to engine sniffing, menu re-arms to "Auto:" state — never blank | | |
| 5.9 | Pick an unappliable encoding on an MHT view | Previous render kept, `CMD_ENCODING_APPLY_FAILED` → re-runs detector, Auto-detect re-checked | | **Known issue**: host round-trip MHT re-decode can freeze the renderer on Linux (`future-work` #10); verify the page-side detect-first path renders instead |

## 6. ESC / close / window lifecycle

| # | Action | Expected | Win | Linux |
|---|--------|----------|-----|-------|
| 6.1 | F3 any file, press `Esc` | Lister closes, file panel regains focus | TC posts `WM_CLOSE` to the lister HWND | JS bridge → `ev://_close/<id>` → synthetic `Q` → DC `cm_ExitViewer` |
| 6.2 | Open several files in sequence (F3 → navigation, or Quick View) then close | No leftover plugin windows, no crash, `gs_Views` fully cleaned up | | |
| 6.3 | Set `[WebView] CleanupOnExit=1`, close DC | Browser cache / temp files removed on unload | keyboard short-cut: verify | Windows-only key |
| 6.4 | F3 one file, then F3 a second into the same view (`ListLoadNext`) | Re-renders in place, no window churn | | |

## 7. Text search / find (invoked from the host's search dialog)

Autotests verify the *script builders* (`BuildFindScript`); the actual
`window.find()` interaction in a rendered doc is manual.

| # | Action | Expected | Win | Linux |
|---|--------|----------|-----|-------|
| 7.1 | Search forward for a word in a rendered doc | Highlight/scroll to first match | TC search dialog → `ListSearchTextW` → `window.find` | DC search dialog (verify DC wires Lister search) |
| 7.2 | Case-sensitive, whole-word, backwards search | Respects the flags | | |
| 7.3 | "Find first" (reset to beginning) | Repeats backwards until start, next search starts at document top | | |

## 8. Printing

Autotests only assert `BuildPrintScript()`; the actual print dialog is manual.

| # | Action | Expected | Win | Linux |
|---|--------|----------|-----|-------|
| 8.1 | Print the current view from the host | Browser print dialog / page renders to printer for the *current* document only | TC print command (`ListPrintW`) | DC print command |
| 8.2 | Verify only the loaded document prints (not other fixtures) | Print output matches the active view | | |

## 9. Zoom

Autotests cover `ZoomHotkeyHandled` (the pure key→factor math) — **not** the
host wiring or persistence.

| # | Action | Expected | Win | Linux |
|---|--------|----------|-----|-------|
| 9.1 | `Ctrl`+`+`, `Ctrl`+`-`, `Ctrl`+`0` (and numpad `+ - 0`) in a rendered doc | Zoom in/out/reset through fixed steps 0.25–5.0 | | `Ctrl`+wheel / zoom via `CMD_ZOOM` bridge; verify all three key forms |
| 9.2 | `Ctrl`+wheel | Zoom changes by fixed steps | | |
| 9.3 | `[WebView] KeepZoom=1`: zoom doc A to ~150%, open doc B (same format), reopen A | A's zoom restored to 150% | per-processor | **Known**: per-origin, so the zoom is shared across all file types, not isolated per processor (future-work #4) |
| 9.4 | Zoom hotkeys are NOT forwarded to the file manager | Zoom does not also trigger the file manager's own zoom | keys consumed by plugin | Qt WebEngine handles its own focus keys |

## 10. Context menus

| # | Action | Expected | Win | Linux |
|---|--------|----------|-----|-------|
| 10.1 | Right-click a file entry in an **HTTP/HTML** view, a **Markdown** view, an **image** view | Standard web context menu (plus Encoding submenu on HTML/MHT per §5.4); **no** Explorer shell menu | | |
| 10.2 | Right-click inside the **directory** view | Explorer shell context menu for the clicked entry (Open/Copy/Properties etc.); selecting a verb acts on the file; `Esc`/outside-click cancels with no action | real `SH*`/COM menu (Win + x64) | **No popup on Linux** — right-click does nothing plugin-defined (future-work #3) |

## 11. Configuration parsing (option toggles)

The autotests read the keys; only the behavioural effect of each toggle is
manual.

| # | Action | Expected | Win | Linux |
|---|--------|----------|-----|-------|
| 11.1 | Add `[WebView] Switches=--disable-smooth-scrolling` | Flag is applied to the engine (compare against unset) | | |
| 11.2 | Set `--disable-gpu` (or `QT_WEBENGINE_CHROMIUM_FLAGS`) | Renders with GPU disabled, no visual regression | | Linux: also `QT_QUICK_BACKEND=software` avoids the Wayland Ctrl+Q jump (Row 12.1) |
| 11.3 | Change `[Markdown] CSS` to another bundled stylesheet | Next markdown load uses the new theme | | |
| 11.4 | Toggle `[Images] FitToScreen` `1`↔`0` | Image opens fitted vs real-size | | |
| 11.5 | Toggle `[Directory] GenDirThumbs` `1`↔`0` | Live vs static icons (Row 3.7) | | Linux: no effect — always static |
| 11.6 | `[Extensions] Dirs=0`, F3 a folder | Folder is **not** claimed (no directory listing) | | |
| 11.7 | Remove `ForcedHtmlExt` (or clear it), F3 `sample.xml` | XML is no longer forced to HTML | | |
| 11.8 | `[WebView] ShowErrorBoxes=0` + break WebView2 (uninstall runtime) | No error dialog on failure | Win only | N/A |

## 12. Linux-only known issues / workarounds

| # | Action | Expected | Win | Linux |
|---|--------|----------|-----|-------|
| 12.1 | First `Ctrl+Q` quick-view of a session under native Wayland | Window may jump to screen centre / DC main window jumps (documented limitation). Workaround: launch `QT_QUICK_BACKEND=software doublecmd` | N/A | affected |
| 12.2 | First `ListLoadW` of a session | Noticeable startup cost (Chromium subprocess spawn) — inherent, not a defect | | |
| 12.3 | Right-click in any view | No plugin popup (Row 10.2) | N/A | affected |

---

## Known-unverifiable / expected-fail rows

None of these are "untestable in principle" — each is a row that **cannot pass
today** on a given platform because of an open bug or a deliberately-deferred
feature. Treat them as *documented expected current behavior* during a manual
pass: verify the failure manifests as described (rather than crashing or
showing an unrelated symptom), then record the result as `expected-fail` rather
than a blocker. Each ties back to `openspec/notes/future-work.md`.

| Row | Why it cannot pass | Expected-current behavior | Tracking |
|-----|--------------------|--------------------------|----------|
| 5.9 | Linux MHT host round-trip re-decode freezes the Qt renderer | MHT auto path should render via the page-side detect-first path (no host round-trip); a full manual-pick re-encode may hang | future-work #10 |
| 2.3 / 2.4 | DC intercepts `F`/`Shift+F` in single-image F3 view | Fullscreen toggle never fires / `Esc`-after-fullscreen unreachable in image view; `F` still works in directory view | future-work #5a; `linux-parity` §image fullscreen |
| 1.14 | Linux external `.url` navigation can blank (Chromium sandbox/DNS/TLS) | Page may stay blank; verify `dc.stderr` surfaces the cause, not a plugin crash | `linux-parity` §URL; future-work none |
| 3.7 / 11.5 | Linux `GenDirThumbs` unimplemented | Static `folder.png`/`file.png` always used; key ignored | future-work #2 |
| 10.2 / 12.3 | Linux has no plugin popup menu | Right-click does nothing plugin-defined | future-work #3 |
| 9.3 / 11.x | Linux zoom is per-origin, not per-processor | `KeepZoom` persists but one shared zoom across all file types | future-work #4 |
| 12.1 | Wayland first-Ctrl+Q window jump (documented; software-rendering workaround) | May jump / DC main window jumps | future-work #7 |

Recheck these rows after any change that touches the corresponding subsystem;
their status is the fastest signal that a deferred item has regressed or been
(resurrected and) fixed.

## Suggested run order

1. Smoke: §1.1, §1.8, §1.16, §2.1, §3.1 — confirm the five loader families + image + dir + PDF all open.
2. Encoding: §5 (the highest regression surface after the charset changes; pairs with the `Examples/encoding-*` fixtures).
3. Keyboard/lifecycle: §6, §7, §9, §10.
4. Config toggles: §11.
5. Platform-divergence spot checks: §3 Linux notes, §5.9, §9.3, §10.2, §12.

Use a fresh `edgeviewer.ini` from `Resources/` for the baseline pass; only
introduce the key tweaks listed in §11 as their own pass so default behaviour
is verified first.

## See also

- `EdgeViewer.Tests/readme.md` — what the autotests DO cover (read this first to
  avoid re-testing covered units).
- `openspec/specs/` — the behavioural contract each row verifies
  (`markdown`, `rst`, `asciidoc`, `html`, `mhtml`, `eml`, `images`,
  `directory-view`, `encoding-override`, `charset-autodetect`,
  `virtual-host-mapping`, `popup-context-menu`, `accelerator-keys`,
  `wlx-contract`, `print`, `text-search`, `offline-mode`, `zoom-control`,
  `linux-parity`).
- `openspec/notes/future-work.md` — deferred items whose status explains the
  platform-divergence notes above (Linux dir thumbnails #2, right-click #3,
  per-processor zoom #4, accelerator relaying #5, Wayland Ctrl+Q #7, MHT Linux
  re-decode #10).
