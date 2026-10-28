#pragma once

#include "BaseFileProcessor.h"
#include "../Globals.h"

#include <fstream>

// Markdown file:
// Load using Marked.js
//------------------------------------------------------------------------
class MdProcessor : public BaseFileProcessor
{
public:
	bool InitPath(const std::filesystem::path& path) override
	{
		if (isBinaryFile(path))
			return false;
		return BaseFileProcessor::InitPath(path);
	}

private:
	static bool isBinaryFile(const std::filesystem::path& path)
	{
		std::ifstream f(path, std::ios::binary);
		if (!f)
			return false;
		unsigned char buf[8192];
		f.read(reinterpret_cast<char*>(buf), sizeof(buf));
		const std::streamsize n = f.gcount();
		if (n <= 0)
			return false;

		// UTF-16 text (BOM) is not binary.
		if (n >= 2 && ((buf[0] == 0xFF && buf[1] == 0xFE) ||
		               (buf[0] == 0xFE && buf[1] == 0xFF)))
			return false;

		size_t nulCount = 0;
		bool evenHasNul = false;
		bool oddHasNul = false;
		for (std::streamsize i = 0; i < n; ++i)
		{
			if (buf[i] == 0)
			{
				++nulCount;
				if (i % 2 == 0)
					evenHasNul = true;
				else
					oddHasNul = true;
			}
		}
		if (nulCount == 0)
			return false;
		// UTF-16 text without BOM: NULs on only one byte parity.
		if (evenHasNul != oddHasNul)
			return false;
		return true;
	}

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

	std::vector<WStrPair> extraPlaceholders() const override
	{
		// Alert callouts have their own light/dark stylesheets (the light
		// one hardcodes a near-white background). Selected by the same
		// dark-mode flag as CSS/CSSDark.
		const std::wstring alertCss = gs_IsDarkMode
			? L"github.markdown.alert.dark.css"
			: L"github.markdown.alert.css";
		return { { L"__ALERT_CSS__", alertCss } };
	}
};
//------------------------------------------------------------------------