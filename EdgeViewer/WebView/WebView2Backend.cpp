#include "WebView2Backend.h"
#include "Globals.h"

#include <wrl.h>
#include <shlwapi.h>

//------------------------------------------------------------------------
WebView2Backend::WebView2Backend(wil::com_ptr<ICoreWebView2Controller> controller,
                                 wil::com_ptr<ICoreWebView2> webview)
	: mController(std::move(controller)), mWebView(std::move(webview))
{
}
//------------------------------------------------------------------------
void WebView2Backend::NavigateToString(const std::wstring& html)
{
	mWebView->NavigateToString(html.c_str());
}
//------------------------------------------------------------------------
void WebView2Backend::Navigate(const std::wstring& uri)
{
	mWebView->Navigate(uri.c_str());
}
//------------------------------------------------------------------------
void WebView2Backend::ExecuteScript(const std::wstring& js)
{
	mWebView->ExecuteScript(js.c_str(), nullptr);
}
//------------------------------------------------------------------------
void WebView2Backend::AddScriptToExecuteOnDocumentCreated(const std::wstring& js)
{
	mWebView->AddScriptToExecuteOnDocumentCreated(js.c_str(), nullptr);
}
//------------------------------------------------------------------------
void WebView2Backend::RegisterVirtualHost(const std::wstring& host, const std::filesystem::path& folder)
{
	auto webview23 = mWebView.try_query<ICoreWebView2_3>();
	webview23->SetVirtualHostNameToFolderMapping(host.c_str(), folder.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
}
//------------------------------------------------------------------------
void WebView2Backend::Close()
{
	if (mController)
		mController->Close();
}
//------------------------------------------------------------------------
