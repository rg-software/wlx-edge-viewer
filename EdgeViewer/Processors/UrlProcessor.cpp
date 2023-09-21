#include "UrlProcessor.h"
#include <shlwapi.h>
#include <wininet.h>
#include <regex>
//------------------------------------------------------------------------
namespace { UrlProcessor url; }
//------------------------------------------------------------------------
bool UrlProcessor::InitPath(const fs::path& path)
{
	mPath = path;
	return isType(path.extension(), "URL");
}
//------------------------------------------------------------------------
void UrlProcessor::OpenIn(ViewPtr webView) const
{ 
	mapDomains(webView, mPath.root_path());

	std::ifstream file(mPath);
	
	for(std::string line; std::getline(file, line); )
		if (line.substr(0, 4) == "URL=")
		{
			auto script = std::format("window.location = '{}';", line.substr(4));
			webView->ExecuteScript(to_utf16(script).c_str(), NULL);
		}
}
//------------------------------------------------------------------------
