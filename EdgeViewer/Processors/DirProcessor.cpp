#include "DirProcessor.h"
#include <shlwapi.h>
#include <wininet.h>
#include <regex>
#include <vector>
#include <algorithm>
//------------------------------------------------------------------------
namespace { DirProcessor dir; }
//------------------------------------------------------------------------
fs::path DirProcessor::stripTwodots(const fs::path& path) const
{
	std::wstring spath = path;
	return spath.ends_with(L"..\\") ? spath.substr(0, spath.length() - 3) : spath;
}
//------------------------------------------------------------------------
bool DirProcessor::InitPath(const fs::path& path)
{
	mPath = GetPhysicalPath(stripTwodots(path));
	return to_int(GlobalSettings()["Extensions"]["Dirs"]) && fs::is_directory(mPath);
}
//------------------------------------------------------------------------
void DirProcessor::OpenIn(ViewPtr webView) const
{ 
	mapDomains(webView, mPath.root_path());

	std::wstring wloader(to_utf16(ReadFile(assetsPath() / L"dirviewer" / L"loader.html")));
	wloader = std::regex_replace(wloader, std::wregex(L"__BASE_URL__"), urlPathW(mPath.relative_path()));
	wloader = std::regex_replace(wloader, std::wregex(L"__BODY__"), genBody(mPath));

	webView->NavigateToString(wloader.c_str());
}
//------------------------------------------------------------------------
std::wstring DirProcessor::genBody(const fs::path& path) const
{
	auto exts = GlobalSettings()["Extensions"]["DirImageExt"];
	auto mask = std::format(L".+\\.({})$", to_utf16(exts));
	auto re = std::wregex(mask, std::regex_constants::icase);
	auto folderThumb = L"http://assets.example/dirviewer/folder.png";
	std::wstringstream ss;

	std::vector<fs::directory_entry> entries;
	for (auto const& dir_entry : fs::directory_iterator(path))
		entries.push_back(dir_entry);

	// place directories before files, sort alphabetically
	std::sort(entries.begin(), entries.end(), 
		[](const fs::directory_entry& a, const fs::directory_entry& b)
		{
			if (a.is_directory() != b.is_directory())
				return a.is_directory();
			return a.path().filename().wstring() < b.path().filename().wstring();
		});

	for (auto const& dir_entry : entries)
	{
		auto fname = dir_entry.path().filename().wstring();
		
		// skip . and ..
		if (fname == L"." || fname == L"..")
			;
		else if (dir_entry.is_directory())
		{
			ss << std::format(L"<div class=\"directory-viewer\" data-name=\"{0}\">" 
							  L"<img class=\"thumbnail\" src=\"{1}\" alt=\"{0}\"></div>", fname, folderThumb).c_str();
		}
		else if (dir_entry.is_regular_file() && std::regex_match(fname, re))
		{
			ss << std::format(L"<a href=\"{0}\" class=\"thumbnail-viewer\">"
							  L"<img class=\"thumbnail\" src=\"{0}\" alt=\"{0}\"></a>", fname).c_str();
		}
	}
	
	return ss.str();
}
//------------------------------------------------------------------------
