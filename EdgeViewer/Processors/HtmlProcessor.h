#pragma once

#include "ProcessorInterface.h"
#include <string>

// HTML file:
// navigate to the specified location in the browser
//------------------------------------------------------------------------
class HtmlProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	static std::string detectedCharset(const fs::path& path);
	static std::string detectedFromBom(const fs::path& path);
	static std::string detectedFromMeta(const fs::path& path);

	fs::path mPath;
};
//------------------------------------------------------------------------
