#include "HtmlProcessor.h"
#include "../Globals.h"
#include <format>
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

	// CSS is selected on the JS side via the document-created listener
	// registered by WebView2Backend::AddApplyStyleScript(). The HTML file is
	// loaded directly via the local.example virtual host; the engine's own
	// charset sniffing (BOM / <meta charset>) applies when no override is
	// requested.

	auto urlNoHost = urlPathW(mPath.relative_path());
	auto urlFull = std::format(L"http://local.example/{}", urlNoHost);
	webView.Navigate(urlFull);

	// Issue #66: HTML views can re-decode their source bytes. Must
	// follow Navigate, which resets the backend's flag for each load.
	webView.SetEncodingOverrideSupported(supportsEncodingOverride());
}

//------------------------------------------------------------------------
