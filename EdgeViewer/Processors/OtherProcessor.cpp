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

	// Network shares cannot be served through the local.example virtual host
	// (local folders only); route them through the host-side evh:// scheme
	// whose WebResourceRequested handler reads the share in the plugin process
	// (issue #77). Linux needs no special case -- ev:// always reads host-side.
	auto urlNoHost = urlPath(mPath.relative_path());

#ifdef _WIN32
	const std::wstring scheme = IsNetworkPath(mPath)
		? L"evh://local.example/" : L"http://local.example/";
#else
	const std::wstring scheme = L"http://local.example/";
#endif
	auto urlFull = std::format(L"{}{}", scheme, to_utf16(urlNoHost));
	webView.Navigate(urlFull.c_str());
}
//------------------------------------------------------------------------
