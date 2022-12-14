#include "ProcessorInterface.h"
#include "ProcessorRegistry.h"
#include <shlwapi.h>
#include <wininet.h>
//------------------------------------------------------------------------
ProcessorInterface::ProcessorInterface()
{
	gsProcRegistry().Add(this);
}
//------------------------------------------------------------------------
bool ProcessorInterface::isType(const fs::path& ext, const std::string& type) const
{
	std::istringstream is(gs_Ini()["Extensions"][type]);
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
std::string ProcessorInterface::urlPath(const fs::path& path) const
{
	char url[INTERNET_MAX_URL_LENGTH];
	DWORD urlLen = INTERNET_MAX_URL_LENGTH;
	UrlCreateFromPathA(to_utf8(path.c_str()).c_str(), url, &urlLen, NULL);
	return std::string(url).substr(5); // remove "file:" (we get "file:<relative-path>" for relative paths)
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
