#include "Globals.h"
#include "Platform.h"

#include <fstream>
#include <sstream>
#include <format>
#include <codecvt>
#include <locale>

std::map<void*, std::shared_ptr<IWebView>> gs_Views;
#ifdef _WIN32
HINSTANCE gs_PluginInstance;
#endif
bool gs_IsDarkMode;
std::map<const ProcessorInterface*, double> gs_ZoomFactor;
std::vector<std::wstring> gs_tempFiles;
//------------------------------------------------------------------------
std::string to_utf8(const std::wstring& in)
{
	// C++23 still deprecates wstring_convert, but both toolchains (MSVC
	// stdcpplatest, GCC -std=c++23) accept it; it is the only
	// <codecvt> entry point that works on both platforms.
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	return conv.to_bytes(in);
}
//------------------------------------------------------------------------
std::wstring to_utf16(const std::string& in)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	return conv.from_bytes(in);
}
//------------------------------------------------------------------------
int to_int(const std::string& in)
{
    return atoi(in.c_str());
}
//------------------------------------------------------------------------
void RemoveTempFiles()
{
    for (const auto& path : gs_tempFiles)
        fs::remove(path);
}
//------------------------------------------------------------------------
mINI::INIStructure& GlobalSettings()
{
    static mINI::INIStructure ini;

    if (ini.size() == 0)
    {
        auto iniPath = fs::path(GetModulePath());
        mINI::INIFile file(to_utf8((iniPath / INI_NAME).wstring()));
        file.read(ini);

        if (!ini["WebView"].has("UserDir"))
            ini["WebView"]["UserDir"] = to_utf8(iniPath.wstring());
    }

    return ini;
}
//------------------------------------------------------------------------
std::string ReadFile(const fs::path& path)  // presumed to be utf-8
{
    std::ifstream t(path);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}
//------------------------------------------------------------------------
