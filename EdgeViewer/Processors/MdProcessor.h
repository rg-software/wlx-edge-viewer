#pragma once

#include "BaseFileProcessor.h"

// Markdown file:
// Load using Marked.js
//------------------------------------------------------------------------
class MdProcessor : public BaseFileProcessor
{
private:
	const std::wstring& cssSection() const override
	{
		static const std::wstring s = L"Markdown";
		return s;
	}
	const std::wstring& loaderDirectory() const override
	{
		static const std::wstring s = L"markdown";
		return s;
	}
	const std::wstring& filenamePlaceholder() const override
	{
		static const std::wstring s = L"__MD_FILENAME__";
		return s;
	}
};
//------------------------------------------------------------------------