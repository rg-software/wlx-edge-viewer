#pragma once

#include <filesystem>

#include "ProcessorInterface.h"

// TODO(mm): fully offline asciidoc (fontAwesome, etc.)

// Asciidoc file:
// Load using Asciidoctor.js
//------------------------------------------------------------------------
class AdocProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const std::filesystem::path& path);
	virtual void OpenIn(IWebView& webView) const;

private:
	std::filesystem::path mPath;
};
//------------------------------------------------------------------------
