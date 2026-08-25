#include "CharsetOverride.h"

#include <cctype>
#include <cwchar>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
// Wide (ASCII) string -> narrow bytes, byte-for-byte. Safe here because
// every caller supplies plain-ASCII input: charset labels from
// EncodingList and virtual-host URLs the processor builds (host names,
// percent-encoded path segments).
std::string WideAsciiToBytes(const std::wstring& in)
{
	std::string out;
	out.reserve(in.size());
	for (wchar_t c : in)
		out.push_back(static_cast<char>(c));
	return out;
}

// Case-insensitive ASCII prefix test against a narrow literal (e.g.
// "<!doctype"). `p` points at `n` bytes of stream data.
bool AsciiPrefixIc(const std::uint8_t* p, std::size_t n, const char* tok, std::size_t tokLen)
{
	if (n < tokLen)
		return false;
	for (std::size_t i = 0; i < tokLen; ++i)
	{
		if (std::tolower(p[i]) != std::tolower(static_cast<unsigned char>(tok[i])))
			return false;
	}
	return true;
}
} // anonymous namespace

//------------------------------------------------------------------------
std::vector<std::uint8_t> CharsetOverride::SpliceCharsetAndBase(
	const std::vector<std::uint8_t>& raw,
	const std::wstring& tag,
	const std::wstring& baseHref)
{
	// Preamble, all pure ASCII. The <meta> is emitted before the <base>
	// (design D4); an empty tag means "Auto-detect" -> no charset meta,
	// but the <base> is always kept so relative refs still resolve.
	std::string preamble;
	if (!tag.empty())
		preamble += "<meta charset=\"" + WideAsciiToBytes(tag) + "\">\n";
	preamble += "<base href=\"" + WideAsciiToBytes(baseHref) + "\">\n";

	// A leading UTF BOM must stay AHEAD of the insertion point so the
	// engine keeps trusting it (BOM'd files don't need overrides).
	std::size_t skip = 0;
	if (raw.size() >= 3 && raw[0] == 0xEF && raw[1] == 0xBB && raw[2] == 0xBF)
		skip = 3;                                                       // UTF-8 BOM
	else if (raw.size() >= 2 && raw[0] == 0xFF && raw[1] == 0xFE)
		skip = 2;                                                       // UTF-16LE BOM
	else if (raw.size() >= 2 && raw[0] == 0xFE && raw[1] == 0xFF)
		skip = 2;                                                       // UTF-16BE BOM

	std::size_t insert = skip;
	const std::size_t avail = raw.size() - skip;
	const std::uint8_t* p = raw.data() + skip;

	// 1. after a leading <!DOCTYPE ...> (case-insensitive)
	if (AsciiPrefixIc(p, avail, "<!doctype", 9))
	{
		for (std::size_t i = 9; i < avail; ++i)
		{
			if (p[i] == '>')
			{
				insert = skip + i + 1;
				break;
			}
		}
	}
	// 2. else after a leading <?xml ... ?> declaration
	else if (AsciiPrefixIc(p, avail, "<?xml", 5))
	{
		for (std::size_t i = 5; i + 1 < avail; ++i)
		{
			if (p[i] == '?' && p[i + 1] == '>')
			{
				insert = skip + i + 2;
				break;
			}
		}
	}
	// 3. else at the start of the content (after any BOM)

	std::vector<std::uint8_t> out;
	out.reserve(raw.size() + preamble.size());
	out.insert(out.end(), raw.begin(), raw.begin() + static_cast<std::ptrdiff_t>(insert));
	out.insert(out.end(), preamble.begin(), preamble.end());
	out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(insert), raw.end());
	return out;
}

//------------------------------------------------------------------------
std::wstring CharsetOverride::BytesToLatin1(const std::vector<std::uint8_t>& raw)
{
	std::wstring out;
	out.reserve(raw.size());
	for (const std::uint8_t b : raw)
		out.push_back(static_cast<wchar_t>(b));
	return out;
}

//------------------------------------------------------------------------
#ifdef _WIN32
// EncodingList label -> Windows code-page id. Every entry EncodingList
// offers maps here; unknown labels return 0 so the caller falls back.
namespace
{
UINT MapTagToWindowsCodePage(const std::wstring& tag)
{
	struct Entry { const wchar_t* tag; UINT cp; };
	static constexpr Entry kMap[] = {
		{L"utf-8",       CP_UTF8},
		{L"utf-16le",    1200},
		{L"utf-16be",    1201},
		{L"windows-1252", 1252},
		{L"iso-8859-15", 28605},
		{L"iso-8859-1",  28591},
		{L"windows-1250", 1250},
		{L"iso-8859-2",  28592},
		{L"windows-1251", 1251},
		{L"koi8-r",      20866},
		{L"iso-8859-5",  28595},
		{L"windows-1253", 1253},
		{L"iso-8859-7",  28597},
		{L"windows-1254", 1254},
		{L"windows-1255", 1255},
		{L"windows-1256", 1256},
		{L"windows-1257", 1257},
		{L"windows-1258", 1258},
		{L"windows-874",  874},
		{L"shift_jis",    932},
		{L"euc-jp",       20932},
		{L"gbk",          936},
		{L"gb2312",       936},
		{L"big5",         950},
		{L"euc-kr",       949},
	};
	for (const auto& e : kMap)
		if (_wcsicmp(tag.c_str(), e.tag) == 0)
			return e.cp;
	return 0;
}
} // anonymous namespace

bool CharsetOverride::TranscodeBytes(const std::wstring& tag,
                                     const std::vector<std::uint8_t>& raw,
                                     std::wstring& outUnicode)
{
	const UINT cp = MapTagToWindowsCodePage(tag);
	if (cp == 0 || raw.empty())
		return false;

	int wlen = MultiByteToWideChar(cp, MB_ERR_INVALID_CHARS,
		reinterpret_cast<const char*>(raw.data()), static_cast<int>(raw.size()),
		nullptr, 0);
	if (wlen <= 0)
	{
		// Retry leniently — some real-world files are malformed for their
		// declared code page; a best-effort decode beats a failed override.
		wlen = MultiByteToWideChar(cp, 0,
			reinterpret_cast<const char*>(raw.data()), static_cast<int>(raw.size()),
			nullptr, 0);
		if (wlen <= 0)
			return false;
	}

	outUnicode.resize(static_cast<std::size_t>(wlen));
	MultiByteToWideChar(cp, 0,
		reinterpret_cast<const char*>(raw.data()), static_cast<int>(raw.size()),
		outUnicode.data(), wlen);
	return true;
}
#endif
//------------------------------------------------------------------------