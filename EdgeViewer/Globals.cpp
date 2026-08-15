#include "Globals.h"
#include "Platform.h"

#include <fstream>
#include <sstream>
#include <format>

std::map<HWND, std::shared_ptr<IWebView>> gs_Views;
HINSTANCE gs_PluginInstance;
bool gs_IsDarkMode;
std::map<const ProcessorInterface*, double> gs_ZoomFactor;
std::vector<std::wstring> gs_tempFiles;
//------------------------------------------------------------------------
std::string to_utf8(const std::wstring& in)
{
	// suggested on Windows by Microsoft
    std::string out;
    int len = WideCharToMultiByte(CP_UTF8, 0, in.c_str(), static_cast<int>(in.size()), nullptr, 0, nullptr, nullptr);
    if (len > 0)
    {
        out.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, in.c_str(), static_cast<int>(in.size()), out.data(), len, nullptr, nullptr);
    }
    return out;
}
//------------------------------------------------------------------------
std::wstring to_utf16(const std::string& in)
{
    std::wstring out;
    int len = MultiByteToWideChar(CP_UTF8, 0, in.c_str(), int(in.size()), nullptr, 0);
    if (len > 0)
    {
        out.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, in.c_str(), int(in.size()), &out[0], len);
    }
    return out;
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
        mINI::INIFile file(to_utf8(iniPath / INI_NAME));
        file.read(ini);

        if (!ini["WebView"].has("UserDir"))
            ini["WebView"]["UserDir"] = to_utf8(iniPath);
    }

    return ini;
}
//------------------------------------------------------------------------
std::string ReadFile(const std::wstring& path)  // presumed to be utf-8
{
    std::ifstream t(path);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}
//------------------------------------------------------------------------
