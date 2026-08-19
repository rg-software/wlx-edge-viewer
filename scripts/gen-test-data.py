#!/usr/bin/env python3
"""Generate test fixtures for EdgeViewer manual Linux-parity testing.

Produces:
  Examples/test-data/sample.xhtml      HTML content in .xhtml file (ForcedHtmlExt test)
  Examples/test-data/sample.xml        HTML content in .xml file   (ForcedHtmlExt test)
  Examples/test-data/sample-local.url  .url pointing to a local file (UrlProcessor delegation)
  Examples/test-data/images/           directory with small PNGs   (DirProcessor thumbnail test)

All outputs go under Examples/test-data/.
"""
import os
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path("Examples/test-data")
ROOT.mkdir(parents=True, exist_ok=True)
(ROOT / "images").mkdir(exist_ok=True)

# ---- sample.xhtml / sample.xml: HTML content that should render as HTML ----
HTML_BODY = """<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>EdgeViewer ForcedHtmlExt test</title>
  <style>body{font-family:sans-serif;max-width:60ch;margin:2em auto;color:#222}h1{color:#c33}</style>
</head>
<body>
  <h1>ForcedHtmlExt test</h1>
  <p>If this renders as a <em>styled HTML page</em>, the
  <code>[Extensions] ForcedHtmlExt</code> regex matched and the engine
  treated the file as HTML.</p>
  <p>If you see a <strong>plain XML tree</strong> with tags like
  <code>&lt;html&gt;</code> shown as text, the Linux build is
  <em>not</em> honoring the regex (Row 2 in linux-parity).</p>
  <ul>
    <li>Bullet one</li>
    <li>Bullet two</li>
  </ul>
</body>
</html>
"""

(ROOT / "sample.xhtml").write_text(HTML_BODY, encoding="utf-8")
(ROOT / "sample.xml").write_text(HTML_BODY, encoding="utf-8")
print(f"Wrote {ROOT/'sample.xhtml'} and sample.xml")

# ---- sample-local.url: a .url pointing at a local file ----
# UrlProcessor reads the URL= line; if it starts with file:/// it delegates
# to LoadAndOpen(<path>) which re-runs FindProcessor. The path needs to be
# absolute and exist on disk.
target = (ROOT / "sample.xhtml").resolve()
url_body = f"[InternetShortcut]\nURL=file:///{target.as_posix()}\n"
(ROOT / "sample-local.url").write_text(url_body, encoding="utf-8")
print(f"Wrote {ROOT/'sample-local.url'} -> {target}")

# ---- minimal PNG generator (no external deps) ----
def make_png(path: Path, w: int, h: int, rgb: tuple[int, int, int]) -> None:
    """Write a single-color uncompressed-ish PNG."""
    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)  # 8-bit RGB
    raw = b""
    for _ in range(h):
        raw += b"\x00"  # filter: none
        raw += bytes(rgb) * w
    idat = zlib.compress(raw)
    path.write_bytes(sig + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b""))
    print(f"Wrote {path} ({w}x{h})")

make_png(ROOT / "images" / "red.png",   64, 64, (220, 60, 60))
make_png(ROOT / "images" / "green.png", 64, 64, (60, 200, 90))
make_png(ROOT / "images" / "blue.png",  64, 64, (60, 100, 220))

print("Done.")
