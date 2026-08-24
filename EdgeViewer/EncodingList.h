#pragma once

#include <array>
#include <cstddef>

//------------------------------------------------------------------------
// EncodingList - single source of truth for the manual-encoding context
// menu (issue #66), shared by both backends so Windows and Linux always
// offer identical entries (spec D5 parity).
//
// Each entry pairs a display label with a TextDecoder tag. An empty tag
// marks the "Auto-detect" entry, which resets to engine sniffing
// (HTML: location.reload(); MHT: stock mhtml2html pipeline).
//
// Tags are plain ASCII identifiers passed verbatim to
// window.__evEncodingApply('<tag>') by whichever backend owns the menu.
namespace EncodingList
{
struct Item
{
	const wchar_t* display;
	const wchar_t* tag;
};

inline constexpr std::array<Item, 25> kItems{{
	{L"Auto-detect", L""},
	{L"UTF-8", L"utf-8"},
	{L"UTF-16LE", L"utf-16le"},
	{L"Windows-1252", L"windows-1252"},
	{L"ISO-8859-15", L"iso-8859-15"},
	{L"ISO-8859-1", L"iso-8859-1"},
	{L"Windows-1250", L"windows-1250"},
	{L"ISO-8859-2", L"iso-8859-2"},
	{L"Windows-1251", L"windows-1251"},
	{L"KOI8-R", L"koi8-r"},
	{L"ISO-8859-5", L"iso-8859-5"},
	{L"Windows-1253", L"windows-1253"},
	{L"ISO-8859-7", L"iso-8859-7"},
	{L"Windows-1254", L"windows-1254"},
	{L"Windows-1255", L"windows-1255"},
	{L"Windows-1256", L"windows-1256"},
	{L"Windows-1257", L"windows-1257"},
	{L"Windows-1258", L"windows-1258"},
	{L"Windows-874", L"windows-874"},
	{L"Shift-JIS", L"shift_jis"},
	{L"EUC-JP", L"euc-jp"},
	{L"GBK", L"gbk"},
	{L"GB2312", L"gb2312"},
	{L"Big5", L"big5"},
	{L"EUC-KR", L"euc-kr"},
}};
}
//------------------------------------------------------------------------
