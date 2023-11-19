#include "UrlProcessor.h"
#include "ProcessorRegistry.h"
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
	std::ifstream file(mPath);
	
	for(std::string line; std::getline(file, line); )
		if (line.substr(0, 4) == "URL=")
		{
			auto url = line.substr(4);

			// open local files using HtmlProcessor
			if (url.substr(0, 8) == "file:///")
			{
				gsProcRegistry().LoadAndOpen(url.substr(8), webView);
			}
			else
			{
				mapDomains(webView, mPath.root_path());
				auto script = std::format("window.location = '{}';", line.substr(4));
				webView->ExecuteScript(to_utf16(script).c_str(), NULL);
			}

			return;
		}
}
//------------------------------------------------------------------------
