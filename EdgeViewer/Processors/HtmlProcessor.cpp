#include "HtmlProcessor.h"
#include "../Globals.h"
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
		// Convert raw bytes to a Latin-1 wstring: each byte 0x00–0xFF
		// becomes the wchar_t with the same numeric value. setHtml
		// receives this as a QString (Unicode); the browser renders it
		// as Latin-1 code points, which is a best-effort initial view.
		// The user can then re-decode via the Encoding context menu:
		// the encoding-override JS reads the base64-encoded raw bytes
		// (injected by SetRawFileBytes) and uses TextDecoder to produce
		// the correctly-decoded HTML.
		std::wstring latin1Str;
		latin1Str.reserve(fileBytes.size());
		for (uint8_t b : fileBytes)
			latin1Str.push_back(static_cast<wchar_t>(b));

		// Inject the raw bytes BEFORE navigation so the script is
		// registered before the page load begins — otherwise Qt Web
		// Engine's renderer reaches the injection point after the
		// document is already created and the script never runs,
		// leaving window.__evRawFileBytesB64 undefined. SetRawFileBytes
		// registers a DocumentCreation-point script (like the working
		// encoding-bootstrap script), which applies to the upcoming
		// setHtml() load.
		webView.SetRawFileBytes(fileBytes);
		webView.NavigateToString(latin1Str, "ev://local.example/");
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
}

//------------------------------------------------------------------------
