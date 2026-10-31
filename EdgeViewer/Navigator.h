#pragma once

#include <format>
#include <fstream>
#include <string>

#include "IWebView.h"
#include "Globals.h"

//------------------------------------------------------------------------
// Pure helper: escape backslash and single-quote for JS string embedding.
std::wstring jsEscape(const std::wstring& str);

// Pure helper: build the window.find() script string from a pattern and TC search params.
std::wstring BuildFindScript(const std::wstring& pattern, int params);

// Pure helper: build the window.print() script string.
std::wstring BuildPrintScript();

//------------------------------------------------------------------------
class Navigator
{
public:
	Navigator(IWebView& webView) : mWebView(webView) {}

	// fs::path is std::wstring on Windows
	// Returns false when no processor claims the path (missing file,
	// unknown type) so callers can decline instead of leaving a
	// blank/broken view behind.
	bool Open(const std::filesystem::path& path) const;
	void Search(const std::wstring& str, int params) const;
	void Print() const;

private:
	IWebView& mWebView;
};
//------------------------------------------------------------------------
