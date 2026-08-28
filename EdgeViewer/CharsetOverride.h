#pragma once

#include <cstdint>
#include <string>
#include <vector>

//------------------------------------------------------------------------
// CharsetOverride - shared, platform-agnostic byte-decode helper for the
// host-side HTML charset override. A pure function over a raw byte stream,
// so it builds and unit-tests without WebView2 or Qt Web Engine. Only the
// Windows backend's override path calls in (the Qt backend decodes with
// Qt-internal converters); dragging an encoding from the shared
// EncodingList is done by the caller, never here. The old byte-mapping
// helpers (BytesToLatin1 / SpliceCharsetAndBase) were removed: they
// corrupted non-ASCII byte streams and are superseded by a real
// Navigate()-based render for the default/auto-detect paths.
namespace CharsetOverride
{
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