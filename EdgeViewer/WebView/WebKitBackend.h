#pragma once

#include "../IWebView.h"
#include <filesystem>
#include <memory>
#include <string>

//------------------------------------------------------------------------
// Linux IWebView implementation wrapping WebKitGTK 4.1 + GTK 3.
//
// Linux-only: included only when building with WebKitGTK headers on
// the include path. The Windows build (EdgeViewer.vcxproj) does not
// reference this file.
//
// The constructor takes the base URI used by NavigateToString
// (Decision 9): pass e.g. "ev://assets.example/markdown/loader.html"
// so the loader's relative <link>/<script src> refs resolve via the
// custom-scheme handler registered in WebKitBackend.cpp's TU.
//
// Threading: WebKitGTK runs on the main GTK thread. Construct and
// use only on the thread that hosts the EdgeLister_Linux GTK
// container. (This matches Double Commander's contract that
// ListLoadNext etc. arrive on the main thread — see design Decision 7.)
class WebKitBackend : public IWebView
{
public:
	WebKitBackend(const std::string& baseUriForLoadHtml);
	~WebKitBackend() override;

	void NavigateToString(const std::wstring& html) override;
	void Navigate(const std::wstring& uri) override;
	void ExecuteScript(const std::wstring& js) override;
	void AddScriptToExecuteOnDocumentCreated(const std::wstring& js) override;
	void RegisterVirtualHost(const std::wstring& host,
	                        const std::filesystem::path& folder) override;
	void Close() override;

	// Linux-only accessor for EdgeLister_Linux.cpp to embed the
	// WebView as a GtkWidget child of the lister container.
	void* GetWidget() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
//------------------------------------------------------------------------