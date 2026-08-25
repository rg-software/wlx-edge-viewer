#include "WebView2Backend.h"
#include "Globals.h"
#include "Log.h"
#include "CharsetOverride.h"

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
void WebView2Backend::NavigateToString(const std::wstring& html,
                                       const std::string& baseUri)
{
	// Retain the base the processor used so a later charset override re-
	// render keeps relative-ref resolution through the <base> splice.
	if (!baseUri.empty())
		m_baseUri = baseUri;
	Log::Line(L"NavigateToString: htmlChars={} base='{}'", html.size(), to_utf16(m_baseUri));

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
void WebView2Backend::SetRawFileBytes(const std::vector<uint8_t>& bytes)
{
	// Cache the pristine source bytes (set by HtmlProcessor before render)
	// so an encoding override can re-decode the ORIGINAL stream, never a
	// previously-spliced one.
	m_rawFileBytes = bytes;
	Log::Line(L"SetRawFileBytes: {} bytes", bytes.size());
}
//------------------------------------------------------------------------
void WebView2Backend::SetEncodingOverrideHtml(bool isHtml)
{
	m_encodingOverrideHtml = isHtml;
	Log::Line(L"SetEncodingOverrideHtml: {}", isHtml ? L"html" : L"loader");
}
//------------------------------------------------------------------------
void WebView2Backend::ApplyCharsetOverride(const std::wstring& tag)
{
	Log::Line(L"ApplyCharsetOverride: tag='{}' html={} rawBytes={}", tag,
	          m_encodingOverrideHtml ? L"yes" : L"no", m_rawFileBytes.size());

	// MHT views re-decode PAGE-SIDE: the mhtml loader owns
	// window.__evEncodingApply. Dispatch the tag/auto signal to it;
	// never splice a charset meta into MIME bytes.
	if (!m_encodingOverrideHtml)
	{
		const std::wstring js = tag.empty()
			? L"window.__evEncodingApply && window.__evEncodingApply(null);"
			: std::format(L"window.__evEncodingApply && window.__evEncodingApply('{}');", tag);
		ExecuteScript(js);
		return;
	}

	// HTML: rebuild from the pristine cache. Embedded-string loads
	// (NavigateToString) always re-encode to UTF-8, so a spliced
	// <meta charset> can never force the engine's decode; instead we
	// transcode the bytes host-side with the chosen code page and render
	// the resulting Unicode text (plus the retained <base href>).
	// Empty tag = Auto-detect = re-render pristine bytes as Latin-1
	// (fresh engine sniffing over the original mojibake, no override).
	if (m_rawFileBytes.empty())
	{
		Log::Line(L"ApplyCharsetOverride: no cached HTML bytes to transcode");
		return;
	}

	std::wstring decoded;
	if (!tag.empty() && CharsetOverride::TranscodeBytes(tag, m_rawFileBytes, decoded))
	{
		std::wstring doc = L"<base href=\"" + to_utf16(m_baseUri) + L"\">\n";
		doc += decoded;
		Log::Line(L"ApplyCharsetOverride: transcoded {} bytes via '{}'",
		          m_rawFileBytes.size(), tag);
		NavigateToString(doc, m_baseUri);
		return;
	}

	// Unknown/undecodable label or Auto-detect: fall back to the plain
	// Latin-1 render of the pristine bytes (same as the initial sniffed
	// view — never blank).
	const std::wstring base = to_utf16(m_baseUri);
	const auto spliced = CharsetOverride::SpliceCharsetAndBase(m_rawFileBytes, tag, base);
	Log::Line(L"ApplyCharsetOverride: fallback render, {} -> {} bytes, base '{}'",
	          m_rawFileBytes.size(), spliced.size(), to_utf16(m_baseUri));
	NavigateToString(CharsetOverride::BytesToLatin1(spliced), m_baseUri);
}
//------------------------------------------------------------------------
