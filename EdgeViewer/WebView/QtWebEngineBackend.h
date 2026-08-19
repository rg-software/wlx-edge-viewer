#pragma once

#include "../IWebView.h"
#include <filesystem>
#include <memory>
#include <string>

//------------------------------------------------------------------------
// Linux IWebView implementation wrapping Qt WebEngine (QWebEngineView).
//
// Linux-only: included only when building with Qt WebEngine headers on
// the include path. The Windows build (EdgeViewer.vcxproj) does not
// reference this file.
//
// Rationale: Double Commander ships official builds for GTK2, Qt5 and
// Qt6. A GTK2 build cannot host a GTK3/WebKitGTK widget, and the Qt
// builds pass QWidget* as the lister parent — so on Linux the plugin
// embeds a QWebEngineView instead of WebKitGTK. Windows keeps WebView2.
//
// The constructor takes the base URI used by NavigateToString
// (Decision 9): pass e.g. "ev://assets.example/markdown/loader.html"
// so the loader's relative <link>/<script src> refs resolve via the
// custom-scheme handler registered in QtWebEngineBackend.cpp's TU.
//
// Threading: QWebEngineView must live on the Qt main (GUI) thread.
// Construct and use only on the thread that hosts the lister widget.
// (This matches Double Commander's contract that ListLoadNext etc.
// arrive on the main thread — see design Decision 7.)
class QtWebEngineBackend : public IWebView
{
public:
	QtWebEngineBackend(const std::string& baseUriForLoadHtml, uint64_t closeId);
	~QtWebEngineBackend() override;

	void NavigateToString(const std::wstring& html) override;
	void Navigate(const std::wstring& uri) override;
	void ExecuteScript(const std::wstring& js) override;
	void AddScriptToExecuteOnDocumentCreated(const std::wstring& js) override;
	void RegisterVirtualHost(const std::wstring& host,
	                        const std::filesystem::path& folder) override;
	void Close() override;

	// Linux-only accessor for EdgeLister_Linux.cpp to embed the
	// WebView as a QWidget child of the lister container.
	void* GetWidget() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
//------------------------------------------------------------------------
