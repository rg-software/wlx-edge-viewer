#pragma once

#include "../IWebView.h"
#include <filesystem>

//------------------------------------------------------------------------
class ProcessorInterface
{
public:
	ProcessorInterface();
	virtual bool InitPath(const std::filesystem::path& path) = 0;
	virtual void OpenIn(IWebView& webView) const = 0;

	// Manual encoding selection (issue #66): only HTML and MHT views can
	// re-decode their source bytes, so only they host the native
	// "Encoding" submenu extension of the engine's right-click menu.
	virtual bool supportsEncodingOverride() const { return false; }

	using WStrPair = std::pair<std::wstring, std::wstring>;
	std::wstring replacePlaceholders(const std::wstring& tpl, std::initializer_list<WStrPair> pairs) const;
	bool isType(const std::filesystem::path& ext, const std::string& type) const;

protected:
	std::string urlPath(const std::filesystem::path& path) const;
	std::wstring urlPathW(const std::filesystem::path& path) const;
	void mapDomains(IWebView& webView, const std::filesystem::path& rootPath) const;
	std::filesystem::path assetsPath() const;
};
//------------------------------------------------------------------------
