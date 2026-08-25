#include "HtmlProcessor.h"
#include "../Globals.h"
#include "../CharsetOverride.h"
#include <format>
#include <fstream>
#include <vector>
//------------------------------------------------------------------------
namespace { HtmlProcessor html; }
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

	// Pre-fetch the file bytes so the encoding override JS can re-decode
	// from memory instead of fetch(). Qt Web Engine's Chromium does not
	// route fetch() through the ev:// scheme handler, and the renderer
	// subprocess does not have the ev:// scheme registered at all —
	// top-level Navigate() through ev:// produces a blank page.
	// Instead, we embed the bytes directly via setHtml (NavigateToString)
	// and inject the raw bytes as base64 for the encoding-override JS.
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
	{
		// Build the base URL the <base href> splice will use so relative
		// subresources of the opened file resolve through the local.example
		// virtual-host mapping. <urlDir> is the directory of the file
		// relative to its mapped root (mPath.root_path()). The uniform
		// http:// form is used on both platforms: Qt Web Engine's
		// NavigateToString rewrites http:// -> ev:// in the HTML bytes.
		const auto relDir = mPath.relative_path().parent_path();
		const auto dirUrl = urlPathW(relDir);
		std::wstring baseHref = L"http://local.example/" + dirUrl;
		if (!baseHref.empty() && baseHref.back() != L'/')
			baseHref += L'/';

		// The default render carries no charset override (empty tag), but
		// does splice the <base href> so relative refs work (html spec).
		const auto renderBytes = CharsetOverride::SpliceCharsetAndBase(
			fileBytes, L"", baseHref);

		// Inject the raw bytes BEFORE navigation so the script is
		// registered before the page load begins — otherwise Qt Web
		// Engine's renderer reaches the injection point after the
		// document is already created and the script never runs,
		// leaving window.__evRawFileBytesB64 undefined. SetRawFileBytes
		// registers a DocumentCreation-point script (like the working
		// encoding-bootstrap script), which applies to the upcoming
		// setHtml() load. It is also the pristine cache the host-side
		// charset override re-splices.
webView.SetRawFileBytes(fileBytes);
		webView.NavigateToString(CharsetOverride::BytesToLatin1(renderBytes), to_utf8(baseHref));
	}
	else
	{
		// Fallback for empty/unreadable files: use Navigate so the
		// scheme handler serves whatever it can.
		auto urlNoHost = urlPathW(mPath.relative_path());
		auto urlFull = std::format(L"http://local.example/{}", urlNoHost);
		webView.Navigate(urlFull);
	}

// Issue #66: HTML views can re-decode their source bytes. Must
	// follow Navigate/NavigateToString, which resets the backend's flag.
	webView.SetEncodingOverrideSupported(supportsEncodingOverride());
	// HTML re-decodes HOST-SIDE (byte splice in ApplyCharsetOverride),
	// unlike the loader-based MHT path which stays page-side.
	webView.SetEncodingOverrideHtml(true);
}

//------------------------------------------------------------------------
