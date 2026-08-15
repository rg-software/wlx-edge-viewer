#pragma once

#include <filesystem>

#include "ProcessorInterface.h"

// Other file types:
// navigate to the specified location in the browser
// (same as HTML processor but without charset detection)
//------------------------------------------------------------------------
class OtherProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const std::filesystem::path& path);
	virtual void OpenIn(IWebView& webView) const;

private:
	std::filesystem::path mPath;
};
//------------------------------------------------------------------------
