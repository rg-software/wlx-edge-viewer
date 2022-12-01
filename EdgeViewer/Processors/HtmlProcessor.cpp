#include "HtmlProcessor.h"
#include <shlwapi.h>
#include <wininet.h>
#include <regex>
//------------------------------------------------------------------------
namespace { HtmlProcessor html; }
//------------------------------------------------------------------------
bool HtmlProcessor::InitPath(const fs::path& path)
{
	mPath = path;
	return isType(path.extension(), "HTML");
}
//------------------------------------------------------------------------
void HtmlProcessor::OpenIn(ViewPtr webView) const
{ 
	auto webview23 = webView.try_query<ICoreWebView2_3>();
	auto modPath = fs::path(GetModulePath());
	
	webview23->SetVirtualHostNameToFolderMapping(L"local.example", mPath.root_path().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);

	wchar_t url[INTERNET_MAX_URL_LENGTH];
	DWORD urlLen = INTERNET_MAX_URL_LENGTH;
	UrlCreateFromPath(mPath.relative_path().c_str(), url, &urlLen, NULL);
	auto urlNoHost = std::wstring(url).substr(5); // remove "file:" (we get "file:<relative-path>" for relative paths)

	auto script = std::format(L"window.location = 'http://local.example/{}';", urlNoHost);
	webView->ExecuteScript(script.c_str(), NULL);
}
//------------------------------------------------------------------------
