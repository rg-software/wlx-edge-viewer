#pragma once

#include "BaseFileProcessor.h"

//------------------------------------------------------------------------
class MhtProcessor : public BaseFileProcessor
{
public:
	virtual bool supportsEncodingOverride() const override { return true; }

private:
	const std::wstring& cssSection() const override
	{
		static const std::wstring s = L"MHTML";
		return s;
	}
	const std::wstring& loaderDirectory() const override
	{
		static const std::wstring s = L"mhtml";
		return s;
	}
	const std::wstring& filenamePlaceholder() const override
	{
		static const std::wstring s = L"__MHTML_FILENAME__";
		return s;
	}
};
//------------------------------------------------------------------------