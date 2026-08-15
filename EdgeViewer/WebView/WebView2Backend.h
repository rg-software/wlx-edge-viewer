#pragma once

#include "IWebView.h"
#include <wil/com.h>
#include <webview2.h>

//------------------------------------------------------------------------
// Windows implementation of IWebView, wrapping the existing WebView2
// COM objects. Windows-only view setup (color profile, accelerator-key
// handler, zoom hotkey, JS message bridge) is registered in the
// factory function and is not part of the IWebView contract.
class WebView2Backend : public IWebView
{
public:
	WebView2Backend(wil::com_ptr<ICoreWebView2Controller> controller,
	                wil::com_ptr<ICoreWebView2> webview);

	void NavigateToString(const std::wstring& html) override;
	void Navigate(const std::wstring& uri) override;
	void ExecuteScript(const std::wstring& js) override;
	void AddScriptToExecuteOnDocumentCreated(const std::wstring& js) override;
	void RegisterVirtualHost(const std::wstring& host, const std::filesystem::path& folder) override;
	void Close() override;

	// Windows-only accessors used by the EdgeLister WndProc for resize/focus.
	// Not part of the IWebView contract.
	wil::com_ptr<ICoreWebView2Controller> GetController() const { return mController; }
	wil::com_ptr<ICoreWebView2> GetWebView() const { return mWebView; }

private:
	wil::com_ptr<ICoreWebView2Controller> mController;
	wil::com_ptr<ICoreWebView2> mWebView;
};
//------------------------------------------------------------------------
