#pragma once

#include "Globals.h"
#include <string>
//------------------------------------------------------------------------
class AdocProcessor
{
public:
	AdocProcessor(const fs::path& path) : mPath(path) {}
	std::wstring Adoc() const;

private:
	const fs::path mPath;
};
//------------------------------------------------------------------------
