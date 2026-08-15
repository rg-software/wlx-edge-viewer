#pragma once

#include "BaseFileProcessor.h"

//------------------------------------------------------------------------
class ImgProcessor : public BaseFileProcessor
{
private:
	const std::wstring& cssSection() const override
	{
		static const std::wstring s = L"Images";
		return s;
	}
	const std::wstring& loaderDirectory() const override
	{
		static const std::wstring s = L"imgview";
		return s;
	}
	const std::wstring& filenamePlaceholder() const override
	{
		static const std::wstring s = L"__IMG_FILENAME__";
		return s;
	}
};
//------------------------------------------------------------------------