#pragma once

#include "ProcessorInterface.h"
#include <string>

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
