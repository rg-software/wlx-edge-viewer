#pragma once

#include "ProcessorInterface.h"

// Markdown file:
// Load using Marked.js
//------------------------------------------------------------------------
class MdProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	fs::path mPath;
};
//------------------------------------------------------------------------
