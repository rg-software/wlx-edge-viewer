#pragma once

#include <mini/ini.h>
#include <windows.h>
#include <filesystem>
#include <map>
#include <codecvt>
#include <string>
#include <vector>

#include "Platform.h"
#include "IWebView.h"

// #define lc_copy			1
// #define lc_newparams	2
// #define lc_selectall	3
// #define lc_ieview_paste	0xFFFF0001
// #define lcp_wraptext	1
// #define lcp_fittowindow 2
// #define lcp_ansi		4
// #define lcp_ascii		8
// #define lcp_variable	12
// #define lcp_darkmodenative 256

#define lcs_findfirst	1
#define lcs_matchcase	2
#define lcs_wholewords	4
#define lcs_backwards	8
#define lcp_darkmode    128

#define WM_WEBVIEW_KEYDOWN WM_USER
#define WM_WEBVIEW_JS_KEYDOWN (WM_USER + 1)
#define CMD_NAVIGATE 0
#define CMD_PRINT 1
#define CMD_SEARCH 2
#define CMD_MENU 3
#define LISTPLUGIN_OK	0
#define LISTPLUGIN_ERROR	1
#define INI_NAME L"edgeviewer.ini"
#define EDGE_LISTER_CLASS "mdLister"

namespace fs = std::filesystem;

class IWebView;
class ProcessorInterface;
extern std::map<HWND, IWebView*> gs_Views;
extern HINSTANCE gs_PluginInstance;
extern bool gs_IsDarkMode;
extern std::map<const ProcessorInterface*, double> gs_ZoomFactor;
extern std::vector<std::wstring> gs_tempFiles;
//------------------------------------------------------------------------
struct ListDefaultParamStruct
{
	int size;
	DWORD PluginInterfaceVersionLow;
	DWORD PluginInterfaceVersionHi;
	char DefaultIniName[MAX_PATH];

	std::wstring OurIniPath()
	{
		return fs::path(DefaultIniName).parent_path() / INI_NAME;
	}
};
//------------------------------------------------------------------------
std::string to_utf8(const std::wstring& in);
std::wstring to_utf16(const std::string& in);
int to_int(const std::string& in);
mINI::INIStructure& GlobalSettings();
std::string ReadFile(const std::wstring& path);
//------------------------------------------------------------------------
