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
	mapDomains(webView, mPath.root_path());
	
	auto urlNoHost = urlPath(mPath.relative_path());
	auto script = std::format("window.location = \"http://local.example/{}\";", urlNoHost);
	webView->ExecuteScript(to_utf16(script).c_str(), NULL);
}
//------------------------------------------------------------------------
