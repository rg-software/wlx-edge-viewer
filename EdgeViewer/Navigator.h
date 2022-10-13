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

	void Open(const fs::path& path) const;
	void Search(const std::wstring& str, int params) const;
	void Print() const;

private:
	bool isType(const fs::path& ext, const std::string& type) const;

	ViewPtr mWebView;
};
//------------------------------------------------------------------------
