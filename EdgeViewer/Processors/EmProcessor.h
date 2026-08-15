#pragma once

#include <filesystem>

#include "ProcessorInterface.h"

// EML file:
// Parse using postal-mime
//------------------------------------------------------------------------
class EmProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const std::filesystem::path& path);
	virtual void OpenIn(IWebView& webView) const;

private:
	std::filesystem::path mPath;
};
//------------------------------------------------------------------------
