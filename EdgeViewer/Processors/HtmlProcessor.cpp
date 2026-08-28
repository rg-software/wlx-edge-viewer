#include "HtmlProcessor.h"
#include "../Globals.h"
#include <cctype>
#include <format>
#include <fstream>
#include <regex>
#include <vector>

//------------------------------------------------------------------------
namespace
{
HtmlProcessor html;

// True when `ext` (e.g. ".xml") is listed in [Extensions] ForcedHtmlExt.
// Mirrors the (removed) relocation mask: ForcedHtmlExt is a regex alternation
// ("xml|xhtml"), so the extension is matched against \.(<alternation>)$ case-
// insensitively. Used so the processor can route a forced file through the
// scheme that serves it as text/html instead of the raw-mime local.example
// virtual host.
bool IsForcedHtmlExt(const std::filesystem::path& ext)
{
	const std::string extLower = [&] {
		std::string e = ext.string();
		for (auto& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		return e;
	}();
	const std::string mask = std::format("\\.({})", GlobalSettings()["Extensions"]["ForcedHtmlExt"]);
	try
	{
		return std::regex_match(extLower, std::regex(mask, std::regex_constants::icase));
	}
	catch (const std::regex_error&)
	{
		return false;
	}
}
}
//------------------------------------------------------------------------
bool HtmlProcessor::InitPath(const std::filesystem::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "HTML");
}
//------------------------------------------------------------------------
void HtmlProcessor::OpenIn(IWebView& webView) const
{
	mapDomains(webView, mPath.root_path());

	// The default HTML render is a REAL navigation to the local.example
	// virtual host (mirroring OtherProcessor), so the engine fetches the
	// file's actual bytes and applies its own BOM / <meta charset> /
	// content sniffing — non-ASCII files (e.g. UTF-8) decode correctly
	// with no byte-mapping. The embedded (NavigateToString) path is
	// reserved for the encoding override / auto-detect re-decode, which
	// operate host-side over the pristine bytes cached below.

	// Cache the pristine bytes so the encoding override JS and host-side
	// re-decode can work from memory (SetRawFileBytes injects the
	// DocumentCreation script that exposes window.__evRawFileBytesB64;
	// it must be registered BEFORE navigation to survive into the load).
	std::vector<uint8_t> fileBytes;
	{
		std::ifstream f(mPath, std::ios::binary);
		if (f.good())
		{
			f.seekg(0, std::ios::end);
			const auto endPos = f.tellg();
			if (endPos > 0)
			{
				f.seekg(0, std::ios::beg);
				const auto size = static_cast<size_t>(endPos);
				fileBytes.resize(size);
				f.read(reinterpret_cast<char*>(fileBytes.data()), size);
			}
		}
	}

	if (!fileBytes.empty())
		webView.SetRawFileBytes(fileBytes);

	// A ForcedHtmlExt file (e.g. .xml) must still render as HTML. It is no
	// longer relocated; it is served in place with Content-Type: text/html.
	// Windows: the local.example virtual host would serve .xml as
	// application/xml and never raises WebResourceRequested, so forced files
	// navigate through the custom "evh://" scheme whose handler emits
	// text/html. Linux: http://local.example is rewritten to ev:// whose
	// handler already serves unknown extensions as text/html, so it needs no
	// special scheme. Genuine .html/.htm keep the plain local.example URL.
	const bool forced = IsForcedHtmlExt(mPath.extension());
#ifdef _WIN32
	const std::wstring scheme = forced ? L"evh://local.example/" : L"http://local.example/";
#else
	const std::wstring scheme = L"http://local.example/";
#endif

	// Retain the HTML file's base so the encoding-override / auto-detect
	// re-decode (embedded re-render) can resolve relative subresources
	// through the same host mapping (see IWebView::SetHtmlBaseHref).
	{
		auto dirUrl = urlPath(mPath.relative_path().parent_path());
		std::string base = to_utf8(scheme) + dirUrl;
		if (!base.empty() && base.back() != '/')
			base += '/';
		webView.SetHtmlBaseHref(base);
	}

	auto urlNoHost = urlPath(mPath.relative_path());
	auto urlFull = to_utf16(urlNoHost);
	std::wstring nav = scheme + urlFull;
	webView.Navigate(nav.c_str());

	// Issue #66: HTML views can re-decode their source bytes. Must
	// follow Navigate, which resets the backend's flag for each load.
	webView.SetEncodingOverrideSupported(supportsEncodingOverride());
	// HTML re-decodes HOST-SIDE (byte splice in ApplyCharsetOverride),
	// unlike the loader-based MHT path which stays page-side.
	webView.SetEncodingOverrideHtml(true);
}

//------------------------------------------------------------------------
