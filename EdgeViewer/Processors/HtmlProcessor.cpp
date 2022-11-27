#include "HtmlProcessor.h"
#include <regex>
//------------------------------------------------------------------------
namespace { HtmlProcessor html; }
//------------------------------------------------------------------------
bool HtmlProcessor::Load(const fs::path& path)
{
	mPath = path;
	return isType(path.extension(), "HTML");
}
//------------------------------------------------------------------------
void HtmlProcessor::OpenIn(ViewPtr webView) const
{ 
	auto webview23 = webView.try_query<ICoreWebView2_3>();
	auto modPath = fs::path(GetModulePath());
	
	webview23->SetVirtualHostNameToFolderMapping(L"local.example", mPath.parent_path().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
	auto script = std::format(L"window.location = 'http://local.example/{}';", mPath.filename().c_str());
	webView->ExecuteScript(script.c_str(), NULL);
}
//------------------------------------------------------------------------
