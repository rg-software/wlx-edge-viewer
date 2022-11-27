#include "ProcessorInterface.h"
//------------------------------------------------------------------------
std::vector<ProcessorInterface*> ProcessorInterface::mRegistry;
//------------------------------------------------------------------------
ProcessorInterface::ProcessorInterface()
{
	mRegistry.push_back(this);
}
//------------------------------------------------------------------------
bool ProcessorInterface::RegistryLoadAndOpen(const fs::path& path, ViewPtr webView)
{
	for (auto p : mRegistry)
		if (p->Load(path))
		{
			p->OpenIn(webView);
			return true;
		}
	return false;
}
//------------------------------------------------------------------------
bool ProcessorInterface::isType(const fs::path& ext, const std::string& type) const
{
	std::istringstream is(gs_Ini["Extensions"][type]);
	std::string s;

	// ini is plain ascii, so conversion to wstring is acceptable here
	while (std::getline(is, s, ','))
	{
		auto e = L"." + std::wstring(std::begin(s), std::end(s));
		if (!_wcsicmp(ext.c_str(), e.c_str()))
			return true;
	}
	return false;
}
//------------------------------------------------------------------------
