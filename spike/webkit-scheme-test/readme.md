# WebKitGTK Scheme Spike

Standalone prototype validating three design assumptions before committing the full port:

1. **Decision 3**: Registering a custom URI scheme `ev://` in WebKitGTK lets us dispatch by host (`assets.example` → plugin assets, `local.example` → file root). WebKitGTK 2.38+ no longer allows registering `http` as a custom scheme, so Fallback A (custom scheme name) is the only viable approach.
2. **Decision 9**: `webkit_web_view_load_html(html, "ev://assets.example/markdown/loader.html")` as base URI makes `ev://assets.example/...` and `ev://local.example/...` URLs in the loaded HTML reach our scheme handler.
3. **Task 1.3**: External HTTPS navigation (`https://example.com/`) is NOT intercepted by the custom `ev://` scheme handler.

## Prerequisites (Linux)

```bash
# Debian/Ubuntu
sudo apt install libwebkit2gtk-4.1-dev gtk3-dev cmake g++

# Fedora
sudo dnf install webkit2gtk4.1-devel gtk3-devel cmake gcc-c++

# Arch Linux
sudo pacman -S webkit2gtk-4.1 gtk3 cmake
```

## Build

From the repo root:

```bash
cmake -B build spike/webkit-scheme-test
cmake --build build
```

## Run

From the repo root (so `Resources/assets/` and `Examples/` are found):

```bash
WEBKIT_DISABLE_DMABUF_RENDERER=1 ./build/webkit_scheme_test
```

If that still fails (Wayland GBM errors), force X11:

```bash
GDK_BACKEND=x11 WEBKIT_DISABLE_DMABUF_RENDERER=1 ./build/webkit_scheme_test
```

## Expected behavior

1. A GTK window opens showing the Markdown loader template.
2. Console logs every scheme request: `[SCHEME] host=... path=...`
   - You should see requests for `assets.example/markdown/marked.js`, `assets.example/highlight_js/...`, `local.example/<test.md>` etc.
   - If **no** `[SCHEME]` logs appear, the `http` scheme registration failed silently.
3. The Markdown file from `Examples/` should render as formatted HTML (marked.js + highlight.js).
   - If the page is blank or shows raw HTML, the scheme handler isn't serving assets correctly.
4. Click "Test External URL" — should load `https://example.com/` in the web view.
   - Console should **NOT** show `[SCHEME] host=example.com` — HTTPS requests bypass our `http` handler.
   - If it **does** show `[SCHEME] host=example.com` or returns an error, the custom scheme is intercepting HTTPS too → switch to **Fallback A** (custom `evassets://` scheme + placeholder substitution).

## What to report back

After running the spike, note:

- **Did Markdown render?** (yes/no)
- **Did console show `[SCHEME]` logs for assets.example and local.example?** (yes/no)
- **Did the "Test External URL" button load example.com without triggering our handler?** (yes/no)
- **Which approach passed: primary (http scheme) or Fallback A (evassets:// scheme)?**

This result gets written into `openspec/changes/port-to-double-commander-linux/design.md` under Decision 3 per Task 1.5, gating the rest of the port's implementation.