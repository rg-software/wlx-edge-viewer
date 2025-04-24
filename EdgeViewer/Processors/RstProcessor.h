#pragma once

#include "ProcessorInterface.h"
#include <string>
#include <mutex>

// reStructuredText file:
// Load using Node.js library "restructured", converted via browserify
// This is almost a copy of MDProcessor, but might diverge eventually
//------------------------------------------------------------------------
class RstProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	fs::path mPath;
};
//------------------------------------------------------------------------
