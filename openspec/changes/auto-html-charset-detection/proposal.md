# Proposal: Automatic charset detection for HTML with provisional auto re-decode

## Why

Chromium's HTML encoding sniffing is deliberately conservative: when an HTML file has no BOM and no `<meta charset>`, it falls back to UTF-8 (or Windows-1252) and never statistically detects Windows-1251/KOI8-R/GBK etc. — producing mojibake for those files with no user recourse beyond the manual Encoding menu. The manual override (html-charset-override change) works, but users must diagnose and click. This change gives a **good** automatic guess so those files render correctly without intervention, while never fighting Chromium when it already succeeded, and never clobbering a user's manual choice.

## What Changes

- **Page-side statistical detection, host-side re-decode (never mutates the live DOM).** A DocumentCreation script (runs before the first paint but only reports) reads the already-injected pristine bytes (`window.__evRawFileBytesB64`), runs a universal statistical charset detector (**jschardet**) on them, and compares the guess against what the engine actually chose (`document.characterSet`):
  - **Agree** → nothing happens. Chromium was right; **zero flicker**, no re-render.
  - **Disagree** → the script sends a single `CMD_AUTO_ENCODING|<tag>` JS→host message; the host calls the existing `ApplyCharsetOverride(tag)` (the same host transcoder the manual menu uses) for a provisional fresh render.
- **Provisional, not destructive.** The auto-applied re-decode is tracked host-side (`autoEncodingApplied`); it does **not** mark the user as having chosen. The pristine bytes stay cached, so selecting "Auto-detect" restores the true sniffed render.
- **User pick always beats auto.** Once the user picks an encoding manually (`userPicked`), auto-detection never fires again for that view.
- **Never second-guess an explicit declaration.** If the source carries a BOM or a `<meta charset>`/`http-equiv` declaration, auto-detection is suppressed entirely — Chromium is authoritative by spec, and re-decoding could actually break it (e.g. UTF-8 text whose byte run looks like windows-1251).
- **Always on — no ini key.** `[HTML] DetectEncoding` stays removed; this detection runs unconditionally on the provisional basis above (a user can still pick Auto-detect to return to the sniffed render).
- **Menu/checks (landed in a companion change, this change extends it):** the Encoding submenu shows the current state: "Auto-detect" checked by default; after an auto-re-decode, shows "Auto-detect (Windows-1251)"-style hint; after a manual pick, that entry is checked.

## Capabilities

### New Capabilities
- `charset-autodetect`: automatic statistical detection of legacy HTML encodings and provisional host-side re-decode, without interfering with Chromium's own sniffing or the manual override.

### Modified Capabilities
- `encoding-override`: the Encoding submenu's current-state display now also surfaces the provisional auto-detected encoding (hinted "Auto-detect (…)" until the user picks explicitly).

## Impact

- **JS assets (add):** a vendored universal charset detector (**jschardet**, UMD `window.jschardet`, LGPL-2.1+ `LICENSE-jschardet.txt` + `NOTICE.md` shipped alongside) under `Resources/assets/charset/`, loaded by the HTML bootstrap; a small glue script `autodetect.js` that atob's the pristine bytes, runs the detector, compares `document.characterSet`, and posts `CMD_AUTO_ENCODING` on disagreement. Browser-side JS is a hard requirement — the detector must run in the renderer where the bytes and `document.characterSet` live.
- **JS→host bridge (both platforms):**
  - Windows: handle `CMD_AUTO_ENCODING` in `WebViewFactory.cpp` `ParseAndPostMessage` → resolve backend by HWND → `ApplyCharsetOverride(tag)` (guarded by `autoEncodingApplied`/`userPicked`).
  - Linux: route the same token through the existing `ev://_cmd` shim's handler in `QtWebEngineBackend.cpp` → the backend's `ApplyCharsetOverride` (guarded identically).
- **C++ (shared):** backend state `autoEncodingApplied`, `userPicked` reset per navigation; `ApplyCharsetOverride` marks `userPicked` when called from the menu. No new C++ dependencies; no vcpkg change.
- **Manual-Encoding menu (Windows WebViewFactory, Qt QtWebEngineBackend):** when `autoEncodingApplied` is set, the "Auto-detect" item's label carries the suggested encoding and remains checked until the user picks a concrete entry.