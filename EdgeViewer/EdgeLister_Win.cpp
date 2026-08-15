#include "EdgeLister.h"
#include "Globals.h"
#include "Navigator.h"
#include "IWebView.h"
#include "Log.h"
#include "WebView/WebView2Backend.h"

#include <ShlObj.h>
#include <atlbase.h>
#include <windows.h>
#include <string>
#include <shlwapi.h>
#include <webview2.h>

//------------------------------------------------------------------------
// Free helpers used by the WndProc: pluginWndProc handles WM_COPYDATA etc.
// inside the EdgeLister window; showPopupMenu is the shell right-click
// menu (Windows-only feature).
static LRESULT CALLBACK pluginWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static void showPopupMenu(HWND hWnd, const std::wstring& filename);

void EdgeLister::RegisterClass(HINSTANCE hinst)
{
	static bool registered = false;
	if (registered)
		return;
	registered = true;

	WNDCLASSA wc = {};
	wc.hInstance = hinst;
	wc.lpfnWndProc = pluginWndProc;
	wc.lpszClassName = EDGE_LISTER_CLASS;
	RegisterClassA(&wc);
}
//------------------------------------------------------------------------
LRESULT CALLBACK pluginWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (const auto& it = gs_Views.find(hWnd); it != std::end(gs_Views))
	{
		IWebView* webView = it->second.get();

		switch (message)
		{
		case WM_SIZE:	// resize webview according to EdgeLister size
			{
				RECT bounds;
				GetClientRect(hWnd, &bounds);
				if (auto* wv2 = dynamic_cast<WebView2Backend*>(webView))
				{
					wil::com_ptr<ICoreWebView2Controller> ctrl = wv2->GetController();
					if (ctrl) ctrl->put_Bounds(bounds);
				}
			}
			break;

		case WM_COPYDATA:
			{
				auto pcds = (COPYDATASTRUCT*)lParam;
				auto strData = std::wstring((wchar_t*)pcds->lpData);

				if (pcds->dwData == CMD_NAVIGATE)
					Navigator(*webView).Open(strData);

				if (pcds->dwData == CMD_PRINT)
					Navigator(*webView).Print();

                if (pcds->dwData == CMD_MENU)
                    showPopupMenu(hWnd, strData);

				if (pcds->dwData == CMD_SEARCH)
				{
					size_t i = strData.find_first_of(L' ');
					int params = std::stoi(strData.substr(0, i));
					std::wstring pattern = strData.substr(i + 1);
					Navigator(*webView).Search(pattern, params);
				}
			}
			break;

		case WM_SETFOCUS:
			{
				if (auto* wv2 = dynamic_cast<WebView2Backend*>(webView))
				{
					wil::com_ptr<ICoreWebView2Controller> ctrl = wv2->GetController();
					if (ctrl) ctrl->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
				}
			}
			break;

		case WM_WEBVIEW_JS_KEYDOWN:
			{
				if ((wParam >= '1' && wParam <= '8') || wParam == 'Q')
					PostMessage(GetParent(hWnd), WM_KEYDOWN, wParam, NULL);
				break;
			}
		case WM_WEBVIEW_KEYDOWN:
			{
				PostMessage(GetParent(hWnd), WM_KEYDOWN, wParam, NULL);
			}
			break;
		}
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
//------------------------------------------------------------------------
void showPopupMenu(HWND hWnd, const std::wstring& filename)
{
	POINT point;
	GetCursorPos(&point);

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		CComPtr<IShellFolder> pDesktopFolder;
		hr = SHGetDesktopFolder(&pDesktopFolder);
		if (SUCCEEDED(hr))
		{
			std::wstring parentPath = filename.substr(0, filename.find_last_of(L'\\'));
			LPITEMIDLIST pidlParent = NULL;

			hr = pDesktopFolder->ParseDisplayName(hWnd, NULL, (LPWSTR)parentPath.c_str(), NULL, &pidlParent, NULL);
			if (SUCCEEDED(hr))
			{
				CComPtr<IShellFolder> pParentFolder;
				hr = pDesktopFolder->BindToObject(pidlParent, NULL, IID_IShellFolder, (void**)&pParentFolder);
				if (SUCCEEDED(hr))
				{
					LPITEMIDLIST pidlFile = NULL;
					std::wstring fileNameOnly = filename.substr(filename.find_last_of(L'\\') + 1);
					hr = pParentFolder->ParseDisplayName(hWnd, NULL, (LPWSTR)fileNameOnly.c_str(), NULL, &pidlFile, NULL);
					if (SUCCEEDED(hr))
					{
						LPCITEMIDLIST aPidls[] = { pidlFile };
						CComPtr<IContextMenu> pContextMenu;
						hr = pParentFolder->GetUIObjectOf(hWnd, 1, aPidls, IID_IContextMenu, NULL, (void**)&pContextMenu);
						if (SUCCEEDED(hr))
						{
							HMENU hMenu = CreatePopupMenu();
							if (hMenu)
							{
								hr = pContextMenu->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL);
								if (SUCCEEDED(hr))
								{
									UINT uCmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, point.x, point.y, hWnd, NULL);

									if (uCmd != 0)
									{
										CMINVOKECOMMANDINFOEX ici = { sizeof(CMINVOKECOMMANDINFOEX) };
										ici.cbSize = sizeof(CMINVOKECOMMANDINFOEX);
										ici.fMask = CMIC_MASK_PTINVOKE;
										ici.hwnd = hWnd;
										ici.ptInvoke = point;
										ici.lpVerb = MAKEINTRESOURCEA(uCmd - 1);
										ici.nShow = SW_SHOWNORMAL;

										pContextMenu->InvokeCommand((LPCMINVOKECOMMANDINFO)&ici);
									}
								}
								DestroyMenu(hMenu);
							}
						}
						CoTaskMemFree(pidlFile);
					}
				}
				CoTaskMemFree(pidlParent);
			}
		}
		CoUninitialize();
	}
}
//------------------------------------------------------------------------
