#pragma once

#include "BaseFileProcessor.h"

//------------------------------------------------------------------------
class AdocProcessor : public BaseFileProcessor
{
private:
	const std::wstring& cssSection() const override
	{
		static const std::wstring s = L"AsciiDoc";
		return s;
	}
	const std::wstring& loaderDirectory() const override
	{
		static const std::wstring s = L"asciidoctor";
		return s;
	}
	const std::wstring& filenamePlaceholder() const override
	{
		static const std::wstring s = L"__ADOC_FILENAME__";
		return s;
	}
};
//------------------------------------------------------------------------