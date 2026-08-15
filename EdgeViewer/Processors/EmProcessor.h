#pragma once

#include "ProcessorInterface.h"

// EML file:
// Parse using postal-mime
//------------------------------------------------------------------------
class EmProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	fs::path mPath;
};
//------------------------------------------------------------------------
