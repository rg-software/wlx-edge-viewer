#include "MdProcessor.h"
#include "mini/ini.h"
#include <regex>
//------------------------------------------------------------------------
namespace { MdProcessor md; }
//------------------------------------------------------------------------
bool MdProcessor::InitPath(const fs::path& path)
{
	mPath = path;
	return isType(path.extension(), "Markdown");
}
//------------------------------------------------------------------------
void MdProcessor::OpenIn(ViewPtr webView) const
{
	const auto& mdIni = gs_Ini().get("Markdown");
	fs::path mdDir = fs::path(GetModulePath()) / L"markdown";
	std::string loader(readFile(mdDir / L"loader.html"));
	loader = std::regex_replace(loader, std::regex("__CSS_NAME__"), mdIni.get("CSS"));
	loader = std::regex_replace(loader, std::regex("__MD_FILENAME__"), to_utf8(mPath.filename()));

	// local.example: root
	// base: full path to dir: urlNoHost without file name
	// md_url: full path to file
	auto modPath = fs::path(GetModulePath());
	auto webview23 = webView.try_query<ICoreWebView2_3>();
	webview23->SetVirtualHostNameToFolderMapping(L"local.example", mPath.parent_path().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
	webview23->SetVirtualHostNameToFolderMapping(L"markdown.example", (modPath / L"markdown").c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
	webView->NavigateToString(to_utf16(loader).c_str());
}
//------------------------------------------------------------------------
