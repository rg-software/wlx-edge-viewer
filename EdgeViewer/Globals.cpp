#include "Globals.h"

ViewsMap gs_Views;
mINI::INIStructure gs_Ini;
HINSTANCE gs_pluginInstance;
//------------------------------------------------------------------------
std::string to_utf8(const std::wstring& in)
{
	// suggested on Windows by Microsoft
    std::string out;
    int len = WideCharToMultiByte(CP_UTF8, 0, in.c_str(), int(in.size()), NULL, 0, 0, 0);
    out.resize(len);
    WideCharToMultiByte(CP_UTF8, 0, in.c_str(), int(in.size()), &out[0], len, 0, 0);
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
