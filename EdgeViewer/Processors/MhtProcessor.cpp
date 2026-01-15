#include "MhtProcessor.h"
#include <regex>
//------------------------------------------------------------------------
namespace { MhtProcessor mht; }
//------------------------------------------------------------------------
bool MhtProcessor::InitPath(const fs::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "MHTML");
}
//------------------------------------------------------------------------
void MhtProcessor::OpenIn(ViewPtr webView) const
{ 
	mapDomains(webView, mPath.root_path());

	std::wstring wloader(to_utf16(ReadFile(assetsPath() / L"mhtml" / L"loader.html")));
	wloader = replacePlaceholders(wloader, {
		{L"__MHTML_FILENAME__", urlPathW(mPath.relative_path())}
	});

	webView->NavigateToString(wloader.c_str());
}
//------------------------------------------------------------------------
