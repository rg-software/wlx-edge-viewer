#include "AdocProcessor.h"
#include <regex>

//------------------------------------------------------------------------
std::wstring AdocProcessor::Adoc() const
{ 
	const auto& adIni = gs_Ini.get("AsciiDoc");
	fs::path adoctorDir = fs::path(GetModulePath()) / L"asciidoctor";
	std::string loader(readFile(adoctorDir / L"loader.html"));
	loader = std::regex_replace(loader, std::regex("__CSS_NAME__"), adIni.get("CSS"));
	loader = std::regex_replace(loader, std::regex("__ADOC_FILENAME__"), to_utf8(mPath.filename()));

	return to_utf16(loader);	// NavigateToString() needs UTF16
}
//------------------------------------------------------------------------
