// TODO:
// 1) HTML files should be loaded with the current method, but file paths must be converted to URLs:
// 2) Markdown should be loaded with my previous approach (convert & load from string aka NavigateToString with base url)
// In the future it might be reasonable to use NavigateToString in all cases BUT now it would not have the right base url)
// 3) set focus on Webview only when there is a focus on our child window
// implement missing functions
// keypresses from webview
// markdown, etc.

#include "listerplugin.h"

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <wrl.h>
#include <wil/com.h>
#include <webview2.h>
#include <webview2environmentoptions.h>
#include <wininet.h>
#include <string>
#include <format>
#include <filesystem>
#include <map>
#include <mutex>
#include <fstream>

#define CMD_NAVIGATE 0
//#define CMD_LOADSTRING 1

namespace fs = std::filesystem;
using namespace Microsoft::WRL;
using ViewCtrlPtr = wil::com_ptr<ICoreWebView2Controller>;
using ViewPtr = wil::com_ptr<ICoreWebView2>;

//------------------------------------------------------------------------
HINSTANCE gs_pluginInstance;
ListDefaultParamStruct gs_Config;
std::mutex gs_ViewCreateLock;
std::map<HWND, ViewCtrlPtr> gs_Views;
//------------------------------------------------------------------------
class Navigator
{
public:
	Navigator(ViewPtr webView) : mWebView(webView) {}
	
	void Open(const std::wstring& path_str)
	{
		auto path = fs::path(path_str);
		auto webview23 = mWebView.try_query<ICoreWebView2_3>();
		webview23->SetVirtualHostNameToFolderMapping(L"local.example", path.parent_path().c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);

		// TO REFACTOR
		if (path_str.ends_with(L"html") || path_str.ends_with(L"htm"))
		{
			auto script = std::format(L"window.location = 'http://local.example/{}';", path.filename().c_str());
			mWebView->ExecuteScript(script.c_str(), NULL);
		}
		else if (path_str.ends_with(L"markdown"))
		{
			// TODO: check unicode
			std::ifstream ifs(path_str);
			std::wstring str(std::istreambuf_iterator<char>{ifs}, {});
			mWebView->NavigateToString(str.c_str());
		}
	}

private:
	ViewPtr mWebView;

	//void mapVirtualHost(const std::wstring& dir)
	//{
	//}

	//std::wstring fileToUri(const std::wstring& FileToLoad)
	//{
	//	wchar_t url[INTERNET_MAX_URL_LENGTH];
	//	DWORD len = INTERNET_MAX_URL_LENGTH;
	//	UrlCreateFromPath(FileToLoad.c_str(), url, &len, NULL);
	//	return std::wstring(url);// , len);

	//	// alternatively, load as string
	//	//std::wstring uri = std::format(L"file:///{}", FileToLoad);
	//	//std::replace(std::begin(uri), std::end(uri), '\\', '/');
	//	//return uri;
	//}
};

LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (gs_Views.find(hWnd) != std::end(gs_Views))
	{
		ViewPtr webview;
		std::wstring strData;
		COPYDATASTRUCT* pcds = nullptr;
		gs_Views[hWnd]->get_CoreWebView2(&webview);

		switch (message)
		{
		case WM_SIZE:
			RECT bounds;
			GetClientRect(hWnd, &bounds);
			gs_Views[hWnd]->put_Bounds(bounds);
			break;

		case WM_COPYDATA:
			pcds = (COPYDATASTRUCT*)lParam;
			strData = std::wstring((wchar_t*)pcds->lpData);

			if (pcds->dwData == CMD_NAVIGATE)
				Navigator(webview).Open(strData);
			//{
			//	// we should decide here whether we navigate to url or load a text
			//	std::wstring script = std::format(L"window.location = '{}';", strData);
			//	webview->ExecuteScript(script.c_str(), NULL);
			//}
			//else if (pcds->dwData == CMD_LOADSTRING)
			//{
			//	webview->NavigateToString(strData.c_str());
			//}
			break;

		case WM_SETFOCUS:
			gs_Views[hWnd]->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
	}

	//case WM_KEYDOWN:
	//case WM_KEYUP:
	//	PostMessage(GetParent(hWnd), message, wParam, lParam);
	//	break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
//------------------------------------------------------------------------
BOOL APIENTRY DllMain(HINSTANCE hinst, unsigned long reason, void* lpReserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		gs_pluginInstance = hinst;
		WNDCLASSA wc = {};
		wc.hInstance = hinst;
		wc.lpfnWndProc = PluginWndProc;
		wc.lpszClassName = "mdLister";
		RegisterClassA(&wc);
	}

	return TRUE;
}
//------------------------------------------------------------------------
HRESULT CreateWebView2Environment(HWND hWnd, const std::wstring& userDir, const std::wstring& fileToLoad)
{
	//CoreWebView2EnvironmentOptions opts;
	// TODO: use SetVirtualHostNameToFolderMapping()
	auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
	options->put_AdditionalBrowserArguments(L"--allow-file-access-from-files");
	options->put_AdditionalBrowserArguments(L"--disable-web-security");
	//wil::com_ptr
	//ICoreWebView2EnvironmentOptions  var options = new CoreWebView2EnvironmentOptions("--allow-file-access-from-files");
	return CreateCoreWebView2EnvironmentWithOptions(nullptr, userDir.c_str(), options.Get(),// nullptr,
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([=](HRESULT result, ICoreWebView2Environment* env)
		{
			env->CreateCoreWebView2Controller(hWnd,
				Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([=](HRESULT result, ICoreWebView2Controller* controller)
				{
					if (controller != nullptr)
					{
						std::scoped_lock lock(gs_ViewCreateLock);
						gs_Views[hWnd] = ViewCtrlPtr(controller);
					}

					// Here we can change settings; check:
					// https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2settings

					// Resize WebView to fit the bounds of the parent window
					RECT bounds;
					GetClientRect(hWnd, &bounds);
					controller->put_Bounds(bounds);
					// controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);	// double check

					ViewPtr webview;
					controller->get_CoreWebView2(&webview);
					Navigator(webview).Open(fileToLoad);
					//webview->Navigate(uri.c_str());



					// Navigation events
					// register an ICoreWebView2NavigationStartingEventHandler to cancel any non-https navigation

					// EventRegistrationToken token;
					
					//webview->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
					//	[](ICoreWebView2* webview, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
					//		wil::unique_cotaskmem_string uri;
					//		args->get_Uri(&uri);
					//		std::wstring source(uri.get());
					//		if (source.substr(0, 5) != L"https") {
					//			args->put_Cancel(true);
					//		}
					//		return S_OK;
					//	}).Get(), &token);

					// Scripting
					// Schedule an async task to add initialization script that freezes the Object object
							
					// webview->AddScriptToExecuteOnDocumentCreated(L"Object.freeze(Object);", nullptr);
							
					// Schedule an async task to get the document URL
							
					// webview->ExecuteScript(L"window.document.URL;", Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
					//	[](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
					//		LPCWSTR URL = resultObjectAsJson;
					//		// doSomethingWithURL(URL);
					//		return S_OK;
					//	}).Get());
							
					// Communication between host and web content
					// Set an event handler for the host to return received message back to the web content
		

					//EventRegistrationToken token;
					// FOR SOME REASON THIS DOES NOT WORK
					 //webview->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
						//[](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args)
						//{
						//	wil::unique_cotaskmem_string message;
						//	args->TryGetWebMessageAsString(&message);
						//
						//	MessageBox(NULL, message.get(), L"", 0);
						//	// processMessage(&message);
						//	//webview->PostWebMessageAsString(message.get());
						//	webview->Navigate(message.get());
						//	return S_OK;
						//}).Get(), &token);

					// Schedule an async task to add initialization script that
					// 1) adds a listener to print message from the host; 2) posts document URL to the host
							
					//  webview->AddScriptToExecuteOnDocumentCreated(
					//	L"window.chrome.webview.addEventListener(\'message\', event => alert(event.data));" \
					//	L"window.chrome.webview.postMessage(window.document.URL);",
					//	nullptr);

					return S_OK;
				}).Get());
			return S_OK;
		}).Get());
}
//------------------------------------------------------------------------
void SendCommandMessage(HWND hWndReceiver, HWND hWndSender, ULONG command, const std::wstring& data)
{
	COPYDATASTRUCT cds;
	cds.dwData = command;
	cds.cbData = DWORD(sizeof(wchar_t) * (data.length() + 1));
	cds.lpData = (void*)data.c_str();
	SendMessage(hWndReceiver, WM_COPYDATA, (WPARAM)hWndSender, (LPARAM)(LPVOID)&cds);
}
//------------------------------------------------------------------------
HWND __stdcall ListLoadW(HWND ParentWin, wchar_t* FileToLoad, int ShowFlags)
{
	HWND hWnd = CreateWindowExA(0, "mdLister", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 
								0, 0, 0, 0, ParentWin, NULL, gs_pluginInstance, NULL);

	auto iniDir = std::filesystem::path(gs_Config.DefaultIniName).parent_path();
	//std::wstring uri = FileToUri(FileToLoad);

	// DON'T OPEN HERE, use a generic navigation function (that can navigate both to md and html)
	if (!SUCCEEDED(CreateWebView2Environment(hWnd, iniDir, FileToLoad)))
		MessageBox(hWnd, std::format(L"Cannot create WebView2. Error code: {}", GetLastError()).c_str(), L"Error", MB_ICONERROR);
	
	//HWND pluginWindow = FindWindowEx(ParentWin, NULL, L"mdLister", NULL);
	//SendCommandMessage(pluginWindow, ParentWin, CMD_NAVIGATE, FileToLoad);

	return hWnd;

	// FOR SOME REASON THIS DOES NOT WORK
	//web::json::value jsonObj = web::json::value::parse(L"{}");
	//jsonObj[L"message"] = web::json::value(MG_CLOSE_WINDOW);
	//jsonObj[L"args"] = web::json::value::parse(L"{}");

	//CheckFailure(PostJsonToWebView(jsonObj, m_controlsWebView.Get()), L"Try again.");


	//HRESULT BrowserWindow::PostJsonToWebView(web::json::value jsonObj, ICoreWebView2 * webview)
	//{
	//	utility::stringstream_t stream;
	//	jsonObj.serialize(stream);

	//	return webview->PostWebMessageAsJson(stream.str().c_str());
	//}


	//wil::com_ptr<ICoreWebView2_3> webview23 = webview.try_query<ICoreWebView2_3>();
	//webview23->SetVirtualHostNameToFolderMapping(); // maybe this approach is better

	//webview->Navigate()
	//webviewController->
	//(ICoreWebView2_3)webview;
	//webview->vir
	//webview->
	//ICoreWebView2_3::SetVirtualHostNameToFolderMapping()
	//SetVirtualHostNameToFolderMapping()

	// CANNOT navigate here (?) maybe too soon or should be inside callback
	// should try to wait until a mutex is released from inside
	//webview->Navigate(L"file:///c:/Projects-Hg/wlx-markdown-viewer/hoedown.html");
			//(L"file:///" + to_wstring(FileToLoad)).c_str());
}
//------------------------------------------------------------------------
HWND __stdcall ListLoadNextW(HWND ParentWin, HWND PluginWin, wchar_t* FileToLoad, int ShowFlags)
{
	// DOES NOT work well if we switch between html and non-html files
	// 
//	std::wstring uri = FileToUri(FileToLoad);
	HWND pluginWindow = FindWindowEx(ParentWin, NULL, L"mdLister", NULL);

	SendCommandMessage(pluginWindow, ParentWin, CMD_NAVIGATE, FileToLoad);
	//COPYDATASTRUCT cds;
	//cds.dwData = CMD_NAVIGATE; // can be anything
	//cds.cbData = DWORD(sizeof(wchar_t) * (uri.length() + 1));
	//cds.lpData = (void*)uri.c_str();
	//SendMessage(pluginWindow, WM_COPYDATA, (WPARAM)ParentWin, (LPARAM)(LPVOID)&cds);

	return LISTPLUGIN_OK;
}
//------------------------------------------------------------------------
void __stdcall ListCloseWindow(HWND ListWin)
{
	PostMessage(ListWin, WM_CLOSE, 0, 0);
}
//------------------------------------------------------------------------
void __stdcall ListSetDefaultParams(ListDefaultParamStruct* dps)
{
	gs_Config = *dps;
}
//------------------------------------------------------------------------
void __stdcall ListGetDetectString(char* DetectString, int maxlen)
{
	// TODO: use ini file
	strcpy_s(DetectString, maxlen, "EXT = \"HTM\" | EXT = \"HTML\" | EXT = \"MARKDOWN\"");

	// FROM AUDIOCONVERTER
	//gPluginIniPath = join_paths(get_dirname(std::string(dps->DefaultIniName)), std::string("audio-converter.ini"));

	//if (GetFileAttributesA(gPluginIniPath.c_str()) == INVALID_FILE_ATTRIBUTES)
	//{
	//	// copy from our archive
	//	std::string archiveIniPath = join_paths(GetModulePath(), std::string("audio-converter.ini"));
	//	CopyFileA(archiveIniPath.c_str(), gPluginIniPath.c_str(), FALSE);
	//}

}
//------------------------------------------------------------------------
//ListPrintW
//ListSearchTextW



