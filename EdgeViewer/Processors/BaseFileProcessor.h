#pragma once

#include "ProcessorInterface.h"
#include <filesystem>
#include <string>
#include <vector>

//------------------------------------------------------------------------
// Common OpenIn implementation for processors that render via a
// loader HTML template under Resources/assets/<dir>/loader.html.
//
// Eliminates per-processor copy-paste: each subclass only declares
// three string getters (its [Extensions] section name, its loader
// directory, and its URL placeholder). The base class handles
// mapDomains, loader-template read, file-content read, base64
// inlining, placeholder substitution, and NavigateToString.
//
// The file content is base64-encoded into the loader as a JS string
// literal (`const fileContent = "__FILE_CONTENT__"`); each loader is
// updated to read that instead of `fetch()`-ing from the virtual host.
// The base64 must be inlined as a quoted literal, NOT as a window
// property name (`window.__FILE_CONTENT__`): base64 padding '=' would
// collide with JS assignment after the dot. (A window-property lookup
// also breaks because replacePlaceholders regex-replaces the token
// inside the brackets.) This removes the JS round-trip and the
// empty-body-then-content flash for the file itself (assets.example
// fetches for CSS/JS libraries still happen, but those are small and
// cached by the browser).
class BaseFileProcessor : public ProcessorInterface
{
public:
	bool InitPath(const std::filesystem::path& path) override;
	void OpenIn(IWebView& webView) const override;

protected:
	std::filesystem::path mPath;

	// Subclass-provided: the [Extensions] section name in edgeviewer.ini.
	// InitPath uses it to match the file extension; OpenIn uses it to
	// read the CSS/CSSDark keys.
	virtual const std::wstring& cssSection() const = 0;

	// Subclass-provided: the loader directory under Resources/assets/
	// (e.g. L"markdown", L"asciidoctor").
	virtual const std::wstring& loaderDirectory() const = 0;

	// Subclass-provided: the URL placeholder inside the loader that
	// the loader.js previously read via fetch() (e.g. L"__MD_FILENAME__").
	// Kept for backward compatibility with loader.html files that
	// still reference the URL placeholder; new loaders may use
	// __FILE_CONTENT__ instead.
	virtual const std::wstring& filenamePlaceholder() const = 0;

private:
	static std::wstring Base64Encode(const std::vector<uint8_t>& bytes);
};
//------------------------------------------------------------------------