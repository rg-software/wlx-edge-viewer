#pragma once

#include <mini/ini.h>
#include <windows.h>
#include <wil/com.h>
#include <webview2.h>
#include <filesystem>
#include <map>

//#define lc_copy			1
//#define lc_newparams	2
//#define lc_selectall	3
////#define lc_ieview_paste	0xFFFF0001
//
//#define lcp_wraptext	1
//#define lcp_fittowindow 2
//#define lcp_ansi		4
//#define lcp_ascii		8
//#define lcp_variable	12
//
//#define lcs_findfirst	1
//#define lcs_matchcase	2
//#define lcs_wholewords	4
//#define lcs_backwards	8

#define WM_WEBVIEW_KEYDOWN WM_USER
#define CMD_NAVIGATE 0
#define LISTPLUGIN_OK	0
#define LISTPLUGIN_ERROR	1
#define INI_NAME L"edgeviewer.ini"
#define EDGE_LISTER_CLASS "mdLister"

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
extern mINI::INIStructure gs_Ini;
extern HINSTANCE gs_pluginInstance;
//------------------------------------------------------------------------
std::string to_utf8(const std::wstring& in);
std::wstring to_utf16(const std::string& in);
std::wstring GetModulePath();
//------------------------------------------------------------------------
