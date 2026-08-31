#include "OtherProcessor.h"
#include "../Globals.h"
#include <format>
//------------------------------------------------------------------------
namespace { OtherProcessor other; }
//------------------------------------------------------------------------
bool OtherProcessor::InitPath(const std::filesystem::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "Other");
}
//------------------------------------------------------------------------
void OtherProcessor::OpenIn(IWebView& webView) const
{
	mapDomains(webView, mPath.root_path());
	
	// note: CSS is applied via DOMContentLoaded script
	// (not sure we need it though)

	auto urlNoHost = urlPath(mPath.relative_path());

	auto urlFull = std::format("http://local.example/{}", urlNoHost);
	webView.Navigate(to_utf16(urlFull).c_str());
}
//------------------------------------------------------------------------
