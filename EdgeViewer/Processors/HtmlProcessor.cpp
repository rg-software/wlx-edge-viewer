#include "HtmlProcessor.h"
#include <regex>
#include <shlwapi.h>
#include <wininet.h>
//------------------------------------------------------------------------
namespace { HtmlProcessor html; }
//------------------------------------------------------------------------
bool HtmlProcessor::InitPath(const fs::path& path)
{
	mPath = GetPhysicalPath(path);
	return isType(path.extension(), "HTML");
}
//------------------------------------------------------------------------
void HtmlProcessor::OpenIn(ViewPtr webView) const
{
	mapDomains(webView, mPath.root_path());
	
	// note: CSS is applied via DOMContentLoaded script

	auto urlNoHost = urlPath(mPath.relative_path());
	auto domain = "local";

	if (to_int(GlobalSettings()["HTML"]["DetectEncoding"]))
	{
		// this domain is not mounted and thus can be intercepted in WebResourceRequested()
		domain = "html";

		// keep the real path and new encoding scheme
		auto charset = detectedCharset(mPath);
		gs_Htmls[to_utf16(urlNoHost)] = { mPath.c_str(), std::wstring(charset.begin(), charset.end()) };
	}

	auto urlFull = std::format("http://{}.example/{}", domain, urlNoHost);
	webView->Navigate(to_utf16(urlFull).c_str());

}
//------------------------------------------------------------------------
std::string HtmlProcessor::detectedFromBom(const fs::path& filePath)
{
	unsigned char bom[3] = { 0 };
	std::ifstream file(filePath, std::ios::binary);
	file.read(reinterpret_cast<char*>(bom), 3);

	if (bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF)
		return "UTF-8";
	else if (bom[0] == 0xFF && bom[1] == 0xFE)
		return "UTF-16LE";
	else if (bom[0] == 0xFE && bom[1] == 0xFF)
		return "UTF-16BE";

	return "";
}
//------------------------------------------------------------------------
std::string HtmlProcessor::detectedFromMeta(const fs::path& filePath)
{
	std::ifstream file(filePath);
	std::string line;
	
	std::regex charsetRegex(R"(<meta[^>]*charset\s*=\s*["']?([^"'>\s]+))", std::regex::icase);
	std::regex contentRegex(R"(<meta[^>]*content\s*=\s*["'][^"']*charset=([^"'>\s]+))", std::regex::icase);
	std::smatch match;

	while (std::getline(file, line))
	{
		if (std::regex_search(line, match, charsetRegex))
			return match[1];
		if (std::regex_search(line, match, contentRegex))
			return match[1];
	}

	return "";
}
//------------------------------------------------------------------------
std::string HtmlProcessor::detectedCharset(const fs::path& path)
{
	std::string r = detectedFromBom(path);
	return r.length() > 0 ? r : detectedFromMeta(path);
}
//------------------------------------------------------------------------
