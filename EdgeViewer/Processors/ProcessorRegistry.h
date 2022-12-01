#pragma once

// must include all processor headers to initialize them
#include "AdocProcessor.h"
#include "MdProcessor.h"
#include "HtmlProcessor.h"

//------------------------------------------------------------------------
class ProcessorRegistry
{
public:
	void Add(ProcessorInterface* processor);
	bool CanLoad(const fs::path& path);
	void LoadAndOpen(const fs::path& path, ViewPtr webView);

private:
	ProcessorInterface* findProcessor(const fs::path& path);
	std::vector<ProcessorInterface*>  mRegistry;
};
//------------------------------------------------------------------------
ProcessorRegistry& gsProcRegistry();
//------------------------------------------------------------------------
