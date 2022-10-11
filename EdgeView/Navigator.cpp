#include "Navigator.h"
#include "mini/ini.h"
#include <sstream>

extern mINI::INIStructure gs_Ini;

//------------------------------------------------------------------------
void Navigator::Open(const std::wstring& path_str) const
{
	auto path = fs::path(path_str);
	auto webview23 = mWebView.try_query<ICoreWebView2_3>();
	webview23->SetVirtualHostNameToFolderMapping(L"local.example", path.parent_path().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);

	if (isType(path_str, "HTML"))
	{
		auto script = std::format(L"window.location = 'http://local.example/{}';", path.filename().c_str());
		mWebView->ExecuteScript(script.c_str(), NULL);
	}
	else if (isType(path_str, "Markdown"))
	{
		// TODO: check unicode
		// TODO: run hoedown
		std::ifstream ifs(path_str);
		std::wstring str(std::istreambuf_iterator<char>{ifs}, {});
		mWebView->NavigateToString(str.c_str());
	}
}
//------------------------------------------------------------------------
bool Navigator::isType(const std::wstring& path_str, const std::string& type) const
{
	std::istringstream is(gs_Ini["Extensions"][type]);
	std::string s;

	// ini is plain ascii, so here conversion to wstring is acceptable
	while (std::getline(is, s, ','))
		if (path_str.ends_with(L"." + std::wstring(std::begin(s), std::end(s))))
			return true;
	return false;
}
//------------------------------------------------------------------------
