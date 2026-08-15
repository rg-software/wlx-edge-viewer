#pragma once

#include <filesystem>

#include "ProcessorInterface.h"

// Image file
// Load using a custom template
//------------------------------------------------------------------------
class ImgProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const std::filesystem::path& path);
	virtual void OpenIn(IWebView& webView) const;

private:
	std::filesystem::path mPath;
};
//------------------------------------------------------------------------
