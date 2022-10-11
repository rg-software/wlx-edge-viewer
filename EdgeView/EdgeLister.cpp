#include "EdgeLister.h"
#include "Navigator.h"

extern ViewsMap gs_Views;

//------------------------------------------------------------------------
void EdgeLister::RegisterClass(HINSTANCE hinst)
{
	WNDCLASSA wc = {};
	wc.hInstance = hinst;
	wc.lpfnWndProc = pluginWndProc;
	wc.lpszClassName = "mdLister";
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
		case WM_SIZE:
			{
				RECT bounds;
				GetClientRect(hWnd, &bounds);
				controller->put_Bounds(bounds);
			}
			break;

		case WM_COPYDATA:
			{
				controller->get_CoreWebView2(&webview);
				COPYDATASTRUCT* pcds = (COPYDATASTRUCT*)lParam;
				auto strData = std::wstring((wchar_t*)pcds->lpData);

				if (pcds->dwData == CMD_NAVIGATE)
					Navigator(webview).Open(strData);
			}
			break;

		case WM_SETFOCUS:
			{
				controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
			}
			break;
		}
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
//------------------------------------------------------------------------
