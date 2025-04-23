#pragma once

#include "ProcessorInterface.h"
#include <string>
#include <mutex>

// MHT file:
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
