#include "Navigator.h"
#include "MdProcessor.h"
#include "AdocProcessor.h"
#include "mini/ini.h"
#include <sstream>

//------------------------------------------------------------------------
void Navigator::Open(const fs::path& path) const// const std::wstring& path_str) const
{
	//auto path = fs::path(path_str);
	auto modPath = fs::path(GetModulePath());
	auto webview23 = mWebView.try_query<ICoreWebView2_3>();

	webview23->SetVirtualHostNameToFolderMapping(L"asciidoctor.example", (modPath / L"asciidoctor").c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
	webview23->SetVirtualHostNameToFolderMapping(L"markdown.example", (modPath / L"markdown").c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
	webview23->SetVirtualHostNameToFolderMapping(L"local.example", path.parent_path().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);

	if (isType(path.extension(), "HTML"))
	{
		auto script = std::format(L"window.location = 'http://local.example/{}';", path.filename().c_str());
		mWebView->ExecuteScript(script.c_str(), NULL);
	}
	else if (isType(path.extension(), "Markdown"))
	{
		auto md = MdProcessor(path).Markdown();
		mWebView->NavigateToString(md.c_str());
	}
	else if (isType(path.extension(), "AsciiDoc"))
	{
		auto adoc = AdocProcessor(path).Adoc();
		mWebView->NavigateToString(adoc.c_str());
	}
}
//------------------------------------------------------------------------
bool Navigator::isType(const fs::path& ext, const std::string& type) const
{
	std::istringstream is(gs_Ini["Extensions"][type]);
	std::string s;
	
	// ini is plain ascii, so conversion to wstring is acceptable here
	while (std::getline(is, s, ','))
	{
		auto e = L"." + std::wstring(std::begin(s), std::end(s));
		if (!_wcsicmp(ext.c_str(), e.c_str()))
			return true;
	}
	return false;
}
//------------------------------------------------------------------------
