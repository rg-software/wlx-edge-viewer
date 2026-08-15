#include "BaseFileProcessor.h"
#include "../Globals.h"
#include "../IWebView.h"

#include <fstream>
#include <iterator>
#include <cstdint>

//------------------------------------------------------------------------
bool BaseFileProcessor::InitPath(const std::filesystem::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), to_utf8(cssSection()));
}

//------------------------------------------------------------------------
void BaseFileProcessor::OpenIn(IWebView& webView) const
{
	mapDomains(webView, mPath.root_path());

	// Read the loader HTML template (UTF-8 file on disk).
	const auto loaderTpl = ReadFile(assetsPath() / loaderDirectory() / L"loader.html");

	// Read the actual file content as raw bytes (no charset conversion
	// at this layer — the loader's JS handles the charset via
	// detect_charset).
	std::vector<uint8_t> fileBytes;
	{
		std::ifstream f(mPath, std::ios::binary);
		f.seekg(0, std::ios::end);
		const auto size = static_cast<size_t>(f.tellg());
		f.seekg(0, std::ios::beg);
		fileBytes.resize(size);
		f.read(reinterpret_cast<char*>(fileBytes.data()), size);
	}
	const auto fileContentB64 = Base64Encode(fileBytes);

	// CSS / CSSDark based on current dark-mode flag.
	const auto& sectionIni = GlobalSettings().get(to_utf8(cssSection()));
	const auto cssFile = gs_IsDarkMode ? sectionIni.get("CSSDark") : sectionIni.get("CSS");

	// Substitute placeholders. The loader reads the inlined content
	// via `atob(window.__FILE_CONTENT__)` instead of fetch()ing from
	// the virtual host.
	const auto loader = replacePlaceholders(to_utf16(loaderTpl), {
		{ filenamePlaceholder(), urlPathW(mPath.relative_path()) },
		{ L"__CSS_NAME__",       to_utf16(cssFile) },
		{ L"__FILE_CONTENT__",   fileContentB64 },
	});

	webView.NavigateToString(loader);
}

//------------------------------------------------------------------------
std::wstring BaseFileProcessor::Base64Encode(const std::vector<uint8_t>& bytes)
{
	// RFC 4648 base64. We emit wide chars so the result can be dropped
	// straight into a replacePlaceholders() call.
	static constexpr char alphabet[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::wstring out;
	out.reserve(((bytes.size() + 2) / 3) * 4);

	for (size_t i = 0; i < bytes.size(); i += 3)
	{
		const uint32_t b0 = bytes[i];
		const uint32_t b1 = (i + 1 < bytes.size()) ? bytes[i + 1] : 0;
		const uint32_t b2 = (i + 2 < bytes.size()) ? bytes[i + 2] : 0;
		const uint32_t triplet = (b0 << 16) | (b1 << 8) | b2;

		out += static_cast<wchar_t>(alphabet[(triplet >> 18) & 0x3F]);
		out += static_cast<wchar_t>(alphabet[(triplet >> 12) & 0x3F]);
		out += (i + 1 < bytes.size()) ? static_cast<wchar_t>(alphabet[(triplet >> 6) & 0x3F]) : L'=';
		out += (i + 2 < bytes.size()) ? static_cast<wchar_t>(alphabet[triplet & 0x3F])     : L'=';
	}
	return out;
}
//------------------------------------------------------------------------