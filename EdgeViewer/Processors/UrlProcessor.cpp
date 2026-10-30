#include "UrlProcessor.h"
#include "../Globals.h"
#include "ProcessorRegistry.h"
#include <regex>
#include <filesystem>
//------------------------------------------------------------------------
namespace { UrlProcessor url; }
//------------------------------------------------------------------------
bool UrlProcessor::InitPath(const std::filesystem::path& path)
{
	mPath = GetPhysicalPath(path);
	if (!std::filesystem::exists(mPath))
		return false;
	if (!std::filesystem::is_regular_file(mPath))
		return false;
	return isType(path.extension(), "URL");
}
//------------------------------------------------------------------------
void UrlProcessor::OpenIn(IWebView& webView) const
{ 
	if (!std::filesystem::exists(mPath) || !std::filesystem::is_regular_file(mPath))
		return;
	std::ifstream file(mPath);
	if (!file.good())
		return;
	
	for(std::string line; std::getline(file, line); )
		if (line.starts_with("URL="))
		{
			auto url = line.substr(4);
			// Strip trailing \r (Windows-style \r\n line endings in
			// .url files — std::getline strips \n but leaves \r).
			while (!url.empty() && url.back() == '\r')
				url.pop_back();

			// open local files using HtmlProcessor
			if (url.starts_with("file:///"))
			{
				gsProcRegistry().LoadAndOpen(url.substr(8), webView);
			}
			else
			{
				mapDomains(webView, mPath.root_path());
				webView.Navigate(to_utf16(url).c_str());
			}

			return;
		}
}
//------------------------------------------------------------------------
