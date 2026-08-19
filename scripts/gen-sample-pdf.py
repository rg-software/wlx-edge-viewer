#!/usr/bin/env python3
"""Generate a minimal valid PDF for EdgeViewer manual testing.

Produces a one-page PDF with two text lines. Computes xref byte offsets
correctly so any PDF reader (incl. Chromium's built-in viewer) accepts it.
"""
import sys

def make_pdf(path: str) -> None:
    objs = []  # list of (body_bytes)
    # 1: Catalog
    objs.append(b"<< /Type /Catalog /Pages 2 0 R >>")
    # 2: Pages
    objs.append(b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
    # 3: Page
    objs.append(
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
        b"/Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>"
    )
    # 4: Content stream
    content = (
        b"BT /F1 24 Tf 72 720 Td (EdgeViewer PDF test) Tj ET\n"
        b"BT /F1 12 Tf 72 690 Td (Page 1 of 1 - generated for manual testing) Tj ET"
    )
    objs.append(b"<< /Length " + str(len(content)).encode() + b" >>\nstream\n" + content + b"\nendstream")
    # 5: Font
    objs.append(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")

    out = bytearray()
    out += b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n"  # binary marker

    offsets = [0]  # obj 0 is the free entry
    for i, body in enumerate(objs, start=1):
        offsets.append(len(out))
        out += f"{i} 0 obj\n".encode()
        out += body
        out += b"\nendobj\n"

    xref_offset = len(out)
    out += b"xref\n"
    out += f"0 {len(objs) + 1}\n".encode()
    out += b"0000000000 65535 f \n"
    for off in offsets[1:]:
        out += f"{off:010d} 00000 n \n".encode()

    out += b"trailer\n"
    out += f"<< /Size {len(objs) + 1} /Root 1 0 R >>\n".encode()
    out += b"startxref\n"
    out += f"{xref_offset}\n".encode()
    out += b"%%EOF\n"

    with open(path, "wb") as f:
        f.write(out)

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "Examples/test-data/sample.pdf"
    make_pdf(out)
    print(f"Wrote {out}")
