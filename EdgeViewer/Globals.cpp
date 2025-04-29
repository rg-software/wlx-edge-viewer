#include "Globals.h"

ViewsMap gs_Views;
HINSTANCE gs_pluginInstance;
bool gs_isDarkMode;
double gs_ZoomFactor = 1.0;
std::vector<std::wstring> gs_tempFiles;

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
// resolve links to make sure we have the actual target location
std::wstring GetPhysicalPathForLink(const std::wstring& path)
{
	HANDLE hFile = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		                       nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);

	if (hFile == INVALID_HANDLE_VALUE)
		return L""; // Could not open the path

	wchar_t buffer[MAX_PATH];
	DWORD result = GetFinalPathNameByHandleW(hFile, buffer, MAX_PATH, FILE_NAME_NORMALIZED);
	CloseHandle(hFile);

	if (result == 0 || result >= MAX_PATH)
		return L"";

    return std::wstring(buffer);
}
//------------------------------------------------------------------------
// If UNC path is returned, copy to temp location and return local path
std::wstring GetPhysicalPath(const std::wstring& path)
{
	std::wstring realPath = GetPhysicalPathForLink(path);

	const std::wstring uncPrefix = L"\\\\?\\UNC\\";
	const std::wstring extendedPrefix = L"\\\\?\\";

	// for UNC files return a path to a temp copy
	if (realPath.starts_with(uncPrefix) && !fs::is_directory(realPath))
	{
		wchar_t tempPath[MAX_PATH], tempFile[MAX_PATH];
		GetTempPathW(MAX_PATH, tempPath);
		GetTempFileNameW(tempPath, L"UNC", 0, tempFile);
        wcscat_s(tempFile, fs::path(realPath).extension().c_str()); // keep the original extension
        gs_tempFiles.push_back(tempFile);

		return CopyFileW(path.c_str(), tempFile, FALSE) ? std::wstring(tempFile) : L"";
	}

	// Strip "\\?\"
    return realPath.starts_with(extendedPrefix) ? realPath.substr(extendedPrefix.length()) : realPath;
}
//------------------------------------------------------------------------
void removeTempFiles()
{
    for (const auto& path : gs_tempFiles)
        fs::remove(path);
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
    std::ifstream t(path);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}
//------------------------------------------------------------------------
std::wstring expandPath(const std::wstring& path)
{
    wchar_t pathFinal[MAX_PATH];
    ExpandEnvironmentStrings(path.c_str(), pathFinal, MAX_PATH); // so we can use any %ENV_VAR%
    return pathFinal;
}
//------------------------------------------------------------------------
