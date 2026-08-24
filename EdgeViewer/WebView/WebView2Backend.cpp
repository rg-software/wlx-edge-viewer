#include "WebView2Backend.h"
#include "Globals.h"

#include <wrl.h>
#include <shlwapi.h>
#include <fstream>

//------------------------------------------------------------------------
WebView2Backend::WebView2Backend(wil::com_ptr<ICoreWebView2Controller> controller,
                                 wil::com_ptr<ICoreWebView2> webview)
	: mController(std::move(controller)), mWebView(std::move(webview))
{
}
//------------------------------------------------------------------------
// WebView2's ICoreWebView2::NavigateToString caps the htmlContent string
// at 2 MB (landing on about:blank beyond it). The pre-fetch processors
// inline the file as base64 (~4/3 the raw size), so documents past
// ~1.5 MB exceed the cap. Qt Web Engine's setHtml has no such 2 MB
// string limit, so this workaround is Windows-only: for oversized HTML,
// write it to a temp file, map a virtual host to that folder, and Navigate
// to it. The per-view mapping is independent of any other lister window.
void WebView2Backend::NavigateToString(const std::wstring& html)
{
	// 2 MB is the string-size cap. Measure in wchar (2 bytes each on
	// Windows); leave headroom so the 2 MB byte limit is not approached.
	constexpr std::size_t kMaxInlineWchar = 1'000'000;
	if (html.size() > kMaxInlineWchar)
	{
		wchar_t tempDir[MAX_PATH];
		wchar_t tempFile[MAX_PATH];
		if (GetTempPathW(MAX_PATH, tempDir) && GetTempFileNameW(tempDir, L"evw", 0, tempFile))
		{
			std::wstring tmpPath = tempFile;
			tmpPath += L".html";
			MoveFileW(tempFile, tmpPath.c_str());

			std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
			if (out)
			{
				std::string utf8html = to_utf8(html);
				out.write(utf8html.data(), static_cast<std::streamsize>(utf8html.size()));
				out.close();
				if (out)
				{
					// Host name is per-backend (each lister window owns its
					// own ICoreWebView2), so reuse a fixed synthetic name.
					auto folder = fs::path(tmpPath).parent_path();
					auto webview23 = mWebView.try_query<ICoreWebView2_3>();
					webview23->SetVirtualHostNameToFolderMapping(
						L"lister.example", folder.c_str(),
						COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);

					std::wstring uri = L"http://lister.example/" + fs::path(tmpPath).filename().wstring();
					mWebView->Navigate(uri.c_str());

					gs_tempFiles.push_back(tmpPath);
					return;
				}
			}
		}
		// Fall through to NavigateToString if the temp-file path failed.
	}

	mWebView->NavigateToString(html.c_str());
}
//------------------------------------------------------------------------
void WebView2Backend::Navigate(const std::wstring& uri)
{
	mWebView->Navigate(uri.c_str());
}
//------------------------------------------------------------------------
void WebView2Backend::ExecuteScript(const std::wstring& js)
{
	mWebView->ExecuteScript(js.c_str(), nullptr);
}
//------------------------------------------------------------------------
void WebView2Backend::AddScriptToExecuteOnDocumentCreated(const std::wstring& js)
{
	mWebView->AddScriptToExecuteOnDocumentCreated(js.c_str(), nullptr);
}
//------------------------------------------------------------------------
void WebView2Backend::RegisterVirtualHost(const std::wstring& host, const std::filesystem::path& folder)
{
	auto webview23 = mWebView.try_query<ICoreWebView2_3>();
	webview23->SetVirtualHostNameToFolderMapping(host.c_str(), folder.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
}
//------------------------------------------------------------------------
void WebView2Backend::Close()
{
	if (mController)
		mController->Close();
}
//------------------------------------------------------------------------
