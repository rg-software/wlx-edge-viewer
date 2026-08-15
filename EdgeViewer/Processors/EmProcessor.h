#pragma once

#include "BaseFileProcessor.h"

//------------------------------------------------------------------------
class EmProcessor : public BaseFileProcessor
{
private:
	const std::wstring& cssSection() const override
	{
		static const std::wstring s = L"EML";
		return s;
	}
	const std::wstring& loaderDirectory() const override
	{
		static const std::wstring s = L"eml";
		return s;
	}
	const std::wstring& filenamePlaceholder() const override
	{
		static const std::wstring s = L"__EML_FILENAME__";
		return s;
	}
};
//------------------------------------------------------------------------