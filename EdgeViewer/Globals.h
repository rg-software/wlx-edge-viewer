#pragma once

#include <mini/ini.h>
#include <windows.h>
#include <wil/com.h>
#include <webview2.h>
#include <filesystem>
#include <map>
#include <codecvt>


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
#define LISTPLUGIN_OK	0
#define LISTPLUGIN_ERROR	1
#define INI_NAME L"edgeviewer.ini"
#define EDGE_LISTER_CLASS "mdLister"
#ifdef _WIN64
#define BROWSER_FOLDER_KEY "BrowserExecutableX64Folder"
#else
#define BROWSER_FOLDER_KEY "BrowserExecutableX86Folder"
#endif

namespace fs = std::filesystem;
using ViewCtrlPtr = wil::com_ptr<ICoreWebView2Controller>;
using ViewPtr = wil::com_ptr<ICoreWebView2>;
using ViewsMap = std::map<HWND, ViewCtrlPtr>;

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
extern ViewsMap gs_Views;
extern HINSTANCE gs_pluginInstance;
extern bool gs_isDarkMode;
//------------------------------------------------------------------------
std::string to_utf8(const std::wstring& in);
std::wstring to_utf16(const std::string& in);
mINI::INIStructure& gs_Ini();
std::wstring GetModulePath();
std::string readFile(const std::wstring& path);
std::wstring expandPath(const std::wstring& path);
//------------------------------------------------------------------------
