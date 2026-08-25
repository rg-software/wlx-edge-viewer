#include "pch.h"
#include "CharsetOverride.h"

#include <string>
#include <string_view>
#include <vector>

//------------------------------------------------------------------------
// Tier-1 tests for the shared byte-splice pool (design D3/D4). Pure byte
// helpers, so these run without WebView2 or Qt Web Engine.
namespace
{
std::vector<uint8_t> toBytes(std::string_view s)
{
	return std::vector<uint8_t>(s.begin(), s.end());
}

std::string toStr(const std::vector<uint8_t>& v)
{
	return std::string(v.begin(), v.end());
}
} // anonymous namespace

TEST_CASE("SpliceCharsetAndBase: doctype file inserts after the declaration", "[t1][charset]")
{
	const auto raw = toBytes("<!DOCTYPE html><html><head><title>x</title></head></html>");
	const auto out = CharsetOverride::SpliceCharsetAndBase(raw, L"windows-1251",
	                                                       L"http://local.example/sub/");
	const std::string s = toStr(out);

	REQUIRE(s.rfind("<!DOCTYPE html>", 0) == 0);
	const auto meta = s.find("<meta charset=\"windows-1251\">");
	const auto base = s.find("<base href=\"http://local.example/sub/\">");
	REQUIRE(meta != std::string::npos);
	REQUIRE(base != std::string::npos);
	REQUIRE(meta < base);                       // <meta> precedes <base>
	REQUIRE(s.find("<html>") > base);           // original content kept after
}

TEST_CASE("SpliceCharsetAndBase: doctype-less file inserts at offset 0", "[t1][charset]")
{
	const auto raw = toBytes("<html><head><title>x</title></head></html>");
	const auto out = CharsetOverride::SpliceCharsetAndBase(raw, L"windows-1251",
	                                                        L"http://local.example/");
	const std::string s = toStr(out);
	REQUIRE(s.rfind("<meta charset=\"", 0) == 0);
	REQUIRE(s.find("<base href=\"http://local.example/\">") != std::string::npos);
	REQUIRE(s.find("<html>") != std::string::npos);
}

TEST_CASE("SpliceCharsetAndBase: xml declaration file", "[t1][charset]")
{
	const auto raw = toBytes("<?xml version=\"1.0\" encoding=\"UTF-8\"?><html><body>x</body></html>");
	const auto out = CharsetOverride::SpliceCharsetAndBase(raw, L"windows-1251",
	                                                        L"http://local.example/");
	const std::string s = toStr(out);
	REQUIRE(s.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?>", 0) == 0);
	REQUIRE(s.find("<meta charset=\"windows-1251\">") != std::string::npos);
	REQUIRE(s.find("<html>") != std::string::npos);
}

TEST_CASE("SpliceCharsetAndBase: BOM stays first", "[t1][charset]")
{
	// UTF-8 BOM = EF BB BF
	const std::vector<uint8_t> raw = {0xEF, 0xBB, 0xBF, '<', 'h', 't', 'm', 'l', '>', 'x', '<', '/', 'h', 't', 'm', 'l', '>'};
	const auto out = CharsetOverride::SpliceCharsetAndBase(raw, L"utf-8",
	                                                        L"http://local.example/");
	REQUIRE(out.size() >= 3);
	REQUIRE(out[0] == 0xEF);
	REQUIRE(out[1] == 0xBB);
	REQUIRE(out[2] == 0xBF);
	const std::string s = toStr(out);
	// BOM precedes the spliced <meta>, so the engine's BOM detection still wins.
	REQUIRE(s.find("<meta charset=\"utf-8\">") > 2);
}

TEST_CASE("SpliceCharsetAndBase: first-meta-wins vs file's own", "[t1][charset]")
{
	// The file declares (wrongly) utf-8; our spliced meta precedes it.
	const auto raw = toBytes("<head><meta charset=\"utf-8\"><title>x</title></head></html>");
	const auto out = CharsetOverride::SpliceCharsetAndBase(raw, L"windows-1251",
	                                                       L"http://local.example/");
	const std::string s = toStr(out);
	const auto ours = s.find("<meta charset=\"windows-1251\">");
	const auto theirs = s.find("<meta charset=\"utf-8\">");
	REQUIRE(ours != std::string::npos);
	REQUIRE(theirs != std::string::npos);
	REQUIRE(ours < theirs);
}

TEST_CASE("SpliceCharsetAndBase: repeated splice from pristine is stable", "[t1][charset]")
{
	const auto raw = toBytes("<!DOCTYPE html><p>hi</p>");
	const auto a = CharsetOverride::SpliceCharsetAndBase(raw, L"koi8-r", L"http://local.example/d/");
	const auto b = CharsetOverride::SpliceCharsetAndBase(raw, L"koi8-r", L"http://local.example/d/");
	REQUIRE(toStr(a) == toStr(b));
	// The helper always splices into raw pristine bytes, so re-selecting
	// the same tag yields exactly one <meta> (the backend never re-splices
	// already-spliced output).
	REQUIRE(toStr(a).find("<meta charset=" ) != std::string::npos);
}

TEST_CASE("SpliceCharsetAndBase: empty tag = no meta but base kept", "[t1][charset]")
{
	const auto raw = toBytes("<html><body>hi</body></html>");
	const auto out = CharsetOverride::SpliceCharsetAndBase(raw, L"", L"http://localhost/base/");
	const std::string s = toStr(out);
	REQUIRE(s.find("charset") == std::string::npos);
	REQUIRE(s.find("<base href=\"http://localhost/base/\">") != std::string::npos);
	REQUIRE(s.find("<html>") != std::string::npos);
}

TEST_CASE("BytesToLatin1 expands bytes to single code points", "[t1][charset]")
{
	const std::vector<uint8_t> raw = {'H', 0xE9, 0xFF, 'X'};
	const std::wstring s = CharsetOverride::BytesToLatin1(raw);
	REQUIRE(s == std::wstring{L'H', 0x00E9, 0x00FF, L'X'});
}

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