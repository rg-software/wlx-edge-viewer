#include "RstProcessor.h"
#include "mini/ini.h"
#include <regex>
//------------------------------------------------------------------------
namespace { RstProcessor rst; }
//------------------------------------------------------------------------
bool RstProcessor::InitPath(const fs::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "RST");
}
//------------------------------------------------------------------------
void RstProcessor::OpenIn(ViewPtr webView) const
{
	mapDomains(webView, mPath.root_path());

	const auto& rstIni = gs_Ini().get("RST");
	const auto cssFile = gs_isDarkMode ? rstIni.get("CSSDark") : rstIni.get("CSS");
	std::string loader(readFile(assetsPath() / L"rst" / L"loader.html"));
	loader = std::regex_replace(loader, std::regex("__BASE_URL__"), urlPath(mPath.parent_path().relative_path()));
	loader = std::regex_replace(loader, std::regex("__CSS_NAME__"), cssFile);
	loader = std::regex_replace(loader, std::regex("__RST_FILENAME__"), urlPath(mPath.relative_path()));

	webView->NavigateToString(to_utf16(loader).c_str());
}
//------------------------------------------------------------------------
