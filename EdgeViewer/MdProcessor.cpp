#include "MdProcessor.h"
#include "mini/ini.h"

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

	// we have converted everything to utf8 (but the detector can still return None or ANSI)
	// if the file has BOM, we need to add the meta field
	auto e = TextEncodingDetect().DetectEncoding((unsigned char*)utf8_text.c_str(), utf8_text.size());
	std::string meta;
	if (e == TextEncodingDetect::UTF8_BOM || e == TextEncodingDetect::UTF8_NOBOM)
		meta = "<meta charset='utf-8'>";

	std::vector<std::string> hoedown_args_list;
	std::istringstream f(gs_Ini["Hoedown"]["HoedownArgs"]);
	std::string s;

	while (f >> s) // space-separated list
		hoedown_args_list.push_back(s);

	const char* hoedown_argv[512];
	for (int i = 0; i < hoedown_args_list.size(); ++i)
		hoedown_argv[i] = hoedown_args_list[i].c_str();

	std::scoped_lock lock(mHoedownLock);

	hoedown_main(int(hoedown_args_list.size()), hoedown_argv);

	SP_INPUT_STRING = OUTPUT_STRING;

	const char* smartypants_argv[] = { "smartypants" };

	if (std::stoi(gs_Ini["Hoedown"]["UseSmartypants"]))	// should use smartypants
		smartypants_main(1, smartypants_argv);
	else
		smartypants_main_null(1, smartypants_argv);

	auto cssName = gs_Ini["Hoedown"]["CustomCSS"];
	fs::path cssPath = fs::path(GetModulePath()) / std::wstring(cssName.begin(), cssName.end());
	std::string css(readFile(cssPath));
	auto r = std::format("<HTML><HEAD>{}<base href=http://local.example></base><style>{}</style></HEAD><BODY>{}</BODY></HTML>",
		meta, css, SP_OUTPUT_STRING);

	// calloc() is called on the hoedown/smartypants side
	free(OUTPUT_STRING);
	free(SP_OUTPUT_STRING);

	return to_utf16(r);	// NavigateToString() needs UTF16
}
//------------------------------------------------------------------------
std::string MdProcessor::readFile(const std::wstring& path)
{
	auto buffer = readFileChar<char>(path);

	auto e = TextEncodingDetect().DetectEncoding((unsigned char*)&buffer[0], buffer.size());
	if (e == TextEncodingDetect::UTF16_LE_BOM || e == TextEncodingDetect::UTF16_LE_NOBOM ||
		e == TextEncodingDetect::UTF16_BE_BOM || e == TextEncodingDetect::UTF16_BE_NOBOM)
	{
		// convert UTF16 input into UTF8
		auto wr = readFileChar<wchar_t>(path, e);
		return to_utf8(std::wstring(wr.begin(), wr.end()));
	}

	return std::string(buffer.begin(), buffer.end());
}
//------------------------------------------------------------------------
