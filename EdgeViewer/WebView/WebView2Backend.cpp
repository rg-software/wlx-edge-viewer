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
	// Every fresh document starts back in "auto-detect" (engine sniffing).
	m_activeEncodingTag.clear();
	m_userPicked = false;
	m_autoApplied = false;
	m_autoSuggestedTag.clear();
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
	m_activeEncodingTag.clear();
	m_userPicked = false;
	m_autoApplied = false;
	m_autoSuggestedTag.clear();
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

	// Expose the pristine bytes to the page (charset-autodetect glue reads
	// window.__evRawFileBytesB64 to run the statistical detector). Mirror the
	// Linux mechanism. Inlined as a base64 string literal (safe charset).
	if (!bytes.empty())
	{
		static constexpr char kAlphabet[] =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		std::string b64;
		b64.reserve(((bytes.size() + 2) / 3) * 4);
		for (size_t i = 0; i < bytes.size(); i += 3)
		{
			const uint32_t b0 = bytes[i];
			const uint32_t b1 = (i + 1 < bytes.size()) ? bytes[i + 1] : 0;
			const uint32_t b2 = (i + 2 < bytes.size()) ? bytes[i + 2] : 0;
			const uint32_t t = (b0 << 16) | (b1 << 8) | b2;
			b64 += kAlphabet[(t >> 18) & 0x3F];
			b64 += kAlphabet[(t >> 12) & 0x3F];
			b64 += (i + 1 < bytes.size()) ? kAlphabet[(t >> 6) & 0x3F] : '=';
			b64 += (i + 2 < bytes.size()) ? kAlphabet[t & 0x3F] : '=';
		}
		const std::wstring js = L"window.__evRawFileBytesB64 = '" +
			std::wstring(b64.begin(), b64.end()) + L"';";
		mWebView->AddScriptToExecuteOnDocumentCreated(js.c_str(), nullptr);
	}
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
	// A pick via the Encoding menu is a USER choice: auto-detection must
	// not re-fire for this view, and the "Auto-detect (X)" hint clears.
	m_userPicked = true;
	m_autoApplied = false;
	m_autoSuggestedTag.clear();
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
		m_activeEncodingTag = tag; // checked entry in the menu
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
		m_activeEncodingTag = tag; // NavigateToString reset it; re-assert
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
	m_activeEncodingTag = tag; // NavigateToString reset it; re-assert
}
//------------------------------------------------------------------------
std::wstring WebView2Backend::GetActiveEncodingTag() const
{
	return m_activeEncodingTag;
}
//------------------------------------------------------------------------
void WebView2Backend::ApplyAutoDetectedEncoding(const std::wstring& tag)
{
	// Provisional auto re-decode. Never overrides a user's manual pick,
	// and only fires once per view (the page guards with __evAutoDetectDone).
	if (m_userPicked || tag.empty() || m_rawFileBytes.empty() || !m_encodingOverrideHtml)
		return;

	// ApplyCharsetOverride marks user-picked; the auto path must NOT do that.
	// Remember the pre-call state and restore it, but keep the applied tag /
	// suggestion so the menu can show "Auto-detect (<tag>)".
	bool hadUserPicked = m_userPicked;
	ApplyCharsetOverride(tag);
	m_userPicked = hadUserPicked;   // still auto, not a user choice
	// The applied override is still conceptually "Auto-detect"; keep the
	// Auto-detect entry checked and surface the suggestion on it instead.
	m_activeEncodingTag.clear();
	m_autoApplied = true;
	m_autoSuggestedTag = tag;
	Log::Line(L"ApplyAutoDetectedEncoding: applied provisional '{}'", tag);
}
//------------------------------------------------------------------------
std::wstring WebView2Backend::GetAutoSuggestedTag() const
{
	return m_autoApplied && !m_userPicked ? m_autoSuggestedTag : L"";
}
//------------------------------------------------------------------------
