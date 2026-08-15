#pragma once

#include <filesystem>

#include "ProcessorInterface.h"

// Markdown file:
// Load using Marked.js
//------------------------------------------------------------------------
class MdProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const std::filesystem::path& path);
	virtual void OpenIn(IWebView& webView) const;

private:
	std::filesystem::path mPath;
};
//------------------------------------------------------------------------
