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
	const int isFullscreen = to_int(imgIni.get("FitToScreen"));
	const auto screenClass = isFullscreen ? "full-screen" : "real-size";

	std::string loader(ReadFile(assetsPath() / L"imgview" / L"loader.html"));
	loader = std::regex_replace(loader, std::regex("__CSS_NAME__"), cssFile);
	loader = std::regex_replace(loader, std::regex("__SCREEN_CLASS__"), screenClass);
	loader = std::regex_replace(loader, std::regex("__IS_FULSCREEN__"), std::to_string(isFullscreen));
	loader = std::regex_replace(loader, std::regex("__IMG_FILENAME__"), urlPath(mPath.relative_path()));

	webView->NavigateToString(to_utf16(loader).c_str());
}
//------------------------------------------------------------------------
