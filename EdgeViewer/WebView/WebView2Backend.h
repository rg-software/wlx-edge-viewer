#pragma once

#include "IWebView.h"
#include <wil/com.h>
#include <webview2.h>

#include <filesystem>
#include <map>
#include <mutex>

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

	void NavigateToString(const std::wstring& html,
	                       const std::string& baseUri = "") override;
	void Navigate(const std::wstring& uri) override;
	void ExecuteScript(const std::wstring& js) override;
	void AddScriptToExecuteOnDocumentCreated(const std::wstring& js) override;
	void RegisterVirtualHost(const std::wstring& host, const std::filesystem::path& folder) override;
	void Close() override;
	void SetRawFileBytes(const std::vector<uint8_t>& bytes) override;
	void SetEncodingOverrideSupported(bool supported) override;
	void SetEncodingOverrideHtml(bool isHtml) override;
	void SetHtmlBaseHref(const std::string& baseHref) override;
	void SetCurrentFileDirectory(const std::filesystem::path& path) override;
	std::filesystem::path GetCurrentFileDirectory() const override;
	void ApplyCharsetOverride(const std::wstring& tag) override;
	std::wstring GetActiveEncodingTag() const override;
	void ApplyAutoDetectedEncoding(const std::wstring& tag) override;
	void ReportAutoDetectedEncoding(const std::wstring& tag) override;
	std::wstring GetAutoSuggestedTag() const override;
	void OnEncodingApplyFailed() override;

	// Windows-only accessors used by the EdgeLister WndProc for resize/focus.
	// Not part of the IWebView contract.
	wil::com_ptr<ICoreWebView2Controller> GetController() const { return mController; }
	wil::com_ptr<ICoreWebView2> GetWebView() const { return mWebView; }

private:
	// Installs the evh:// WebResourceRequested handler that serves a
	// ForcedHtmlExt file (and its relative subresources) from the mapped
	// folder with Content-Type: text/html for .xml/.xhtml (see
	// HtmlProcessor -- Windows only). No-op after the first call.
	void InstallForcedHtmlSchemeHandler();
	// Translation + serving half of the evh:// handler (see .cpp).
	bool BuildForcedHtmlResponse(const wchar_t* uri, ICoreWebView2Environment* environment,
	                             ICoreWebView2WebResourceResponse** outResponse);
	// Guard for m_hostFolders (written on OpenIn, read on the
	// WebResourceRequested callback thread).
	std::mutex m_hostFoldersMutex;
	std::map<std::wstring, std::filesystem::path> m_hostFolders;
	bool m_forcedHtmlHandlerInstalled = false;
	EventRegistrationToken m_forcedHtmlToken{};

	wil::com_ptr<ICoreWebView2Controller> mController;
	wil::com_ptr<ICoreWebView2> mWebView;

	// Host-side charset override state (html-charset-override change):
	// pristine source bytes cached by SetRawFileBytes (set by the HTML
	// processor before render) and the base URI used by the last embedded
	// render, reused to rebuild relative-ref resolution on re-decode.
	std::vector<uint8_t> m_rawFileBytes;
	std::string m_baseUri;
	std::wstring m_lastNavigateUri;    // last real Navigate() target (HTML file)
	std::filesystem::path m_currentFileDir; // opened file (EML save folder default)
	bool m_encodingOverrideSupported = false; // true for HTML and MHT (issue #66)
	bool m_encodingOverrideHtml = false;
	std::wstring m_activeEncodingTag;  // "" = auto-detect (default)
	bool m_userPicked = false;         // a manual menu pick was made
	bool m_autoApplied = false;        // an auto-re-decode was applied
	std::wstring m_autoSuggestedTag;   // encoding auto-detection suggested
	bool m_autoAlreadyApplied = false; // latch: auto already ran for this logical load
};
//------------------------------------------------------------------------
