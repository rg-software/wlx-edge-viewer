#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

//------------------------------------------------------------------------
// Shared, platform-agnostic web view interface. The concrete backend
// (WebView2 on Windows, Qt Web Engine on Linux) lives in a platform-only
// translation unit; processors and Navigator call only these methods.
class IWebView
{
public:
	virtual ~IWebView() = default;

	virtual void NavigateToString(const std::wstring& html,
	                               const std::string& baseUri = "") = 0;
	virtual void Navigate(const std::wstring& uri) = 0;
	virtual void ExecuteScript(const std::wstring& js) = 0;
	virtual void AddScriptToExecuteOnDocumentCreated(const std::wstring& js) = 0;
	virtual void RegisterVirtualHost(const std::wstring& host, const std::filesystem::path& folder) = 0;
	virtual void Print() { ExecuteScript(L"window.print();"); }
	virtual void Close() = 0;

	// Pre-fetch: inject raw file bytes (base64-encoded) into the page
	// as window.__evRawFileBytesB64 so the encoding override JS can
	// re-decode without fetch() — custom ev:// schemes don't support
	// fetch() from the page context in Qt Web Engine.  Must be called
	// before Navigate(); default no-op (Windows WebView2 doesn't need it).
	virtual void SetRawFileBytes(const std::vector<uint8_t>&) {}

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
