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

	void Open(const std::wstring& path_str);

private:
	ViewPtr mWebView;

	//void mapVirtualHost(const std::wstring& dir)
	//{
	//}

	//std::wstring fileToUri(const std::wstring& FileToLoad)
	//{
	//	wchar_t url[INTERNET_MAX_URL_LENGTH];
	//	DWORD len = INTERNET_MAX_URL_LENGTH;
	//	UrlCreateFromPath(FileToLoad.c_str(), url, &len, NULL);
	//	return std::wstring(url);// , len);

	//	// alternatively, load as string
	//	//std::wstring uri = std::format(L"file:///{}", FileToLoad);
	//	//std::replace(std::begin(uri), std::end(uri), '\\', '/');
	//	//return uri;
	//}
};

