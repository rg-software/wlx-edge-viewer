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

	const auto& rstIni = GlobalSettings().get("RST");
	const auto cssFile = gs_IsDarkMode ? rstIni.get("CSSDark") : rstIni.get("CSS");
	std::wstring wloader(to_utf16(ReadFile(assetsPath() / L"rst" / L"loader.html")));
	wloader = replacePlaceholders(wloader, {
		{L"__BASE_URL__", urlPathW(mPath.parent_path().relative_path())},
		{L"__CSS_NAME__", to_utf16(cssFile)},
		{L"__RST_FILENAME__", urlPathW(mPath.relative_path())}
	});

	webView->NavigateToString(wloader.c_str());
}
//------------------------------------------------------------------------
