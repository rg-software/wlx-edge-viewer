#pragma once

#include "ProcessorInterface.h"
#include <string>
#include <mutex>

//------------------------------------------------------------------------
class MdProcessor : public ProcessorInterface
{
public:
	virtual bool InitPath(const fs::path& path);
	virtual void OpenIn(ViewPtr webView) const;

private:
	static std::mutex mHoedownLock;
	fs::path mPath;
};
//------------------------------------------------------------------------
