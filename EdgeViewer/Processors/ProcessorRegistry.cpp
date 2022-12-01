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
bool ProcessorRegistry::CanLoad(const fs::path& path)
{
	return findProcessor(path) != nullptr;
}
//------------------------------------------------------------------------
void ProcessorRegistry::LoadAndOpen(const fs::path& path, ViewPtr webView)
{
	const auto p = findProcessor(path);
	if (p != nullptr)
		p->OpenIn(webView);
}
//------------------------------------------------------------------------
ProcessorInterface* ProcessorRegistry::findProcessor(const fs::path& path)
{
	for (const auto& p : mRegistry)
		if (p->InitPath(path))
			return p;
	return nullptr;
}
//------------------------------------------------------------------------
