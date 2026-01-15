#include "ImgProcessor.h"
#include "mini/ini.h"
#include <regex>
//------------------------------------------------------------------------
namespace { ImgProcessor img; }
//------------------------------------------------------------------------
bool ImgProcessor::InitPath(const fs::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "Images");
}
//------------------------------------------------------------------------
void ImgProcessor::OpenIn(ViewPtr webView) const
{
	mapDomains(webView, mPath.root_path());

	const auto& imgIni = GlobalSettings().get("Images");
	const auto cssFile = gs_IsDarkMode ? imgIni.get("CSSDark") : imgIni.get("CSS");
	const auto isFullscreen = to_int(imgIni.get("FitToScreen"));
	const auto screenClass = isFullscreen ? "full-screen" : "real-size";

	std::wstring wloader(to_utf16(ReadFile(assetsPath() / L"imgview" / L"loader.html")));
	wloader = replacePlaceholders(wloader, {
		{L"__CSS_NAME__", to_utf16(cssFile)},
		{L"__SCREEN_CLASS__", to_utf16(screenClass)},
		{L"__IS_FULSCREEN__", std::to_wstring(isFullscreen)},
		{L"__IMG_FILENAME__", urlPathW(mPath.relative_path())}
	});

	webView->NavigateToString(wloader.c_str());
}
//------------------------------------------------------------------------
