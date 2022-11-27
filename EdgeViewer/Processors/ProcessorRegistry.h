#pragma once

// must include all processor headers to initialize them
#include "AdocProcessor.h"
#include "MdProcessor.h"
#include "HtmlProcessor.h"

//------------------------------------------------------------------------
class ProcessorRegistry
{
public:
	static void Add(ProcessorInterface* processor);
	static bool CanLoad(const fs::path& path);
	static void LoadAndOpen(const fs::path& path, ViewPtr webView);

private:
	static ProcessorInterface* findProcessor(const fs::path& path);
	static std::vector<ProcessorInterface*>  mRegistry;
};
//------------------------------------------------------------------------
