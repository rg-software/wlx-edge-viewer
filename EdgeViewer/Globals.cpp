#include "Globals.h"

ViewsMap gs_Views;
HINSTANCE gs_pluginInstance;

//------------------------------------------------------------------------
std::string to_utf8(const std::wstring& in)
{
	// suggested on Windows by Microsoft
    std::string out;
    int len = WideCharToMultiByte(CP_UTF8, 0, in.c_str(), int(in.size()), NULL, 0, 0, 0);
    if (len > 0)
    {
        out.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, in.c_str(), int(in.size()), &out[0], len, 0, 0);
    }
    return out;
}
//------------------------------------------------------------------------
std::wstring to_utf16(const std::string& in)
{
    std::wstring out;
    int len = MultiByteToWideChar(CP_UTF8, 0, in.c_str(), int(in.size()), NULL, 0);
    if (len > 0)
    {
        out.resize(len);
        MultiByteToWideChar(CP_UTF8, 0, in.c_str(), int(in.size()), &out[0], len);
    }
    return out;
}
//------------------------------------------------------------------------
std::wstring GetModulePath()	// keep backslash at the end
{
	wchar_t iniFilePath[MAX_PATH];
	GetModuleFileName(gs_pluginInstance, iniFilePath, MAX_PATH);
	wcsrchr(iniFilePath, L'\\')[1] = L'\0';

	return iniFilePath;
}
//------------------------------------------------------------------------
mINI::INIStructure& gs_Ini()
{
    static mINI::INIStructure ini;
    
    if (ini.size() == 0)
    {
        auto iniPath = fs::path(GetModulePath());
        mINI::INIFile file(to_utf8(iniPath / INI_NAME));
        file.read(ini);

        if (!ini["Chromium"].has("UserDir"))
            ini["Chromium"]["UserDir"] = to_utf8(iniPath);
    }
    
    return ini;
}
//------------------------------------------------------------------------
std::string readFile(const std::wstring& path)  // presumed to be utf-8
{
    std::wifstream t(path);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}
//------------------------------------------------------------------------
