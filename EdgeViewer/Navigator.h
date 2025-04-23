#pragma once

#include "Globals.h"
#include <webview2.h>
#include <string>
#include <format>
#include <fstream>

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

	std::wstring jsEscape(const std::wstring& str) const;
};
//------------------------------------------------------------------------
