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
	std::wstring wloader(to_utf16(ReadFile(assetsPath() / L"markdown" / L"loader.html")));
	wloader = replacePlaceholders(wloader, {
		{L"__BASE_URL__", urlPathW(mPath.parent_path().relative_path())},
		{L"__CSS_NAME__", to_utf16(cssFile)},
		{L"__MD_FILENAME__", urlPathW(mPath.relative_path())}
	});

	webView->NavigateToString(wloader.c_str());
}
//------------------------------------------------------------------------
