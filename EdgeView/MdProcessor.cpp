#include "MdProcessor.h"
#include "mini/ini.h"

const char* INPUT_STRING;
const char* SP_INPUT_STRING;
char* OUTPUT_STRING;
char* SP_OUTPUT_STRING;

// TOFIX:
extern std::wstring GetModulePath();
extern mINI::INIStructure gs_Ini;

std::mutex MdProcessor::mHoedownLock;
//------------------------------------------------------------------------
std::wstring MdProcessor::Markdown() const
{
	// copy functionality from the old plugin (verbatim...)
	// hoedown needs utf-8 as an input, so we need to convert UTF16 docs

	std::string utf8_text = readFile(mPath);
	INPUT_STRING = utf8_text.c_str();

	// we have converted everything to utf8 (but the detector can still return None or ANSI)
	TextEncodingDetect::Encoding e = TextEncodingDetect().DetectEncoding((unsigned char*)utf8_text.c_str(), utf8_text.size());
	std::string meta;
	if (e == TextEncodingDetect::UTF8_BOM || e == TextEncodingDetect::UTF8_NOBOM)
		meta = "<meta charset='utf-8'>";

	//char hoedown_args[512];
	//char smartypants_args[512];
	//char html_template[512];

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
	std::string file_url = "http://local.example";
	std::string css(readFile(cssPath));
	std::string result = "<HTML><HEAD>" + meta + "<base href=\"" +
		std::string(file_url) + "\"></base><style>" +
		css + "</style></HEAD><BODY>" +
		std::string(SP_OUTPUT_STRING) +
		"</BODY></HTML>";

	// calloc() is called on the hoedown/smartypants side
	free(OUTPUT_STRING);
	free(SP_OUTPUT_STRING);

	return std::wstring(result.begin(), result.end()); // TODO: double check it works correctly!!!
}
//------------------------------------------------------------------------
std::string MdProcessor::readFile(const std::wstring& path)
{
	auto buffer = readFileChar<char>(path);

	TextEncodingDetect::Encoding e = TextEncodingDetect().DetectEncoding((unsigned char*)&buffer[0], buffer.size());
	if (e == TextEncodingDetect::UTF16_LE_BOM || e == TextEncodingDetect::UTF16_LE_NOBOM ||
		e == TextEncodingDetect::UTF16_BE_BOM || e == TextEncodingDetect::UTF16_BE_NOBOM)
	{
		auto wr = readFileChar<wchar_t>(path, e);
		return to_utf8(std::wstring(wr.begin(), wr.end()));
		//boost::nowide::narrow(std::wstring(wr.begin(), wr.end()));
	}

	return std::string(buffer.begin(), buffer.end());
}
//------------------------------------------------------------------------
