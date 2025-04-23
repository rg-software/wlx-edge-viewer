#pragma once

#include "ProcessorInterface.h"
#include <string>

// TODO(mm): fully offline asciidoc (fontAwesome, etc.)

// Asciidoc file:
// Load using Asciidoctor.js
//------------------------------------------------------------------------
class AdocProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	fs::path mPath;
};
//------------------------------------------------------------------------
