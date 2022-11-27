#pragma once

#include "ProcessorInterface.h"
#include <string>

//------------------------------------------------------------------------
class HtmlProcessor : public ProcessorInterface
{
public:
	virtual bool Load(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	fs::path mPath;
};
//------------------------------------------------------------------------
