#include "EmProcessor.h"
#include "../Globals.h"
#include "mini/ini.h"
//------------------------------------------------------------------------
namespace { EmProcessor eml; }
//------------------------------------------------------------------------
bool EmProcessor::InitPath(const std::filesystem::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "EML");
}
//------------------------------------------------------------------------
void EmProcessor::OpenIn(IWebView& webView) const
{ 
	mapDomains(webView, mPath.root_path());

	const auto& emlIni = GlobalSettings().get("EML");
	const auto cssFile = gs_IsDarkMode ? emlIni.get("CSSDark") : emlIni.get("CSS");

	std::wstring wloader(to_utf16(ReadFile(assetsPath() / L"eml" / L"loader.html")));
	wloader = replacePlaceholders(wloader, {
		{L"__CSS_NAME__", to_utf16(cssFile)},
		{L"__EML_FILENAME__", urlPathW(mPath.relative_path())}
	});

	webView.NavigateToString(wloader.c_str());
}
//------------------------------------------------------------------------