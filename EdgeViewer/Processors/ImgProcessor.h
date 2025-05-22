#pragma once

#include "ProcessorInterface.h"
#include <string>
#include <mutex>

// Image file
// Load using a custom template
//------------------------------------------------------------------------
class ImgProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	fs::path mPath;
};
//------------------------------------------------------------------------
