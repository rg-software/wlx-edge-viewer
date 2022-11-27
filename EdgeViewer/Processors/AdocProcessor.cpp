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
	const auto& adIni = gs_Ini.get("AsciiDoc");
	fs::path adoctorDir = fs::path(GetModulePath()) / L"asciidoctor";
	std::string loader(readFile(adoctorDir / L"loader.html"));
	loader = std::regex_replace(loader, std::regex("__CSS_NAME__"), adIni.get("CSS"));
	loader = std::regex_replace(loader, std::regex("__ADOC_FILENAME__"), to_utf8(mPath.filename()));

	auto webview23 = webView.try_query<ICoreWebView2_3>();
	auto modPath = fs::path(GetModulePath());
	webview23->SetVirtualHostNameToFolderMapping(L"local.example", mPath.parent_path().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
	webview23->SetVirtualHostNameToFolderMapping(L"asciidoctor.example", (modPath / L"asciidoctor").c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
	webView->NavigateToString(to_utf16(loader).c_str());
}
//------------------------------------------------------------------------
