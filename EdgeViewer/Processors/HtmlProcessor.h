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
	fs::path mPath;
};
//------------------------------------------------------------------------
