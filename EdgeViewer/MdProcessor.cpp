#include "MdProcessor.h"
#include "mini/ini.h"
#include <regex>

extern "C" int hoedown_main(int argc, const char** argv);
extern "C" int smartypants_main(int argc, const char** argv);
extern "C" int smartypants_main_null(int argc, const char** argv);

extern "C" const char* INPUT_STRING;
extern "C" const char* SP_INPUT_STRING;
extern "C" char* OUTPUT_STRING;
extern "C" char* SP_OUTPUT_STRING;

const char* INPUT_STRING;
const char* SP_INPUT_STRING;
char* OUTPUT_STRING;
char* SP_OUTPUT_STRING;

std::mutex MdProcessor::mHoedownLock;
//------------------------------------------------------------------------
std::wstring MdProcessor::Markdown() const
{
	// functionality copied from the old plugin
	// hoedown needs utf-8 as an input

	std::string utf8_text = readFile(mPath);
	INPUT_STRING = utf8_text.c_str();
	const auto& mdIni = gs_Ini.get("Markdown");

	// we have converted everything to utf8 (but the detector can still return None or ANSI)
	// if the file has BOM, we need to add the meta field
	auto e = TextEncodingDetect().DetectEncoding((unsigned char*)utf8_text.c_str(), utf8_text.size());
	std::string meta;
	if (e == TextEncodingDetect::UTF8_BOM || e == TextEncodingDetect::UTF8_NOBOM)
		meta = "<meta charset='utf-8'>";

	std::vector<std::string> hoedown_args_list = { "hoedown" };
	std::istringstream f(mdIni.get("HoedownArgs"));
	std::string s;

	while (f >> s) // space-separated list
		hoedown_args_list.push_back(s);

	const char* hoedown_argv[512];
	for (size_t i = 0; i < hoedown_args_list.size(); ++i)
		hoedown_argv[i] = hoedown_args_list[i].c_str();

	std::scoped_lock lock(mHoedownLock);

	hoedown_main(int(hoedown_args_list.size()), hoedown_argv);

	SP_INPUT_STRING = OUTPUT_STRING;

	const char* smartypants_argv[] = { "smartypants" };

	if (std::stoi(mdIni.get("UseSmartypants")))	// should use smartypants
		smartypants_main(1, smartypants_argv);
	else
		smartypants_main_null(1, smartypants_argv);

	fs::path mdDir = fs::path(GetModulePath()) / L"markdown";
	std::string loader(readFile(mdDir / L"loader.html"));
	loader = std::regex_replace(loader, std::regex("__META__"), meta);
	loader = std::regex_replace(loader, std::regex("__CSS_NAME__"), mdIni.get("CSS"));
	loader = std::regex_replace(loader, std::regex("__MARKDOWN_BODY__"), SP_OUTPUT_STRING);

	// calloc() is called on the hoedown/smartypants side
	free(OUTPUT_STRING);
	free(SP_OUTPUT_STRING);

	return to_utf16(loader);	// NavigateToString() needs UTF16
}
//------------------------------------------------------------------------
