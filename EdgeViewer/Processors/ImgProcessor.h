#pragma once

#include "BaseFileProcessor.h"
#include <filesystem>

//------------------------------------------------------------------------
class ImgProcessor : public BaseFileProcessor
{
public:
	bool InitPath(const std::filesystem::path& path) override;

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
	std::vector<WStrPair> extraPlaceholders() const override;
};
//------------------------------------------------------------------------