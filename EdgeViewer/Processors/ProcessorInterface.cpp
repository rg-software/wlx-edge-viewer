#include "ProcessorInterface.h"
#include "ProcessorRegistry.h"
#include "../Globals.h"
#include <regex>
#include <format>
#include <string>
#include <codecvt>
#include <locale>
#include <cwctype>
#include <cctype>
#include <sstream>
#include <algorithm>

//------------------------------------------------------------------------
namespace
{
// Cross-platform percent-encoder (RFC 3986 unreserved set + a few reserved
// characters that are safe in our use). Replaces the wininet UrlEscapeW call.
bool isUnreservedUrlChar(wchar_t ch)
{
	if (ch >= L'A' && ch <= L'Z') return true;
	if (ch >= L'a' && ch <= L'z') return true;
	if (ch >= L'0' && ch <= L'9') return true;
	switch (ch)
	{
		case L'-': case L'_': case L'.': case L'~':
		case L'/': case L':': case L'@': case L'!':
		case L'$': case L'&': case L'\'': case L'(':
		case L')': case L'*': case L'+': case L',':
		case L';': case L'=': case L'?':
			return true;
	}
	return false;
}

// Cross-platform case-insensitive wide-string compare (replaces the
// Win32-only _wcsicmp).
int wcsicmp(const std::wstring& a, const std::wstring& b)
{
	if (a.size() != b.size())
		return a.size() < b.size() ? -1 : 1;
	for (size_t i = 0; i < a.size(); ++i)
	{
		auto ca = std::towlower(static_cast<wint_t>(a[i]));
		auto cb = std::towlower(static_cast<wint_t>(b[i]));
		if (ca != cb)
			return ca < cb ? -1 : 1;
	}
	return 0;
}

std::wstring percentEncode(const std::wstring& in)
{
	// Encode the wstring as UTF-8 bytes (the only path through the WebView
	// expects UTF-8 URLs), then percent-encode bytes outside the unreserved
	// set.
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	std::string utf8 = conv.to_bytes(in);

	std::wstring out;
	out.reserve(utf8.size() * 3);
	for (unsigned char c : utf8)
	{
		if ((c >= 'A' && c <= 'Z') ||
		    (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') ||
		    c == '-' || c == '_' || c == '.' || c == '~' ||
		    c == '/' || c == ':' || c == '@' || c == '!' ||
		    c == '$' || c == '&' || c == '\'' || c == '(' ||
		    c == ')' || c == '*' || c == '+' || c == ',' ||
		    c == ';' || c == '=' || c == '?')
		{
			out += static_cast<wchar_t>(c);
		}
		else
		{
			static const wchar_t hex[] = L"0123456789ABCDEF";
			out += L'%';
			out += hex[c >> 4];
			out += hex[c & 0xF];
		}
	}
	return out;
}
} // anonymous namespace
//------------------------------------------------------------------------
ProcessorInterface::ProcessorInterface()
{
	gsProcRegistry().Add(this);
}
//------------------------------------------------------------------------
bool ProcessorInterface::isType(const std::filesystem::path& ext, const std::string& type) const
{
	std::istringstream is(GlobalSettings()["Extensions"][type]);
	std::string s;

	// Extension strings are plain ASCII, so case-insensitive comparison
	// on the narrow string is acceptable on both platforms (the Win32
	// _wcsicmp equivalent).
	auto lower = [](std::string str)
	{
		std::transform(str.begin(), str.end(), str.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return str;
	};
	std::string extLower = lower(ext.string());

	while (std::getline(is, s, ','))
	{
		if (extLower == lower("." + s))
			return true;
	}
	return false;
}
//------------------------------------------------------------------------
std::wstring ProcessorInterface::urlPathW(const std::filesystem::path& path) const
{
	std::wstring pathWithSlashes = path.wstring();
	std::replace(pathWithSlashes.begin(), pathWithSlashes.end(), L'\\', L'/');	// prevent escaping

	// Handle # character which UrlEscapeW treats as a fragment delimiter.
	// Mask it with a placeholder before encoding, then restore %23.
	std::wstring placeholder = L"%23";

	if (pathWithSlashes.find(L"#") != std::wstring::npos)
	{
		int i = 0;
		do
		{
			placeholder = std::format(L"_H{}_", i++);
		}
		while (pathWithSlashes.find(placeholder) != std::wstring::npos);

		pathWithSlashes = std::regex_replace(pathWithSlashes, std::wregex(L"#"), placeholder);
	}

	auto escaped = percentEncode(pathWithSlashes);
	return std::regex_replace(escaped, std::wregex(placeholder), L"%23");
}
//------------------------------------------------------------------------
std::wstring ProcessorInterface::replacePlaceholders(const std::wstring& tpl, std::initializer_list<WStrPair> pairs) const
{
	auto result = tpl;
	for (const auto& pair : pairs)
		result = std::regex_replace(result, std::wregex(pair.first), pair.second);
	return result;
}
//------------------------------------------------------------------------
std::string ProcessorInterface::urlPath(const std::filesystem::path& path) const
{
	return to_utf8(urlPathW(path));
}
//------------------------------------------------------------------------
std::filesystem::path ProcessorInterface::assetsPath() const
{
	return std::filesystem::path(GetModulePath()) / L"assets";
}
//------------------------------------------------------------------------
void ProcessorInterface::mapDomains(IWebView& webView, const std::filesystem::path& rootPath) const
{
	webView.RegisterVirtualHost(L"assets.example", assetsPath());
	webView.RegisterVirtualHost(L"local.example", rootPath);
}
//------------------------------------------------------------------------
