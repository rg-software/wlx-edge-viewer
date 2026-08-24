#pragma once

#include <mini/ini.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
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
#ifdef _WIN32
// Per-build ini key pinning a specific browser executable folder
// ([WebView] section); absent key = auto-detect Edge (see WebViewFactory).
#ifdef _WIN64
#define BROWSER_FOLDER_KEY "BrowserExecutableX64Folder"
#else
#define BROWSER_FOLDER_KEY "BrowserExecutableX86Folder"
#endif
#endif

namespace fs = std::filesystem;

class IWebView;
class ProcessorInterface;
// gs_Views owns the IWebView backend via shared_ptr so the entry stays
// alive for the lifetime of the lister window. EdgeLister_Win's WndProc
// and ListCloseWindow read gs_Views[hwnd] after the WebView2 factory's
// async callback has fired.
//
// The key is the opaque lister handle: HWND on Windows, GtkWidget* on
// Linux (Double Commander passes the parent widget pointer). void* keeps
// the shared header free of platform types.
extern std::map<void*, std::shared_ptr<IWebView>> gs_Views;
#ifdef _WIN32
extern HINSTANCE gs_PluginInstance;
#endif
extern bool gs_IsDarkMode;
extern std::map<const ProcessorInterface*, double> gs_ZoomFactor;
extern std::vector<std::wstring> gs_tempFiles;
//------------------------------------------------------------------------
struct ListDefaultParamStruct
{
	int size;
	uint32_t PluginInterfaceVersionLow;
	uint32_t PluginInterfaceVersionHi;
	char DefaultIniName[260];
};
//------------------------------------------------------------------------
std::string to_utf8(const std::wstring& in);
std::wstring to_utf16(const std::string& in);
int to_int(const std::string& in);
mINI::INIStructure& GlobalSettings();
std::string ReadFile(const fs::path& path);
//------------------------------------------------------------------------
// Attachment save support (JS->host CMD_SAVE). Decode + filename
// sanitize + disk write are portable C++ shared by both backends; only
// the folder picker is per-OS (see Platform.h::PickFolder).
std::vector<uint8_t> DecodeBase64UrlSafe(const std::string& in);
std::wstring SanitizeAttachmentName(const std::wstring& name);
// Returns true on success, false on failure (empty folder or write error).
bool SaveAttachmentToFolder(const std::wstring& folder, const std::wstring& filename,
                            std::vector<uint8_t>& bytes);
// Build the JS that reports a CMD_SAVE result back to the EML loader via
// the loader-registered `window.__emlSaveResult(status, message)` callback.
// The `&&` guard makes it a no-op when the callback is absent. status is
// "ok", "cancel" or "error"; message is shown verbatim in the view.
std::wstring BuildSaveResultScript(const std::wstring& status, const std::wstring& message);
//------------------------------------------------------------------------
