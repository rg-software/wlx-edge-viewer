## Context

See proposal.md ("Why") for motivation. The HTML processor currently renders via `NavigateToString(BytesToLatin1(renderBytes))`, which corrupts non-ASCII bytes (byte N → codepoint N → re-encoded UTF-8 → mojibake). The fix is to render the default HTML view via a real `Navigate()` to the `local.example` virtual-host URL, identical to the `OtherProcessor` (`OtherProcessor.cpp:19-22`), so the engine fetches the actual file bytes and applies its own charset sniffing.

Key constraints discovered during investigation:
- The `OtherProcessor` already does top-level `Navigate("http://local.example/<rel>")` for PDF/DOCX/etc. and it works; the spike notes in `QtWebEngineBackend.cpp:67-69` assert top-level `setUrl()` through the `ev://` scheme works. The `c68637b` commit claimed the opposite for HTML — unverified and contradicted by the codebase. This design assumes `Navigate()` works for HTML (to be confirmed on Linux per the user's plan).
- The encoding-override (`ApplyCharsetOverride`) and auto-detect (`ApplyAutoDetectedEncoding`) paths re-decode **host-side** from `m_rawFileBytes` (cached by `SetRawFileBytes`) and re-render through embedded `NavigateToString`. These are unchanged and remain the embedded path.
- The CSS-injection script (Linux `QtWebEngineBackend.cpp:587-597`) and the autodetect bootstrap (616-624) are `DocumentCreation` scripts injected once at backend construction. They gate on `location.href`/`baseURI` (CSS) and `__evRawFileBytesB64` (autodetect) — both satisfied by a `Navigate()` load, so both features keep working without JS changes.

## Goals / Non-Goals

**Goals:**
- Make the default HTML render navigate to `local.example` so the engine decodes real bytes (fixes mojibake for correctly-declared non-ASCII HTML on both platforms).
- Preserve the pristine-byte cache (`SetRawFileBytes`) so the Encoding submenu and auto-detect keep working exactly as before.
- Keep custom CSS injection working for HTML.

**Non-Goals:**
- Change `OtherProcessor`, `UrlProcessor`, or the text-loader renderers (Markdown/RST/AsciiDoc/MHT/EML still use `NavigateToString`).
- Change the embedded re-decode/override rendering path.
- Detect or force the charset on the default path (engine sniffing handles it).
- Any changes on Linux beyond what mirrors Windows.

## Decisions

### 1. Default HTML load uses `Navigate()` to the `local.example` URL (mirroring `OtherProcessor`)

`HtmlProcessor::OpenIn` should build the URL exactly like `OtherProcessor` and call `webView.Navigate(...)`, instead of pre-fetching bytes + `NavigateToString(BytesToLatin1(...))`.
- **Why**: lets the engine's built-in charset sniffing decode the real file bytes (BOM / `<meta>` / content sniffing), so correctly-declared UTF-8 files render correctly with zero byte-mapping. Removes `BytesToLatin1` from the default path entirely.
- **Alternative (rejected)**: keep embedded render + host-side transcode of the declared charset. More code, still needs the transcode machinery for what a real URL gives for free, and the user's wrong-charset concern showed it needs a detect-agreement invariant.

### 2. HTML still caches pristine bytes via `SetRawFileBytes` before navigating

Because auto-detect reads the bytes in-page and the override re-decodes host-side, HTML must keep calling `SetRawFileBytes(fileBytes)` **before** `Navigate()`. On Linux this injects the `window.__evRawFileBytesB64` DocumentCreation script, which the autodetect bootstrap requires; the script is registered before `setUrl`, so it survives into the load (same guarantee as the current embedded path). On Windows `SetRawFileBytes` caches in the backend alongside the JS bridge.
- **Why**: keeps the override + auto-detect paths byte-identical to today; no JS changes needed.
- **Note**: this means HTML still reads the file once into memory (to cache the bytes) even though the render is now a URL navigation. That is acceptable — the bytes are small and the cache is required by the existing design (`m_rawFileBytes`).

### 3. No `<base href>` splice on the default path

With a real URL navigation, relative subresource references resolve against the URL path (`http://local.example/<urlDir>/…`) naturally — no `SpliceCharsetAndBase` needed for the default render. The embedded re-decode/override path still splices `<base href>` via `SpliceCharsetAndBase` as it does today (`WebView2Backend.cpp:220`, Linux re-decode path).

### 4. Re-decode and auto-detect remain embedded (`NavigateToString`), unchanged

`ApplyCharsetOverride`/'ApplyAutoDetectedEncoding handle the "engine got it wrong" cases and re-render decoded Unicode through `NavigateToString`+`SpliceCharsetAndBase`. These are untouched. Only the *initial* render switches to `Navigate()`.

### 5. No vcpkg.json change

This is pure render-path re-routing within the existing processors/backends; no new dependency.

### 6. ForcedHtmlExt served in place as HTML (custom scheme on Windows, `ev://` default MIME on Linux)

A `ForcedHtmlExt` file (`.xml`/`.xhtml`) was previously temp-copied to a `.html` file by `Platform_Win.cpp GetPhysicalPath` so the `local.example` virtual host would serve it as `text/html`. With the real `Navigate()` render this broke relative subresources (`<img src="images/blue.png">` resolved against the temp dir). The relocation is removed so the file is served at its real location, and it is served with `Content-Type: text/html` so it renders as HTML:
- Linux needs nothing: it never relocates, and the `ev://` scheme handler's default MIME already serves unknown extensions (incl. `.xml`) as `text/html`, so `http://local.example/<rel>` → `ev://…` renders correctly with relative refs at the source dir.
- Windows cannot use `WebResourceRequested` to retag a `local.example` virtual-host response (virtual-host requests bypass that event, per the `InstallOfflineMode` finding), so it registers a **custom URI scheme** whose requests DO fire `WebResourceRequested`; the handler reads the mapped file and answers with `Content-Type: text/html`.

`HtmlProcessor::OpenIn` navigates ForcedHtmlExt through the scheme that yields `text/html` (custom scheme on Windows; `http://local.example` on Linux, rewritten to `ev://`), while genuine `.html`/`.htm` keep the plain `local.example` navigation. Auto-detect and encoding override continue to operate host-side over the pristine bytes cached by `SetRawFileBytes`. (The earlier embedded-render `<base href>` approach for relocated files was reverted: it caused an auto-detect regression and still left relative images broken.)

## Risks / Trade-offs

- **Top-level `ev://` navigation may still blank on some Qt Web Engine builds** (the `c68637b` claim). → The user will test on Linux as the final step. If it blanks, this change is blocked; the fallback is the earlier host-side transcode approach. Windows (WebView2) navigates to `http://local.example` which is well-tested by `OtherProcessor`'s historical use, so risk is Linux-specific. The implementation keeps the `SetRawFileBytes` cache and a `Navigate`-fallback branch so reverting is localized.
- **Autodetect/CSS injection depend on the injected DocumentCreation scripts firing for a URL load.** → Verified in code: both gate on `__evRawFileBytesB64`/`location.href`, which a real navigation satisfies. Confirmed in Verification (below).
- **Decoder chosen by engine may differ from an embedded-view pick.** → Same as today; the override path still lets users force a code page.

## Migration Plan

Localized to `HtmlProcessor.cpp` + verification. No schema/data migration. Rollback: revert `OpenIn` to the embedded render.

## Verification

- Build Release for Win32 and x64 (and Linux per user).
- Open `Examples/RAD.HTML`: non-ASCII block/box-drawing chars must render correctly (fixes the bug).
- Open a windows-1251 HTML with no `<meta>`: must still be fixable via Encoding → Windows-1251 (override path intact) and auto-detect must still correct on disagreement.
- Open HTML with `[HTML] CSS`/`CSSDark` overridden (e.g. a real stylesheet): injection still applies.
- Confirm `location.href` carries `http://local.example`/`ev://local.example` on each platform (CSS gate path).
