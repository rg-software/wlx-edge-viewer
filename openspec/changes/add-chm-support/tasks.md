## 1. Dependency and build wiring

- [ ] 1.1 Add `overlay-ports/chmlib/` (manifest, `portfile.cmake` pinning the maintained `jedwing/CHMLib` mirror at a fixed commit + SHA512, the missing `CMakeLists.txt`, `strings_h.patch`) — the builtin port's `jedrea.com` download 404s
- [ ] 1.2 Register `overlay-ports` in `vcpkg-configuration.json` and add `chmlib` to `vcpkg.json`
- [ ] 1.3 `CMakeLists.txt`: locate `chmlib` with `find_path`/`find_library` into an imported target, link it, and add the new shared sources
- [ ] 1.4 `EdgeViewer/EdgeViewer.vcxproj` (+ `.filters`): register `ChmArchive.cpp/.h` and `ChmProcessor.cpp/.h`
- [ ] 1.5 Build both platforms to confirm `chmlib` configures and links before writing feature code

## 2. Host archive layer (C++)

- [ ] 2.1 Define `EdgeViewer/ChmArchive.{h,cpp}`: open/close, metadata (title, home topic, TOC path, code page), enumerate entries, retrieve an entry's bytes
- [ ] 2.2 Parse `/#SYSTEM` (default topic, TOC path, title, language) and `/#WINDOWS`, with a fallback home topic (first HTML entry) when unresolvable
- [ ] 2.3 Add the LCID→codepage table (~70 entries, 1252 fallback) and expose the declared code page
- [ ] 2.4 Windows: detect a path not representable in the system code page and stage a tracked temp copy via the existing `GenTempFile`/temp-tracking path; open in place otherwise
- [ ] 2.5 Add `EdgeViewer.Tests` coverage for the pure helpers (metadata parsing, codepage table, staging decision, entry-path/MIME mapping)
- [ ] 2.6 Smoke-test the layer against a representative CHM (home topic, TOC path, and an entry read all resolve)

## 3. Scheme spike (resolve the design's Open Question)

- [ ] 3.1 Spike on Linux: confirm an `<iframe src="ev://…">` navigates, XHR to `ev://chm.example/…` returns bytes, and `<img>`/`<link>` subresources load
- [ ] 3.2 Spike on Windows: confirm the `evh://` archive branch serves an entry and that an iframe/subresource request reaches `WebResourceRequested`
- [ ] 3.3 Record the spike outcome in `design.md`; if iframe/subresource loads are blocked, switch Decision 2 to the temp-extraction fallback and adjust the affected tasks before proceeding

## 4. Archive-backed scheme and instance registry (C++)

- [ ] 4.1 `IWebView`: add a method to register/unregister an archive source under a per-view instance token
- [ ] 4.2 `WebView2Backend`: add the `chm.example` branch to the `evh://` handler that reads from the backend's archive registry
- [ ] 4.3 `QtWebEngineBackend`: add the same branch to the global `EvSchemeHandler` against a process-wide instance-token registry
- [ ] 4.4 Allocate the instance token per open archive and release the archive + registry entry on `ListCloseWindow` (both platforms)
- [ ] 4.5 Verify two simultaneous CHM views resolve against their own archives and that a closed view no longer resolves

## 5. Processor, loader, and sidebar

- [ ] 5.1 `EdgeViewer/Processors/ChmProcessor.{h,cpp}`: self-registering, `InitPath` matches the `CHM` token, `OpenIn` opens the archive, registers the source, and navigates the loader
- [ ] 5.2 `Resources/assets/chm/loader.html`: two-pane layout (sidebar + content iframe) with placeholders for the instance token, home topic, TOC path, and CSS name
- [ ] 5.3 Sidebar JS: fetch the `.hhc` entry by XHR (not `fetch()`), parse the nested `<ul>`/`<OBJECT type="text/sitemap">` tree tolerantly, render it, and navigate the content pane on activation
- [ ] 5.4 Rewrite `ms-its:` / `mk:@MSITStore:` link forms to archive-scheme URLs; confirm relative links and `#fragment` anchors navigate
- [ ] 5.5 Emit `Content-Type: text/html; charset=<mapped codepage>` for text entries in the scheme handler, falling back to engine sniffing when unknown
- [ ] 5.6 Add `Resources/assets/chm/` stylesheets and select `CSS`/`CSSDark` from `gs_IsDarkMode`; theme the sidebar and page chrome
- [ ] 5.7 Handle malformed/truncated archives with an in-viewer error page and a clean archive release

## 6. Wiring and config

- [ ] 6.1 `Resources/edgeviewer.ini`: add `CHM=CHM` to `[Extensions]` and a `[CHM]` section with `CSS`/`CSSDark`
- [ ] 6.2 `EdgeViewer/WlxDetect.cpp`: add `"CHM"` to the detect-string section order (between `MHTML` and `EML`); confirm `DllMain.cpp` needs no other change
- [ ] 6.3 Verify the detect string still fits the 260-character WLX limit with the shipped ini
- [ ] 6.4 Add a representative `.chm` fixture under `Examples/` (record how it was produced) for manual verification

## 7. Documentation

- [ ] 7.1 Update `Readme.md`: list CHM as supported and remove/adjust the pointer to the separate CHM plugin
- [ ] 7.2 Add the deferred items to `openspec/notes/future-work.md` (`.hhk` index UI, `$FIftiMain` whole-archive search, per-view CHM Encoding submenu, ActiveX-dependent help content)

## 8. Verification

- [ ] 8.1 Build Release for Win32 and x64; confirm both link `chmlib` and produce `EdgeViewer-Win32.dll` / `EdgeViewer-x64.dll`
- [ ] 8.2 Build the Linux target (`cmake -B build -S . && cmake --build build -j`) and confirm it links `chmlib`
- [ ] 8.3 Manual Windows: open the `Examples/` CHM in Total Commander — home topic renders, sidebar lists the TOC, a TOC entry and an in-topic link navigate, a relative image loads, and closing the view releases the archive
- [ ] 8.4 Manual Linux: repeat the Windows checks in Double Commander, including a non-Latin CHM to exercise the code-page response
- [ ] 8.5 Regression pass: open one file of each existing type (Markdown, RST, AsciiDoc, MHTML, EML, HTML, image, directory, PDF) on both platforms and confirm no change; verify `CleanupOnExit` still removes temp files when a narrow-path CHM was staged
