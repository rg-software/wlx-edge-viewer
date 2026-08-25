#pragma once

#include <filesystem>
#include <string>

//------------------------------------------------------------------------
// Shared, platform-agnostic web view interface. The concrete backend
// (WebView2 on Windows, Qt Web Engine on Linux) lives in a platform-only
// translation unit; processors and Navigator call only these methods.
class IWebView
{
public:
	virtual ~IWebView() = default;

	virtual void NavigateToString(const std::wstring& html) = 0;
	virtual void Navigate(const std::wstring& uri) = 0;
	virtual void ExecuteScript(const std::wstring& js) = 0;
	virtual void AddScriptToExecuteOnDocumentCreated(const std::wstring& js) = 0;
	virtual void RegisterVirtualHost(const std::wstring& host, const std::filesystem::path& folder) = 0;
	virtual void Print() { ExecuteScript(L"window.print();"); }
	virtual void Close() = 0;

	// Manual encoding selection (issue #66): the host-side native
	// "Encoding" submenu is only meaningful on views whose processor can
	// re-decode its source bytes (HTML, MHT). The processor reports this
	// through supportsEncodingOverride() during OpenIn so the backend can
	// gate the menu without guessing from the page URL. Default no-op:
	// WebView2 gates via the processor pointer directly, Qt Web Engine
	// stores the flag here.
	virtual void SetEncodingOverrideSupported(bool) {}
};
//------------------------------------------------------------------------
