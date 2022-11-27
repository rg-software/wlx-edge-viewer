#pragma once

#include "../Globals.h"

//------------------------------------------------------------------------
class ProcessorInterface
{
public:
	ProcessorInterface();
	virtual bool InitPath(const fs::path& path) = 0;
	virtual void OpenIn(ViewPtr webView) const = 0;
	
protected:
	bool isType(const fs::path& ext, const std::string& type) const;
};
//------------------------------------------------------------------------
