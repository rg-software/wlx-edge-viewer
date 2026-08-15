#pragma once

#include <filesystem>

#include "IWebView.h"

// must include all processor headers to initialize them
#include "AdocProcessor.h"
#include "MdProcessor.h"
#include "HtmlProcessor.h"
#include "UrlProcessor.h"
#include "MhtProcessor.h"

//------------------------------------------------------------------------
class ProcessorRegistry
{
public:
	void Add(ProcessorInterface* processor);
	ProcessorInterface* FindProcessor(const std::filesystem::path& path) const;
	void LoadAndOpen(const std::filesystem::path& path, IWebView& webView) const;

private:
	std::vector<ProcessorInterface*>  mRegistry;
};
//------------------------------------------------------------------------
ProcessorRegistry& gsProcRegistry();
//------------------------------------------------------------------------
