#include "Navigator.h"
#include "MdProcessor.h"
#include "mini/ini.h"
#include <sstream>

//------------------------------------------------------------------------
void Navigator::Open(const std::wstring& path_str) const
{
	auto path = fs::path(path_str);
	auto webview23 = mWebView.try_query<ICoreWebView2_3>();
	webview23->SetVirtualHostNameToFolderMapping(L"local.example", path.parent_path().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);

	if (isType(path.extension(), "HTML"))
	{
		auto script = std::format(L"window.location = 'http://local.example/{}';", path.filename().c_str());
		mWebView->ExecuteScript(script.c_str(), NULL);
	}
	else if (isType(path.extension(), "Markdown"))
	{
		auto md = MdProcessor(path_str).Markdown();
		mWebView->NavigateToString(md.c_str());
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
