#include "pch.h"
#include "CharsetOverride.h"

#include <vector>

//------------------------------------------------------------------------
// Tier-1 tests for the shared byte-decode helper CharsetOverride. Pure
// byte helper, so these run without WebView2 or Qt Web Engine. Only
// TranscodeBytes survives: the byte-mapping helpers (BytesToLatin1 /
// SpliceCharsetAndBase) were removed for corrupting non-ASCII streams.
//
// NOTE: these tests compile only on Windows, where the helper is defined.

#ifdef _WIN32
TEST_CASE("TranscodeBytes: windows-1251 Cyrillic decodes correctly", "[t1][charset][win32]")
{
	// "Привет" in windows-1251.
	const std::vector<uint8_t> raw = {0xCF, 0xF0, 0xE8, 0xE2, 0xE5, 0xF2};
	std::wstring out;
	REQUIRE(CharsetOverride::TranscodeBytes(L"windows-1251", raw, out));
	REQUIRE(out == std::wstring{L'\u041F', L'\u0440', L'\u0438', L'\u0432', L'\u0435', L'\u0442'});
}

TEST_CASE("TranscodeBytes: koi8-r Cyrillic decodes correctly", "[t1][charset][win32]")
{
	// "Привет" in KOI8-R.
	const std::vector<uint8_t> raw = {0xF0, 0xD2, 0xC9, 0xD7, 0xC5, 0xD4};
	std::wstring out;
	REQUIRE(CharsetOverride::TranscodeBytes(L"koi8-r", raw, out));
	REQUIRE(out == std::wstring{L'\u041F', L'\u0440', L'\u0438', L'\u0432', L'\u0435', L'\u0442'});
}

TEST_CASE("TranscodeBytes: unknown label returns false", "[t1][charset][win32]")
{
	std::wstring out;
	REQUIRE_FALSE(CharsetOverride::TranscodeBytes(L"no-such-codec", {0x41, 0x42}, out));
}

TEST_CASE("TranscodeBytes: empty bytes returns false", "[t1][charset][win32]")
{
	std::wstring out;
	REQUIRE_FALSE(CharsetOverride::TranscodeBytes(L"windows-1251", {}, out));
}
#endif