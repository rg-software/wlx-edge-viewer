#include "CharsetOverride.h"

#include <cwchar>

#ifdef _WIN32
#include <windows.h>
#endif

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