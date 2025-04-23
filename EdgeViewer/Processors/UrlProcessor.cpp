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
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "URL");
}
//------------------------------------------------------------------------
void UrlProcessor::OpenIn(ViewPtr webView) const
{ 
	std::ifstream file(mPath);
	
	for(std::string line; std::getline(file, line); )
		if (line.starts_with("URL="))
		{
			auto url = line.substr(4);

			// open local files using HtmlProcessor
			if (url.starts_with("file:///"))
			{
				gsProcRegistry().LoadAndOpen(url.substr(8), webView);
			}
			else
			{
				mapDomains(webView, mPath.root_path());
				auto script = std::format("window.location = '{}';", url);
				webView->ExecuteScript(to_utf16(script).c_str(), NULL);
			}

			return;
		}
}
//------------------------------------------------------------------------
