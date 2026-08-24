## Context

See `proposal.md` — Why. The EML renderer (`Resources/assets/eml/eml.js`) already parses the message with PostalMime and holds each attachment's bytes; `attachmentList()` (eml.js:82) renders a plain-text footer with no save action. The blocker is transport: saving to a user-chosen folder is a native host operation (folder picker + file write), and the current JS→host bridge is one-way.

Existing JS→host paths (unchanged by this work, all fire-and-forget):
- **Windows**: `window.chrome.webview.postMessage(msg)` → `WebMessageReceived` COM callback → `ParseAndPostMessage` (WebViewFactory.cpp:172-179, 91-113). Routes CMD_KEY (bare `PostMessage`), CMD_MENU (`WM_COPYDATA` to the plugin HWND), CMD_ZOOM (`put_ZoomFactor`). The callback hands the full message string to C++ with no practical size ceiling.
- **Linux**: the injected shim (QtWebEngineBackend.cpp:334-351) turns `postMessage(msg)` into `new Image().src = 'ev://_cmd/<id>/' + encodeURIComponent(msg)`, resolved by the scheme handler (QtWebEngineBackend.cpp:173-215). The message travels in the URL — this is the size ceiling that shapes the Linux payload decision.

Host→JS today is `ExecuteScript` (IWebView.h:17), implemented by both backends (`WebView2Backend.cpp:24`, `QtWebEngineBackend.cpp:449`) — fire-and-forget, enough to call a JS function that reports a save result.

## Goals / Non-Goals

**Goals:**
- Attachments render as clickable save entries in the EML footer.
- Clicking one saves the attachment to a user-chosen folder, with the outcome surfaced back into the loaded view (success / cancel / failure).
- One shared JS protocol and, as far as possible, one implementation path on both Windows and Linux.
- No new third-party dependency (no vcpkg.json change).

**Non-Goals:**
- Saving inline cid-referenced images separately; drag-to-extract; save-all-as-ZIP.
- Reworking the existing CMD_KEY / CMD_MENU / CMD_ZOOM behavior.
- Making IWebView richer than needed (see Decision 1: no new interface method is required).

## Decisions

### Decision 1: Host→JS reply uses the existing `ExecuteScript`

The save result is delivered by calling `ExecuteScript` with a loader-registered JS callback, e.g. `window.__emlSaveResult && window.__emlSaveResult('ok'|'cancel'|'error', 'path-or-message')`. No new `IWebView` method is introduced.

- **Why**: `ExecuteScript` is already platform-agnostic and implemented by both backends; posting a string that stores the callback target is all the loader needs. Avoids a WebView2-specific `postWebMessageToString`/Qt-webChannel divergence.
- **Alternative rejected**: adding a dedicated `IWebView::PostWebMessage` per backend. Added interface surface with no benefit; the loader already calls plain JS.

### Decision 2: JS→host save payload = the renderer's already-held base64 bytes (Windows)

The loader posts `CMD_SAVE|<sanitized-filename>|<base64-bytes>` via the existing postMessage path. On Windows this reaches `ParseAndPostMessage` as a full string (no WM_COPYDATA size wall — the `WebMessageReceived` callback is not bounded the way `WM_COPYDATA` is, and `CMD_SAVE` is dispatched to the host directly, not re-sent through the HWND copy path).

The host decodes base64 → bytes and passes them (plus the chosen folder) to the write step. The folder-picker runs first so the host can bail before extracting on cancel.

### Decision 3: Linux receives `CMD_SAVE` over the existing `ev://_cmd` shim, with an explicit size guard at 1 MB

The same `ev://_cmd/<id>/CMD_SAVE|...` route used by CMD_ZOOM carries the save message on Linux. Because that transport embeds the payload in a URL (size-limited), a guard is required: the handler checks the encoded message length against a hard ceiling before attempting decode and declines oversized attachments with a returned failure result — **never silently truncated** (matches `host-js-bridge` spec "Large attachment is not truncated").

The **practical cap is 1 MB of raw attachment bytes**, chosen as follows:

- Chromium (embedded by QtWebEngine) hard-rejects URLs past ~**2 MB**, so the engine itself caps the transport well before our code sees huge payloads.
- Base64 inflates bytes ~4/3, so 2 MB of URL → ~1.5 MB raw worst-case, before any percent-encoding overhead.
- The loader sends **URL-safe base64** (`+`→`-`, `/`→`_`; `=` pads are left alone by `encodeURIComponent`). This keeps URL length ≈ raw byte length with negligible encoding creep and no `%2B`/`%2F` blow-up.
- The 1 MB guard therefore fires cleanly **below** the ~2 MB engine ceiling, producing a deterministic "too large" result instead of relying on (or racing) the engine's abort.

- **Why**: reuses the working per-OS shim with zero new Qt infra; keeps both platforms' scripts doing `postMessage("CMD_SAVE|...")` — only the base64 alphabet differs.
- **Consequence / future work**: Windows writes attachments at any size; Linux saves up to 1 MB raw. Larger Linux supports needs a separate Qt transport (QWebChannel, or payload-referencing a host-readable temp copy) and is deliberately out of scope here.

### Decision 4: Per-OS folder picker behind `Platform.h`, shared write on the host

Add a single portable surface to `Platform.h`: `std::wstring PickFolder();` returning the chosen folder (empty string = user cancelled). The per-OS implementation owns the native dialog and its parent-window plumbing:
- `Platform_Win.cpp`: an `IFileDialog` folder-picker (consistent with the existing ShellFolder usage) parented to the active lister HWND.
- `Platform_Linux.cpp`: `QFileDialog::getExistingDirectory`, rooted in the backend's widget context.

The write itself is portable C++ (`std::ofstream`, `std::filesystem`), done in the shared save handler that the per-OS command dispatch invokes — it takes `(bytes, filename, folder)`. Filename sanitization lives with the shared write (strip path separators / reserved Windows names) so both OSes behave consistently.

### Decision 5: Threading model — run the picker+write on the message handler's thread

The folder dialog + file write run synchronously on whichever thread delivers the command (Windows `WebMessageReceived` callback; Linux `WebEngineUrlRequestJob` handler). This matches how `CMD_ZOOM` already executes in-place and avoids cross-thread marshaling for the reply. The modal picker blocks the view until the user decides, which is acceptable for an explicit save action.

### Decision 6: Windows `NavigateToString` 2 MB cap — temp-file virtual-host fallback

Discovered when the EML sample gained a >1 MB attachment: WebView2's `ICoreWebView2::NavigateToString` hard-caps the htmlContent string at **2 MB** (over which the page lands on `about:blank`), while Qt's `setHtml` (Linux) has no such string cap. Since the pre-fetch loaders base64-inline the whole file (~4/3 raw size), a document raw past ~1.5 MB exceeds the cap on Windows.

`WebView2Backend::NavigateToString` therefore switches to a fallback when the HTML exceeds ~1,000,000 wchar: it writes the HTML to a `%TEMP%` file, registers a fixed synthetic virtual host `lister.example` → that temp folder (per-core-webview, so independent per lister window), and calls `Navigate("http://lister.example/<file>")` instead. Linux keeps `setHtml` (no cap). `WebPolicy::IsLocalUri` was extended to treat `lister.example` as local so offline-mode classification stays consistent.

- **Why**: reuses the plugin's existing virtual-host infrastructure instead of `file://` (which WebPolicy treats as non-local and OfflineMode would block) and is the documented WebView2 workaround for content over the 2 MB cap.
- **Trade-off**: this is Windows-only plumbing; if Linux later gains a string limit in a different form it needs its own path. The temp file is cleaned up via the existing `gs_tempFiles` lifetime.

## Risks / Trade-offs

- [Linux large attachments] → The `ev://_cmd` URL transport caps Linux saves at 1 MB raw. Mitigated by the explicit guard (decline with message, never truncate). Support beyond 1 MB is a recognized future change (QWebChannel or temp-file handoff), deliberately out of scope here.
- [Base64 ~33% overhead in the Windows message] → Memory cost is transient (one string per save); acceptable for attachments within manual-save scope. Very large (e.g. >100MB) attachments could push memory; guard on the Windows side too with the same decline path.
- [Modal folder dialog blocks the (UI) thread] → This is the plugin's own thread, and saving is an explicit user action; acceptable. Avoids needing an async reply callback across threads.
- [Filename collides / sanitization] → Shared sanitize strips path separators and invalid filesystem chars; both OSes derive the target as `<folder>/<sanitized name>`.
- [Existing flows regress] → CMD_SAVE is an additive token; `ParseAndPostMessage` keeps existing branches untouched; new `ev://_cmd` message parsing defaults to ignoring unknown commands.

## Migration Plan

Single change; the loader assets (`loader.html`, `eml.js`, `style*.css`) change in lockstep with the C++ command dispatch. Windows and Linux build separately (their own branch), so the save command lands on each backend behind its own tree. No config keys; no vcpkg change. Rollback = revert the change; the existing attachments renderer is backward-compatible if the listener-side callback is absent (`window.__emlSaveResult && ...` guard makes the reply a no-op when unmounted).

## Open Questions

- Should Linux large-attachment save (beyond the 1 MB guard) be pursued later via QWebChannel or a temp-file handoff? That is a separate, larger transport change; it is deliberately out of scope here (see Decision 3).
- Should an optional size-limit config key appear in `edgeviewer.ini` for the Windows payload cap? The default (no ceiling except available memory) is already safe per the guard; a configurable ceiling can be added later without spec change.