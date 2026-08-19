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
	virtual void Close() = 0;
};
//------------------------------------------------------------------------
