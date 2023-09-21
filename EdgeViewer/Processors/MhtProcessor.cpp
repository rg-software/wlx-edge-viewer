#include "MhtProcessor.h"
#include <regex>
//------------------------------------------------------------------------
namespace { MhtProcessor mht; }
//------------------------------------------------------------------------
bool MhtProcessor::InitPath(const fs::path& path)
{
	mPath = path;
	return isType(path.extension(), "MHTML");
}
//------------------------------------------------------------------------
void MhtProcessor::OpenIn(ViewPtr webView) const
{ 
	mapDomains(webView, mPath.root_path());

	//const auto& adIni = gs_Ini().get("AsciiDoc");
	std::string loader(readFile(assetsPath() / L"mhtml" / L"loader.html"));
	//loader = std::regex_replace(loader, std::regex("__BASE_URL__"), urlPath(mPath.parent_path().relative_path()));
	//loader = std::regex_replace(loader, std::regex("__CSS_NAME__"), adIni.get("CSS"));
	loader = std::regex_replace(loader, std::regex("__MHTML_FILENAME__"), urlPath(mPath.relative_path()));

	webView->NavigateToString(to_utf16(loader).c_str());
}
//------------------------------------------------------------------------
