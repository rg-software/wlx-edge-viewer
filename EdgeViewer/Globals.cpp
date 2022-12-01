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
std::string readFile(const std::wstring& path)
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
