#include "AdocProcessor.h"
#include <regex>
//------------------------------------------------------------------------
namespace { AdocProcessor adoc; }
//------------------------------------------------------------------------
bool AdocProcessor::InitPath(const fs::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "AsciiDoc");
}
//------------------------------------------------------------------------
void AdocProcessor::OpenIn(ViewPtr webView) const
{ 
	mapDomains(webView, mPath.root_path());

	const auto& adIni = GlobalSettings().get("AsciiDoc");
	std::wstring wloader(to_utf16(ReadFile(assetsPath() / L"asciidoctor" / L"loader.html")));
	wloader = replacePlaceholders(wloader, {
		{L"__BASE_URL__", urlPathW(mPath.parent_path().relative_path())},
		{L"__CSS_NAME__", to_utf16(adIni.get("CSS"))},
		{L"__ADOC_FILENAME__", urlPathW(mPath.relative_path())}
	});

	webView->NavigateToString(wloader.c_str());
}
//------------------------------------------------------------------------
