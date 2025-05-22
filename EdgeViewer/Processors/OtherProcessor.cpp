#include "OtherProcessor.h"
#include <regex>
#include <shlwapi.h>
#include <wininet.h>
//------------------------------------------------------------------------
namespace { OtherProcessor other; }
//------------------------------------------------------------------------
bool OtherProcessor::InitPath(const fs::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "Other");
}
//------------------------------------------------------------------------
void OtherProcessor::OpenIn(ViewPtr webView) const
{
	mapDomains(webView, mPath.root_path());
	
	// note: CSS is applied via DOMContentLoaded script
	// (not sure we need it though)

	auto urlNoHost = urlPath(mPath.relative_path());

	auto urlFull = std::format("http://local.example/{}", urlNoHost);
	webView->Navigate(to_utf16(urlFull).c_str());
}
//------------------------------------------------------------------------
