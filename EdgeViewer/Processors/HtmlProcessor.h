#pragma once

#include <filesystem>

#include "ProcessorInterface.h"
#include <string>

// HTML file:
// navigate to the specified location in the browser
//------------------------------------------------------------------------
class HtmlProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const std::filesystem::path& path);
	virtual void OpenIn(IWebView& webView) const;

private:
	std::filesystem::path mPath;
};
//------------------------------------------------------------------------
