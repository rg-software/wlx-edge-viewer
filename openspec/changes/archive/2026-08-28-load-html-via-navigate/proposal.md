## Why

Non-ASCII HTML files (e.g. `Examples/RAD.HTML`, a genuine UTF-8 file) render as mojibake (`â–ˆ` instead of `█`). The root cause is the embedded-string render introduced in `c68637b`/`4065d2c`: `HtmlProcessor::OpenIn` passes the file bytes through `BytesToLatin1` (byte → same-value Latin-1 codepoint) into `NavigateToString`/`setHtml`, which re-encodes those codepoints to UTF-8, so the original UTF-8 bytes are never preserved and the displayed characters are wrong. The engine still reports a plausible charset (`utf-8` from the spliced `<meta>`), and jschardet over the pristine bytes agrees, so the auto-detect disagree-gate never re-decodes and the corruption is never corrected. The clean fix is to load HTML the same way the `OtherProcessor` already loads PDF/DOCX/etc. — top-level `Navigate()` to the `local.example` virtual host, letting the engine fetch the real bytes and apply its own charset sniffing. This reverts the premise of `c68637b` (that top-level `ev://` navigation renders a blank page), which contradicts the codebase's own spike notes and is unverified.

## What Changes

- **HTML initial render via `Navigate()` (not `NavigateToString`).** `HtmlProcessor::OpenIn` shall navigate to `http://local.example/<rel>` (rewritten to `ev://local.example/<rel>` on Linux) so the WebView engine fetches the real file bytes from the `local.example` virtual host and applies its own BOM/`<meta>`/content charset sniffing. This makes a correctly-declared UTF-8 file render correctly with no `BytesToLatin1` and no host-side transcode.
- **Remove the `BytesToLatin1` initial path and the `<base href>` splice from the default HTML render.** With a real URL navigation, relative subresource references resolve naturally against the URL path; no `SpliceCharsetAndBase` is needed for the default path. (`BytesToLatin1` may remain for the embedded re-decode/override path if still required there.)
- **Keep the raw-bytes cache for the override + auto-detect paths.** `SetRawFileBytes` must still run so jschardet auto-detect and the "Encoding" submenu re-decode keep working from pristine bytes.
- **Keep host-side re-decode (`ApplyCharsetOverride`) and auto-detect (`ApplyAutoDetectedEncoding`) as-is** — they re-render through the embedded-string path from pristine bytes, which remains correct. Only the *default* render switches to `Navigate()`.
- **Custom CSS injection keeps working.** Because the engine now reports a real `local.example` origin via `location.href` (not just `document.baseURI`), the DOMContentLoaded CSS injector gate (`ad14a1c`) fires identically; no change to the injector logic is required, only verification.

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities
- `html`: The "HTML Rendering" requirement currently mandates an embedded-string load and *prohibits* top-level `http(s)://local.example` navigation. This requirement changes to: the default HTML render navigates to the `local.example` virtual-host URL, letting the engine decode real bytes; the raw-bytes cache and host-side re-decode/auto-detect remain. CSS injection, virtual host mapping, and dark-mode stylesheet selection requirements are unchanged (verify only).
- `encoding-override`: The "HTML re-decode with forced charset" requirement currently describes re-rendering through an embedded-string load as the default. It must be clarified that embedded re-decode is the *override/auto-detect* path only, while the *default* render is a real `local.example` navigation that lets the engine sniff the actual bytes.

## Impact

- **Code**: `EdgeViewer/Processors/HtmlProcessor.cpp` (`OpenIn`) — switch default render from `NavigateToString(BytesToLatin1(...))` to `Navigate(...)`; keep `SetRawFileBytes`; drop the default-path base splice. `CharsetOverride.{h,cpp}` unchanged unless the re-decode path needs the base splice (likely still used by `ApplyCharsetOverride`). Backends (`WebView2Backend.cpp`, `QtWebEngineBackend.cpp`) unchanged functionally; `NavigateToString` baseUri override may become unused by HTML but is still used by the loader renderers.
- **Static assets**: `Resources/assets/charset/autodetect.js` unchanged (still runs on `window.__evRawFileBytesB64` presence); verify the disagree-gate and report paths behave the same when the engine's `document.characterSet` reflects real bytes.
- **Specs**: `openspec/specs/html/spec.md` and `openspec/specs/encoding-override/spec.md` updated via delta specs.
- **Behavioral risk to verify on Linux**: whether top-level `setUrl()` to the `ev://` scheme renders (not blanks) for HTML documents. The `OtherProcessor` already relies on this for PDF/DOCX and the spike notes assert it works; confirms the `c68637b` premise is false. If it does blank, this change is blocked and must return to embedded rendering with the host-side transcode fix instead.
