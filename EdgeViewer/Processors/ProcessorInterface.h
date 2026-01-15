#pragma once

#include "../Globals.h"

//------------------------------------------------------------------------
class ProcessorInterface
{
public:
	ProcessorInterface();
	virtual bool InitPath(const fs::path& path) = 0;
	virtual void OpenIn(ViewPtr webView) const = 0;
	
protected:
	using WStrPair = std::pair<std::wstring, std::wstring>;
	
	std::string urlPath(const fs::path& path) const;
	std::wstring urlPathW(const fs::path& path) const;
	std::wstring replacePlaceholders(const std::wstring& tpl, std::initializer_list<WStrPair> pairs) const;

	bool isType(const fs::path& ext, const std::string& type) const;
	void mapDomains(ViewPtr webView, const fs::path& rootPath) const;
	fs::path assetsPath() const;
};
//------------------------------------------------------------------------
