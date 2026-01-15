#include "ProcessorInterface.h"
#include "ProcessorRegistry.h"
#include <shlwapi.h>
#include <wininet.h>
#include <regex>
#include <format>
//------------------------------------------------------------------------
ProcessorInterface::ProcessorInterface()
{
	gsProcRegistry().Add(this);
}
//------------------------------------------------------------------------
bool ProcessorInterface::isType(const fs::path& ext, const std::string& type) const
{
	std::istringstream is(GlobalSettings()["Extensions"][type]);
	std::string s;

	// ini is plain ascii, so conversion to wstring is acceptable here
	while (std::getline(is, s, ','))
	{
		auto e = L"." + std::wstring(std::begin(s), std::end(s));
		if (!_wcsicmp(ext.c_str(), e.c_str()))
			return true;
	}
	return false;
}
//------------------------------------------------------------------------
std::wstring ProcessorInterface::urlPathW(const fs::path& path) const
{
	std::wstring pathWithSlashes = path;
	std::replace(pathWithSlashes.begin(), pathWithSlashes.end(), L'\\', L'/');	// prevent escaping

	// Handle # character which UrlEscapeW treats as delimiter
	std::wstring placeholder = L"%23";
	
	// find a unique placeholder to use
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

	wchar_t url[INTERNET_MAX_URL_LENGTH];
	DWORD urlLen = INTERNET_MAX_URL_LENGTH;

	UrlEscapeW(pathWithSlashes.c_str(), url, &urlLen, URL_ESCAPE_AS_UTF8);
	return std::regex_replace(url, std::wregex(placeholder), L"%23");
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
std::string ProcessorInterface::urlPath(const fs::path& path) const
{
	return to_utf8(urlPathW(path));
}
//------------------------------------------------------------------------
fs::path ProcessorInterface::assetsPath() const
{
	return fs::path(GetModulePath()) / L"assets";
}
//------------------------------------------------------------------------
void ProcessorInterface::mapDomains(ViewPtr webView, const fs::path& rootPath) const
{
	auto webview23 = webView.try_query<ICoreWebView2_3>();
	webview23->SetVirtualHostNameToFolderMapping(L"assets.example", assetsPath().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
	webview23->SetVirtualHostNameToFolderMapping(L"local.example", rootPath.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
}
//------------------------------------------------------------------------
