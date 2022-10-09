// TODO: window resize events
// implement missing functions
// hotkeys -- pass to the parent
// make sure the right child is active
// markdown, etc.

#include "listerplugin.h"

#include <windows.h>
#include <stdlib.h>
#include <string>
#include <format>
#include <tchar.h>
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include <pathcch.h>

using namespace Microsoft::WRL;

//------------------------------------------------------------------------
HINSTANCE gs_pluginInstance;
ListDefaultParamStruct gs_Config;
wil::com_ptr<ICoreWebView2Controller> webviewController;
wil::com_ptr<ICoreWebView2> webview;
//------------------------------------------------------------------------
// TODO: FIX THIS! CYR FILENAMES GET GARBLED
inline std::wstring to_wstring(const std::string& s)
{
	auto size = MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);	// should be ASCII anyway
	std::wstring result(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &result[0], size);

	return result;
}
//------------------------------------------------------------------------
//template<typename CharT> std::basic_string<CharT> get_dirname(const std::basic_string<CharT>& path)
//{
//	return path.substr(0, path.find_last_of(static_cast<CharT>('\\')));	// return without trailing backslash
//}
//------------------------------------------------------------------------
LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hWnd, message, wParam, lParam);

	/////////////////////////
	TCHAR greeting[] = _T("Hello, Windows desktop!");

	switch (message)
	{
	case WM_SIZE:
		if (webviewController != nullptr) {
			RECT bounds;
			GetClientRect(hWnd, &bounds);
			webviewController->put_Bounds(bounds);
		};
		break;
	case WM_DESTROY:
		//webviewController->Close();
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
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
HRESULT CreateWebView2Environment(HWND hWnd, PCWSTR userDir, const std::wstring& uri)
{
	return CreateCoreWebView2EnvironmentWithOptions(nullptr, userDir, nullptr,
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>([=](HRESULT result, ICoreWebView2Environment* env) /*-> HRESULT */
		{
			env->CreateCoreWebView2Controller(hWnd,
				Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>([=](HRESULT result, ICoreWebView2Controller* controller) /*-> HRESULT*/
			{
				if (controller != nullptr) 
				{
					webviewController = controller;
					webviewController->get_CoreWebView2(&webview);
				}

				// Here we can change settings; check:
				// https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2settings
				wil::com_ptr<ICoreWebView2Settings> settings;
				webview->get_Settings(&settings);
				settings->put_IsScriptEnabled(TRUE);
				settings->put_AreDefaultScriptDialogsEnabled(TRUE);
				settings->put_IsWebMessageEnabled(TRUE);

				// Resize WebView to fit the bounds of the parent window
				RECT bounds;
				GetClientRect(hWnd, &bounds);
				webviewController->put_Bounds(bounds);

				// Navigation events
				// register an ICoreWebView2NavigationStartingEventHandler to cancel any non-https navigation
				webview->Navigate(uri.c_str());

				//EventRegistrationToken token;
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
							
				// webview->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
				// [](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
				//		wil::unique_cotaskmem_string message;
				//		args->TryGetWebMessageAsString(&message);
				//		// processMessage(&message);
				//		webview->PostWebMessageAsString(message.get());
				//		return S_OK;
				// }).Get(), &token);

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
HWND __stdcall ListLoadW(HWND ParentWin, wchar_t* FileToLoad, int ShowFlags)
{
	HWND hWnd = CreateWindowExA(0, "mdLister", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 
								0, 0, 0, 0, ParentWin, NULL, gs_pluginInstance, NULL);

	wchar_t iniDir[MAX_PATH];
	wcscpy_s(iniDir, to_wstring(gs_Config.DefaultIniName).c_str());	// hope it is plain ascii!
	PathCchRemoveFileSpec(iniDir, MAX_PATH);

	std::wstring uri = L"file:///" + std::wstring(FileToLoad);

	if (!SUCCEEDED(CreateWebView2Environment(hWnd, iniDir, uri)))
		MessageBox(hWnd, std::format(L"Cannot create WebView2. Error code: {}", GetLastError()).c_str(), L"Error", MB_ICONERROR);
	
	//webview->Navigate(L"https://www.bing.com/");

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
	return hWnd;
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
	strcpy_s(DetectString, maxlen, "EXT = \"HTM\" | EXT = \"HTML\"");
}
//------------------------------------------------------------------------



