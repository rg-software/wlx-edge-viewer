#pragma once

#include "CommonTypes.h"
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
	ViewPtr mWebView;

	bool isType(const std::wstring& path_str, const std::string& type) const;
};

