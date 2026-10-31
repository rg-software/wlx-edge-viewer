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
void ProcessorRegistry::Remove(ProcessorInterface* processor)
{
	std::erase(mRegistry, processor);
}
//------------------------------------------------------------------------
bool ProcessorRegistry::LoadAndOpen(const std::filesystem::path& path, IWebView& webView) const
{
	const auto p = FindProcessor(path);
	if (p == nullptr)
		return false;
	p->OpenIn(webView);
	return true;
}
//------------------------------------------------------------------------
ProcessorInterface* ProcessorRegistry::FindProcessor(const std::filesystem::path& path) const
{
	for (const auto& p : mRegistry)
		if (p->InitPath(path))
			return p;
	return nullptr;
}
//------------------------------------------------------------------------
