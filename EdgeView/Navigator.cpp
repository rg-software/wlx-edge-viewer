#include "Navigator.h"

void Navigator::Open(const std::wstring& path_str)
{
	auto path = fs::path(path_str);
	auto webview23 = mWebView.try_query<ICoreWebView2_3>();
	webview23->SetVirtualHostNameToFolderMapping(L"local.example", path.parent_path().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);

	// TO REFACTOR
	if (path_str.ends_with(L"html") || path_str.ends_with(L"htm"))
	{
		auto script = std::format(L"window.location = 'http://local.example/{}';", path.filename().c_str());
		mWebView->ExecuteScript(script.c_str(), NULL);
	}
	else if (path_str.ends_with(L"markdown"))
	{
		// TODO: check unicode
		std::ifstream ifs(path_str);
		std::wstring str(std::istreambuf_iterator<char>{ifs}, {});
		mWebView->NavigateToString(str.c_str());
	}
}
