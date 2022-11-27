#pragma once

#include "ProcessorInterface.h"
#include <string>

// TODO: fully offline asciidoc (fontAwesome, etc.)
//------------------------------------------------------------------------
class AdocProcessor : public ProcessorInterface
{
public:
	virtual bool Load(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	fs::path mPath;
};
//------------------------------------------------------------------------
