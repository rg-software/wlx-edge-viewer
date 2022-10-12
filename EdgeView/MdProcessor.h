#pragma once

#include "CommonTypes.h"
#include "TextEncodingDetect/text_encoding_detect.h"
#include <string>
#include <codecvt>
#include <mutex>
#include <fstream>

extern "C" int hoedown_main(int argc, const char** argv);
extern "C" int smartypants_main(int argc, const char** argv);
extern "C" int smartypants_main_null(int argc, const char** argv);

extern "C" const char* INPUT_STRING;
extern "C" const char* SP_INPUT_STRING;
extern "C" char* OUTPUT_STRING;
extern "C" char* SP_OUTPUT_STRING;

using namespace AutoIt::Common;

#pragma warning(push)
#pragma warning(disable:4996)

class MdProcessor
{
public:
	MdProcessor(const std::wstring& path) : mPath(path) {}
	std::wstring Markdown() const;

private:
	template<typename T> static std::vector<T> readFileChar(const std::wstring& path, TextEncodingDetect::Encoding e = TextEncodingDetect::None)
	{
		std::basic_ifstream<T> in(path, std::ios::binary);

		if (e == TextEncodingDetect::UTF16_LE_BOM || e == TextEncodingDetect::UTF16_BE_BOM)
			in.imbue(std::locale(in.getloc(), new std::codecvt_utf16<wchar_t, 0x10ffff, std::consume_header>));

		std::istreambuf_iterator<T> it(in);

		return std::vector<T>(it, {});
	}

	static std::string readFile(const std::wstring& path);

	static std::mutex mHoedownLock;
	const std::wstring mPath;
};

#pragma warning(pop)
