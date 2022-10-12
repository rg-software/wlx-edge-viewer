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

	void Open(const std::wstring& path_str) const;

private:
	bool isType(const fs::path& ext, const std::string& type) const;

	ViewPtr mWebView;
};
//------------------------------------------------------------------------
