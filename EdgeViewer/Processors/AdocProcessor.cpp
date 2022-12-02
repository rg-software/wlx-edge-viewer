#include "AdocProcessor.h"
#include <regex>
//------------------------------------------------------------------------
namespace { AdocProcessor adoc; }
//------------------------------------------------------------------------
bool AdocProcessor::InitPath(const fs::path& path)
{
	mPath = path;
	return isType(path.extension(), "AsciiDoc");
}
//------------------------------------------------------------------------
void AdocProcessor::OpenIn(ViewPtr webView) const
{ 
	mapDomains(webView, mPath.root_path());

	const auto& adIni = gs_Ini().get("AsciiDoc");
	std::string loader(readFile(assetsPath() / L"asciidoctor" / L"loader.html"));
	loader = std::regex_replace(loader, std::regex("__BASE_URL__"), urlPath(mPath.parent_path().relative_path()));
	loader = std::regex_replace(loader, std::regex("__CSS_NAME__"), adIni.get("CSS"));
	loader = std::regex_replace(loader, std::regex("__ADOC_FILENAME__"), urlPath(mPath.relative_path()));

	webView->NavigateToString(to_utf16(loader).c_str());
}
//------------------------------------------------------------------------
