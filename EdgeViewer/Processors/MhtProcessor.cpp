#include "MhtProcessor.h"
#include <regex>
//------------------------------------------------------------------------
namespace { MhtProcessor mht; }
//------------------------------------------------------------------------
bool MhtProcessor::InitPath(const fs::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "MHTML");
}
//------------------------------------------------------------------------
void MhtProcessor::OpenIn(ViewPtr webView) const
{ 
	mapDomains(webView, mPath.root_path());

	std::string loader(readFile(assetsPath() / L"mhtml" / L"loader.html"));
	loader = std::regex_replace(loader, std::regex("__MHTML_FILENAME__"), urlPath(mPath.relative_path()));

	webView->NavigateToString(to_utf16(loader).c_str());
}
//------------------------------------------------------------------------
