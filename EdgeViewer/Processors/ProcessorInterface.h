#pragma once

#include "../Globals.h"

//------------------------------------------------------------------------
class ProcessorInterface
{
public:
	ProcessorInterface();
	virtual bool Load(const fs::path& path) = 0;
	virtual void OpenIn(ViewPtr webView) const = 0;
	
	// TODO: refactor it (code duplication)
	static bool RegistryLoad(const fs::path& path);
	static bool RegistryLoadAndOpen(const fs::path& path, ViewPtr webView);

private:
	static std::vector<ProcessorInterface*> mRegistry;

protected:
	bool isType(const fs::path& ext, const std::string& type) const;
};
//------------------------------------------------------------------------
