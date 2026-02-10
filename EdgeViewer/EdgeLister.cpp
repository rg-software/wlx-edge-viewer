#include "EdgeLister.h"
#include "Navigator.h"

#include <ShlObj.h>
#include <atlbase.h>
#include <windows.h>
#include <string>
#include <Shlwapi.h>

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
				auto pcds = (COPYDATASTRUCT*)lParam;
				auto strData = std::wstring((wchar_t*)pcds->lpData);

				// command: navigate to the specified resource
				if (pcds->dwData == CMD_NAVIGATE)
					Navigator(webview).Open(strData);

				// print the current file
				if (pcds->dwData == CMD_PRINT)
					Navigator(webview).Print();

                // right-click (sent by DirProcessor)
                if (pcds->dwData == CMD_MENU)
                    showPopupMenu(hWnd, strData);

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

		case WM_WEBVIEW_JS_KEYDOWN:
			{
				// pass hotkeys 1-8
				if (wParam >= '1' && wParam <= '8')
					PostMessage(GetParent(hWnd), WM_KEYDOWN, wParam, NULL);
				break;
			}
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
void EdgeLister::showPopupMenu(HWND hWnd, const std::wstring& filename)
{
	POINT point;
	GetCursorPos(&point);
	
	// Initialize COM
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr))
	{
		// Get the desktop shell folder
		CComPtr<IShellFolder> pDesktopFolder;
		hr = SHGetDesktopFolder(&pDesktopFolder);
		if (SUCCEEDED(hr))
		{
			// Get PIDL for the parent directory
			std::wstring parentPath = filename.substr(0, filename.find_last_of(L'\\'));
			LPITEMIDLIST pidlParent = NULL;

			hr = pDesktopFolder->ParseDisplayName(hWnd, NULL, (LPWSTR)parentPath.c_str(), NULL, &pidlParent, NULL);
			if (SUCCEEDED(hr))
			{
				CComPtr<IShellFolder> pParentFolder;
				hr = pDesktopFolder->BindToObject(pidlParent, NULL, IID_IShellFolder, (void**)&pParentFolder);
				if (SUCCEEDED(hr))
				{
					// Get relative PIDL for the file within the parent directory
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