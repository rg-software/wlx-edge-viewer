#pragma once

#include "ProcessorInterface.h"

// MHT file:
// Load using MTHML2HTML
//------------------------------------------------------------------------
class MhtProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	fs::path mPath;
};
//------------------------------------------------------------------------
