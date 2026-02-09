#include "ProcessorRegistry.h"
//------------------------------------------------------------------------
ProcessorRegistry& gsProcRegistry()
{
	static ProcessorRegistry registry;
	return registry;
}
//------------------------------------------------------------------------
void ProcessorRegistry::Add(ProcessorInterface* processor)
{
	mRegistry.push_back(processor);
}
//------------------------------------------------------------------------
void ProcessorRegistry::LoadAndOpen(const fs::path& path, ViewPtr webView) const
{
	const auto p = FindProcessor(path);
	if (p != nullptr)
		p->OpenIn(webView);
}
//------------------------------------------------------------------------
ProcessorInterface* ProcessorRegistry::FindProcessor(const fs::path& path) const
{
	for (const auto& p : mRegistry)
		if (p->InitPath(path))
			return p;
	return nullptr;
}
//------------------------------------------------------------------------
