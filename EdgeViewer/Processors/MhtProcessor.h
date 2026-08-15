#pragma once

#include <filesystem>

#include "ProcessorInterface.h"

// MHT file:
// Load using MTHML2HTML
//------------------------------------------------------------------------
class MhtProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const std::filesystem::path& path);
	virtual void OpenIn(IWebView& webView) const;

private:
	std::filesystem::path mPath;
};
//------------------------------------------------------------------------
