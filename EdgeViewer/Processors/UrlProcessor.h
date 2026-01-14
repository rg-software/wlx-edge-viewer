#pragma once

#include "ProcessorInterface.h"

// URL shortcut: a text file containing a line like
// URL=https://my-website.com/document.html
// 
// Find the URL target, then:
// - open it as a local HTML if it has "file:///" prefix
// - nagivate to the target page otherwise
//------------------------------------------------------------------------
class UrlProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	fs::path mPath;
};
//------------------------------------------------------------------------
