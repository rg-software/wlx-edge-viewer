#pragma once

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
	ProcessorInterface* FindProcessor(const fs::path& path) const;
	void LoadAndOpen(const fs::path& path, ViewPtr webView) const;

private:
	std::vector<ProcessorInterface*>  mRegistry;
};
//------------------------------------------------------------------------
ProcessorRegistry& gsProcRegistry();
//------------------------------------------------------------------------
