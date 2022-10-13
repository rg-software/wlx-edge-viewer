#include "EdgeLister.h"
#include "Navigator.h"

//------------------------------------------------------------------------
void EdgeLister::RegisterClass(HINSTANCE hinst)
{
	WNDCLASSA wc = {};
	wc.hInstance = hinst;
	wc.lpfnWndProc = pluginWndProc;
	wc.lpszClassName = EDGE_LISTER_CLASS;
	RegisterClassA(&wc);
}
//------------------------------------------------------------------------
LRESULT EdgeLister::pluginWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (const auto& it = gs_Views.find(hWnd); it != std::end(gs_Views))
	{
		ViewPtr webview;
		ViewCtrlPtr controller = it->second;

		switch (message)
		{
		case WM_SIZE:	// resize webview according to EdgeLister size
			{
				RECT bounds;
				GetClientRect(hWnd, &bounds);
				controller->put_Bounds(bounds);
			}
			break;

		case WM_COPYDATA:	// generic "data received" event
			{
				controller->get_CoreWebView2(&webview);
				COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;
				auto strData = std::wstring((wchar_t*)pcds->lpData);

				// command: navigate to the specified resource
				if (pcds->dwData == CMD_NAVIGATE)
					Navigator(webview).Open(strData);

				// print the current file
				if (pcds->dwData == CMD_PRINT)
					Navigator(webview).Print();

				// search text in the browser window
				if (pcds->dwData == CMD_SEARCH)
				{
					size_t i = strData.find_first_of(L' ');
					int params = std::stoi(strData.substr(0, i));
					std::wstring pattern = strData.substr(i + 1);
					Navigator(webview).Search(pattern, params);
				}
			}
			break;

		case WM_SETFOCUS:	// set the real focus on the webview
			{
				controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
			}
			break;

		case WM_WEBVIEW_KEYDOWN:	// resend webview keypess events to the parent
			{
				PostMessage(GetParent(hWnd), WM_KEYDOWN, wParam, NULL);
			}
			break;
		}
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
//------------------------------------------------------------------------
