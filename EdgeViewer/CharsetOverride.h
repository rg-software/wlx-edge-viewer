#pragma once

#include <cstdint>
#include <string>
#include <vector>

//------------------------------------------------------------------------
// CharsetOverride - shared, platform-agnostic byte-splice helpers for the
// host-side HTML charset override (design D3/D4). Pure functions over raw
// byte streams, so they build and unit-test without WebView2 or Qt Web
// Engine. Both backends (WebView2, Qt Web Engine) and the HTML processor
// call into these; dragging an encoding from the shared EncodingList is
// done by the caller, never here.
namespace CharsetOverride
{
// Insert `<meta charset="<tag>">` and `<base href="<baseHref>">` at the
// head of a raw HTML byte stream (design D3/D4). Returns a new vector;
// `raw` is never mutated, so repeated calls over the same pristine cache
// are idempotent.
//
// Insertion point (design D4):
//   1. after a leading `<!DOCTYPE ...>` (case-insensitive) if present;
//   2. else after a leading `<?xml ... ?>` declaration;
//   3. else at byte offset 0.
// A leading UTF BOM, when present, always stays ahead of the insertion
// point (and thus continues to win the engine's decode decision). The
// <meta> is emitted BEFORE the <base>. An empty `tag` emits no <meta>
// (the "Auto-detect" case) but keeps the <base>.
std::vector<uint8_t> SpliceCharsetAndBase(const std::vector<uint8_t>& raw,
                                          const std::wstring& tag,
                                          const std::wstring& baseHref);

// Expand raw bytes into a std::wstring of single code points: each byte
// 0x00-0xFF becomes the wchar_t with the same numeric value. Hands an
// arbitrary byte stream to the engine's setHtml/NavigateToString as a
// Latin-1 string. Shared by HtmlProcessor and the backends' override path.
std::wstring BytesToLatin1(const std::vector<uint8_t>& raw);

#ifdef _WIN32
// Decode `raw` bytes using the legacy code page named by `tag` (an
// EncodingList label such as "windows-1251", "koi8-r") into a real
// Unicode std::wstring via MultiByteToWideChar. Returns false when the
// label is unknown or the bytes cannot be decoded (caller falls back to
// the engine-default/auto rendering — never a blank view).
//
// Embedded-string loaders (NavigateToString / setHtml) always re-encode
// their argument to UTF-8, so a spliced <meta charset> can never change
// how the engine decodes the stream; the ONLY way to force a legacy code
// page through those paths is to transcode the pristine bytes host-side.
bool TranscodeBytes(const std::wstring& tag,
                    const std::vector<uint8_t>& raw,
                    std::wstring& outUnicode);
#endif
}
//------------------------------------------------------------------------