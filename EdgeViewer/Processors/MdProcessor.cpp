#include "MdProcessor.h"
#include "mini/ini.h"
#include <regex>
//------------------------------------------------------------------------
namespace { MdProcessor md; }
//------------------------------------------------------------------------
bool MdProcessor::InitPath(const fs::path& path)
{
	mPath = path;
	return isType(path.extension(), "Markdown");
}
//------------------------------------------------------------------------
void MdProcessor::OpenIn(ViewPtr webView) const
{
	mapDomains(webView, mPath.root_path());

	const auto& mdIni = gs_Ini().get("Markdown");
	std::string loader(readFile(assetsPath() / L"markdown" / L"loader.html"));
	loader = std::regex_replace(loader, std::regex("__BASE_URL__"), urlPath(mPath.parent_path().relative_path()));
	loader = std::regex_replace(loader, std::regex("__CSS_NAME__"), mdIni.get("CSS"));
	loader = std::regex_replace(loader, std::regex("__MD_FILENAME__"), urlPath(mPath.relative_path()));

	webView->NavigateToString(to_utf16(loader).c_str());
}
//------------------------------------------------------------------------
