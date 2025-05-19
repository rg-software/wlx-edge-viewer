#pragma once

#include "ProcessorInterface.h"
#include <string>

// Other file types:
// navigate to the specified location in the browser
// (same as HTML processor but without charset detection)
//------------------------------------------------------------------------
class OtherProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	fs::path mPath;
};
//------------------------------------------------------------------------
