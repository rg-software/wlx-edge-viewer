#pragma once

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

#define CMD_NAVIGATE 0
#define LISTPLUGIN_OK	0
#define LISTPLUGIN_ERROR	1
#define INI_NAME L"edgeviewer.ini"

namespace fs = std::filesystem;
using ViewCtrlPtr = wil::com_ptr<ICoreWebView2Controller>;
using ViewPtr = wil::com_ptr<ICoreWebView2>;
using ViewsMap = std::map<HWND, ViewCtrlPtr>;

inline std::string to_utf8(const std::wstring& in)
{
    std::string out;
    int len = WideCharToMultiByte(CP_UTF8, 0, in.c_str(), int(in.size()), NULL, 0, 0, 0);
    out.resize(len);
    WideCharToMultiByte(CP_UTF8, 0, in.c_str(), int(in.size()), &out[0], len, 0, 0);
    return out;
}

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
    //std::string OurIniPathUtf8()
    //{
    //    return to_utf8(OurIniPath());
    //}
};
