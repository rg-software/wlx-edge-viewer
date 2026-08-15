#pragma once

#include <filesystem>

#include "ProcessorInterface.h"

// reStructuredText file:
// Load using Node.js library "restructured", converted via browserify
// This is almost a copy of MDProcessor, but might diverge eventually
//------------------------------------------------------------------------
class RstProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const std::filesystem::path& path);
	virtual void OpenIn(IWebView& webView) const;

private:
	std::filesystem::path mPath;
};
//------------------------------------------------------------------------
