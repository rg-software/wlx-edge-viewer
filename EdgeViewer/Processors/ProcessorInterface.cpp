#include "ProcessorInterface.h"
#include "ProcessorRegistry.h"
//------------------------------------------------------------------------
ProcessorInterface::ProcessorInterface()
{
	ProcessorRegistry::Add(this);
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
