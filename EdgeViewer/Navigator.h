#pragma once

#include <format>
#include <fstream>
#include <string>

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
	Navigator(ViewPtr webView) : mWebView(webView) {}

	// fs::path is std::wstring on Windows
	void Open(const fs::path& path) const;
	void Search(const std::wstring& str, int params) const;
	void Print() const;

private:
	ViewPtr mWebView;
};
//------------------------------------------------------------------------
