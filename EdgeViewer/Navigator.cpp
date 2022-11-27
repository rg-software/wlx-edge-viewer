#include "Navigator.h"

// must include all processor headers to initialize them
#include "Processors/AdocProcessor.h"
#include "Processors/MdProcessor.h"
#include "Processors/HtmlProcessor.h"

#include "mini/ini.h"
#include <sstream>

//------------------------------------------------------------------------
void Navigator::Open(const fs::path& path) const
{
	ProcessorInterface::RegistryLoadAndOpen(path, mWebView);
}
//------------------------------------------------------------------------
void Navigator::Search(const std::wstring& str, int params) const
{
	auto aCaseSensitive = (params & lcs_matchcase) ? L"true" : L"false";
	auto aBackwards = (params & lcs_backwards) ? L"true" : L"false";
	auto aWholeWord = (params & lcs_wholewords) ? L"true" : L"false";

	// syntax: find(aString, aCaseSensitive, aBackwards, aWrapAround, aWholeWord, aSearchInFrames, aShowDialog)
	// returns true if found
	auto script = std::format(L"window.find('{}', {}, {}, false, {}, false, false);", str, aCaseSensitive, aBackwards, aWholeWord);

	if (params & lcs_findfirst)
	{
		// special case: need to go back till the beginning
		// (there is no way in Chromium to reset search, so we will search backwards until the string cannot be found anymore)
		script = std::format(L"while(window.find('{}', {}, !{}, false, {}, false, false));", str, aCaseSensitive, aBackwards, aWholeWord);
	}
	mWebView->ExecuteScript(script.c_str(), NULL);
}
//------------------------------------------------------------------------
void Navigator::Print() const
{
	mWebView->ExecuteScript(L"window.print();", NULL);
}
//------------------------------------------------------------------------
