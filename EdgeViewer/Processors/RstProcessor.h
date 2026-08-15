#pragma once

#include "BaseFileProcessor.h"

//------------------------------------------------------------------------
class RstProcessor : public BaseFileProcessor
{
private:
	const std::wstring& cssSection() const override
	{
		static const std::wstring s = L"RST";
		return s;
	}
	const std::wstring& loaderDirectory() const override
	{
		static const std::wstring s = L"rst";
		return s;
	}
	const std::wstring& filenamePlaceholder() const override
	{
		static const std::wstring s = L"__RST_FILENAME__";
		return s;
	}
};
//------------------------------------------------------------------------