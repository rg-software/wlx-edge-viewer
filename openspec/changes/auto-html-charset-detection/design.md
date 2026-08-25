# Design: Automatic charset detection for HTML (provisional auto re-decode)

## Context

The manual override shipped in `html-charset-override`: HTML re-decodes host-side (Windows `MultiByteToWideChar`, Linux `QTextCodec`) via `ApplyCharsetOverride`, driven by the Encoding menu. Chromium's own sniffing is conservative — with no BOM/meta it falls back to UTF-8/Windows-1252 and never statistically detects windows-1251/KOI8-R etc. So those files are mojibake until the user manually picks a code page. This change automates the good guess, without disturbing Chromium when it's right and without clobbering a manual choice.

## Goals / Non-Goals

**Goals:**
- Legacy-encoded HTML (no BOM, no meta, non-UTF-8) renders correctly on first paint after a quick provisional correction.
- No flicker when Chromium already chose the right encoding (the common case).
- The user's manual pick always wins; auto never re-fires after a manual choice.
- Auto is provisional: selecting "Auto-detect" restores the true sniffed render.
- Works identically on Windows (WebView2) and Linux (Qt Web Engine), Win32/x64.

**Non-Goals:**
- No `[HTML] DetectEncoding` ini key — always active.
- No new C++ dependency; no vcpkg change.
- No change to MHT (its loader already re-decodes page-side).
- No interference with the engine's own sniffing; we act only on the *result*.

## Decisions

### D1: Detect page-side, re-decode host-side (no live-DOM mutation)

Rejects both endpoints:
- *Full JS re-decode* → proven broken on Qt (document.write / head-body swaps blank the render and kill the context menu — html-charset-override §2).
- *Full host detection* → needs a C++ charset detector dependency.

Instead: the page runs a small statistical detector over **pristine bytes already in the page** (`window.__evRawFileBytesB64`, injected by `SetRawFileBytes`), guesses a tag, and compares it to the engine's actual decision (`document.characterSet`). Only when they **disagree** does it send one `CMD_AUTO_ENCODING|<tag>` JS→host message. The host then calls the *existing* `ApplyCharsetOverride(tag)` — the same host transcode the manual menu uses. No re-decode logic exists page-side; the page only **detects and requests**.

The detector is a vendored chardet-style universal detector (MIT, ~30 KB) under `Resources/assets/charset/`, plus a small glue script. This mirrors the existing pattern of vendored JS assets (marked.js, highlight.js, mhtml2html).

Why page-side detection is safe here: it never touches `document` — it only reads the base64 bytes, runs math, and posts a message. The Qt-blank failure mode was specifically *re-decode*, not detection.

### D2: One new JS→host command, reusing existing bridges

`CMD_AUTO_ENCODING|<tag>`:
- **Windows:** arrives via the existing `window.chrome.webview.postMessage` → `ParseAndPostMessage` in `WebViewFactory.cpp`. New branch looks up the backend by HWND from `gs_Views` and calls `ApplyCharsetOverride(tag)`, but only if auto is allowed (D3).
- **Linux:** the existing `ev://_cmd/<id>` shim (used for CMD_ZOOM/CMD_SAVE) already forwards `chrome.webview.postMessage`; the scheme handler in `QtWebEngineBackend.cpp` adds a `CMD_AUTO_ENCODING` case → resolves the backend by container id → `ApplyCharsetOverride(tag)`.

This is the same hop pattern as the ESC bridge and the imgview zoom — no new transport, only new tokens. (It is a *new* JS→host message; AGENTS.md's "no new JS→host commands" note was scoped to the manual override mechanism.)

### D3: State machine — provisional auto, manual wins

Each backend keeps two transient booleans, reset on every `Navigate`/`NavigateToString`:

- `userPicked` — set in `ApplyCharsetOverride` **only when the call comes from the menu** (i.e. not when it comes from `CMD_AUTO_ENCODING`). Once true, `CMD_AUTO_ENCODING` is ignored for the rest of the view.
- `autoApplied` — set when `CMD_AUTO_ENCODING` successfully dispatches; used to (a) prevent re-firing on subsequent navigations of the same view and (b) drive the menu hint (D4).

To distinguish menu-vs-auto at the `ApplyCharsetOverride` boundary, introduce a lightweight `IWebView::ApplyCharsetOverride` param or a separate entry point (`ApplyAutoDetectedEncoding(tag)` that sets `autoApplied` but not `userPicked` and calls the existing method). The menu path stays the plain `ApplyCharsetOverride`.

Auto also never fires when **no cached bytes** are present (non-HTML views) — the DocumentCreation script gates on `window.__evRawFileBytesB64` existing (only HTML sets it).

### D4: Menu/check state surfaces the provisional guess

- Default: "Auto-detect" checked (active tag `""`).
- After auto re-decode: the active checked item is still "Auto-detect", but its label reads e.g. **"Auto-detect (Windows-1251)"** so the user sees what's happening and can either accept/override.
- Manual pick: that entry becomes checked (existing radio behavior).
- Picking "Auto-detect" always restores engine sniffing (`ApplyCharsetOverride("")`) and clears `autoApplied`.

The label hint needs a `autoSuggestedTag` field on the backend (set when auto applies) surfaced like `GetActiveEncodingTag` (`GetAutoSuggestedTag`), so both menu builders can render the parenthetical.

### D5: Suppress when the source declares an encoding

Even if `document.characterSet` disagrees with our guess, do **not** fire auto when the source carries a BOM or a `<meta charset>`/`http-equiv` — Chromium is then authoritative and our guess could be wrong for a legitimately UTF-8/other file. Detection of a declaration is cheap: the glue script checks the first 1024 bytes of the decoded-Latin1 head for BOM (byte-level) or a `<meta ...charset=...>` / `Content-Type: text/html; charset=` regex before deciding to post.

## Risks / Trade-offs

- **Wrong guesses**: detector confidence matters. Only auto-apply when the detector returns a high-confidence tag; otherwise leave the initial sniffed render (still mojibake, user can use the manual menu). Configurable threshold in the glue script.
- **Flicker when it fires**: acceptable — it only fires for files Chromium actually mis-detected; one re-render is the cost of fixing.
- **New JS→host message**: additive and reuses existing bridges; the "manual override stays JS→host-free" property from html-charset-override is preserved (auto is a distinct, detection-only channel).
- **chardet size** (~30 KB): negligible vs the bundled marked.js/highlight.js.

## Migration Plan

1. Vendor the detector + glue script under `Resources/assets/charset/`.
2. Add `CMD_AUTO_ENCODING` handling (Windows `ParseAndPostMessage`, Linux `_cmd` handler) + `autoApplied`/`userPicked`/`autoSuggestedTag` backend state.
3. Wire the DocumentCreation glue script only for HTML renders (gate on `__evRawFileBytesB64`).
4. Extend both menu builders with the "Auto-detect (suggested: …)" label + radio check when `autoApplied`.
5. Spec deltas: new `charset-autodetect` capability; `encoding-override` menu-state modification. Readme future-work #1 moved fully (automatic detection now ships).