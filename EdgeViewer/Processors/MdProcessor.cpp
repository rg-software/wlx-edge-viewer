#include "MdProcessor.h"
#include "mini/ini.h"
#include <regex>
//------------------------------------------------------------------------
namespace { MdProcessor md; }
//------------------------------------------------------------------------
bool MdProcessor::InitPath(const fs::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "Markdown");
}
//------------------------------------------------------------------------
void MdProcessor::OpenIn(ViewPtr webView) const
{
	mapDomains(webView, mPath.root_path());

	const auto& mdIni = GlobalSettings().get("Markdown");
	const auto cssFile = gs_IsDarkMode ? mdIni.get("CSSDark") : mdIni.get("CSS");
	std::string loader(ReadFile(assetsPath() / L"markdown" / L"loader.html"));
	loader = std::regex_replace(loader, std::regex("__BASE_URL__"), urlPath(mPath.parent_path().relative_path()));
	loader = std::regex_replace(loader, std::regex("__CSS_NAME__"), cssFile);
	loader = std::regex_replace(loader, std::regex("__MD_FILENAME__"), urlPath(mPath.relative_path()));

	webView->NavigateToString(to_utf16(loader).c_str());
}
//------------------------------------------------------------------------
