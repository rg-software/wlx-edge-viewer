#include "DirProcessor.h"
#include <shlwapi.h>
#include <wininet.h>
#include <regex>
//------------------------------------------------------------------------
namespace { DirProcessor dir; }
//------------------------------------------------------------------------
bool DirProcessor::InitPath(const fs::path& path)
{
	mPath = path;
	return atoi(gs_Ini()["Extensions"]["Dirs"].c_str()) && fs::is_directory(path);
}
//------------------------------------------------------------------------
void DirProcessor::OpenIn(ViewPtr webView) const
{ 
	mapDomains(webView, mPath.root_path());

	std::wstring wloader(to_utf16(readFile(assetsPath() / L"dirviewer" / L"loader.html")));
	wloader = std::regex_replace(wloader, std::wregex(L"__BASE_URL__"), urlPathW(mPath.relative_path()));
	wloader = std::regex_replace(wloader, std::wregex(L"__BODY__"), genBody(mPath));

	webView->NavigateToString(wloader.c_str());
}
//------------------------------------------------------------------------
std::wstring DirProcessor::genBody(const fs::path& path) const
{
	auto exts = gs_Ini()["Extensions"]["DirImageExt"];
	auto mask = std::format(L".+\\.({})$", to_utf16(exts));
	auto re = std::wregex(mask, std::regex_constants::icase);
	std::wstringstream ss;

	for (auto const& dir_entry : fs::directory_iterator(path))
		if (dir_entry.is_regular_file())
		{
			auto fname = dir_entry.path().filename().wstring();

			if (std::regex_match(fname, re))
				ss << std::format(L"<a href=\"{0}\" class=\"thumbnail-viewer\">"
								  L"<img class=\"thumbnail\" src=\"{0}\" alt=\"{0}\">", fname).c_str();
		}
	
	return ss.str();
}
//------------------------------------------------------------------------
