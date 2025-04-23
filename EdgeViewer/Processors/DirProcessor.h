#pragma once

#include "ProcessorInterface.h"
#include <string>

// Directory:
// Preview thumbnails (generate HTML with thumbnails, then navigate to it)
//------------------------------------------------------------------------
class DirProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	fs::path mPath;
	std::wstring genBody(const fs::path& path) const;
};
//------------------------------------------------------------------------
