#include "Navigator.h"
#include "Processors/ProcessorRegistry.h"
#include <format>

//------------------------------------------------------------------------
std::wstring jsEscape(const std::wstring& str)
{
	std::wstring output;
	for (wchar_t ch : str)
	{
		if (ch == L'\\') output += L"\\\\";
		else if (ch == L'\'') output += L"\\'";
		else output += ch;
	}
	return output;
}
//------------------------------------------------------------------------
std::wstring BuildFindScript(const std::wstring& pattern, int params)
{
	auto aCaseSensitive = (params & lcs_matchcase) ? L"true" : L"false";
	auto aBackwards = (params & lcs_backwards) ? L"true" : L"false";
	auto aWholeWord = (params & lcs_wholewords) ? L"true" : L"false";

	// syntax: find(aString, aCaseSensitive, aBackwards, aWrapAround, aWholeWord, aSearchInFrames, aShowDialog)
	// returns true if found
	auto script = std::format(L"window.find('{}', {}, {}, false, {}, false, false);", jsEscape(pattern), aCaseSensitive, aBackwards, aWholeWord);

	if (params & lcs_findfirst)
	{
		// special case: need to go back till the beginning
		// (there is no way in Chromium to reset search, so we will search backwards until the string cannot be found anymore)
		script = std::format(L"while(window.find('{}', {}, !{}, false, {}, false, false));", jsEscape(pattern), aCaseSensitive, aBackwards, aWholeWord);
	}

	return script;
}
//------------------------------------------------------------------------
std::wstring BuildPrintScript()
{
	return L"window.print();";
}
//------------------------------------------------------------------------

//------------------------------------------------------------------------
void Navigator::Open(const std::filesystem::path& path) const
{
	// use path-specific (essentially file type-specific) load and open procedure
	gsProcRegistry().LoadAndOpen(path, mWebView);
}
//------------------------------------------------------------------------
void Navigator::Search(const std::wstring& str, int params) const
{
	mWebView.ExecuteScript(BuildFindScript(str, params).c_str());
}
//------------------------------------------------------------------------
void Navigator::Print() const
{
	mWebView.Print();
}
//------------------------------------------------------------------------
