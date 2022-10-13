#pragma once

#include "Globals.h"
#include <string>
#include <mutex>

//------------------------------------------------------------------------
class MdProcessor
{
public:
	MdProcessor(const fs::path& path) : mPath(path) {}
	std::wstring Markdown() const;

private:
	static std::mutex mHoedownLock;
	const fs::path mPath;
};
//------------------------------------------------------------------------
