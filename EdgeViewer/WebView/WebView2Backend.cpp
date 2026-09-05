#include "WebView2Backend.h"
#include "Globals.h"
#include "CharsetOverride.h"

#include <wrl.h>
#include <shlwapi.h>
#include <format>
#include <fstream>
#include <vector>
#include <cctype>
#include <cwctype>
#include <sstream>

//------------------------------------------------------------------------
namespace
{
// Content type for a ForcedHtmlExt file (and its relative subresources) served
// through the evh:// scheme. Forced documents are .xml/.xhtml and must render
// as HTML, so those map to text/html; everything else uses a small map so
// images/css/js still display. Unknown types fall back to a neutral type.
std::wstring MimeForPath(const std::filesystem::path& p)
{
	std::wstring ext = p.extension().wstring();
	for (auto& c : ext) c = static_cast<wchar_t>(std::towlower(c));
	if (ext == L".xml" || ext == L".xhtml" || ext == L".html" || ext == L".htm")
		return L"text/html";
	if (ext == L".css") return L"text/css";
	if (ext == L".js" || ext == L".mjs") return L"text/javascript";
	if (ext == L".json") return L"application/json";
	if (ext == L".png") return L"image/png";
	if (ext == L".jpg" || ext == L".jpeg") return L"image/jpeg";
	if (ext == L".gif") return L"image/gif";
	if (ext == L".svg") return L"image/svg+xml";
	if (ext == L".webp") return L"image/webp";
	if (ext == L".ico") return L"image/x-icon";
	if (ext == L".bmp") return L"image/bmp";
	if (ext == L".woff") return L"font/woff";
	if (ext == L".woff2") return L"font/woff2";
	if (ext == L".ttf") return L"font/ttf";
	if (ext == L".otf") return L"font/otf";
	if (ext == L".txt") return L"text/plain";
	if (ext == L".pdf") return L"application/pdf";
	return L"application/octet-stream";
}

// Percent-decode the path portion of an evh:// URI (urlPath masks '#' as %23,
// and spaces may appear as %20). Returns false on any malformed escape.
bool UrlDecodeInto(std::wstring& out, const std::wstring& in)
{
	out.clear();
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); ++i)
	{
		const wchar_t c = in[i];
		if (c == L'%')
		{
			if (i + 2 >= in.size()) return false;
			const auto hexVal = [](wchar_t h) -> int {
				if (h >= L'0' && h <= L'9') return h - L'0';
				if (h >= L'a' && h <= L'f') return h - L'a' + 10;
				if (h >= L'A' && h <= L'F') return h - L'A' + 10;
				return -1;
			};
			const int hi = hexVal(in[i + 1]);
			const int lo = hexVal(in[i + 2]);
			if (hi < 0 || lo < 0) return false;
			out.push_back(static_cast<wchar_t>((hi << 4) | lo));
			i += 2;
		}
		else
		{
			out.push_back(c);
		}
	}
	return true;
}
}
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
	// Remember the real file URL so a later "Auto-detect" menu pick can
	// re-navigate to it (fresh engine sniff) instead of an embedded
	// Latin-1 byte-map re-render that corrupts non-ASCII bytes.
	m_lastNavigateUri = uri;
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

	// Remember the mapping so the evh:// ForcedHtmlExt handler can translate
	// evh://<host>/<rel> back to <folder>/<rel> and serve it as text/html.
	{
		std::lock_guard lock(m_hostFoldersMutex);
		m_hostFolders[host] = folder;
	}
	InstallForcedHtmlSchemeHandler();
}
//------------------------------------------------------------------------
void WebView2Backend::InstallForcedHtmlSchemeHandler()
{
	if (m_forcedHtmlHandlerInstalled)
		return;
	m_forcedHtmlHandlerInstalled = true;

	// Serve evh://<host>/<rel> by translating back to <folder>/<rel>. Forced
	// documents (.xml/.xhtml) are served as text/html so they render as HTML;
	// relative subresources resolve through the same mapping. The callback
	// runs on an arbitrary thread, so the host map is mutex-guarded; the
	// backend itself outlives the webview (the owning view holds its IWebView
	// shared_ptr for the webview's whole lifetime), so capturing `this` is safe.
	mWebView->AddWebResourceRequestedFilter(L"evh://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
	mWebView->add_WebResourceRequested(
		Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
			[this](ICoreWebView2* sender, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
				wil::com_ptr<ICoreWebView2WebResourceRequest> request;
				wil::unique_cotaskmem_string uri;
				args->get_Request(&request);
				request->get_Uri(&uri);

				wil::com_ptr<ICoreWebView2_2> webview2;
				sender->QueryInterface(IID_PPV_ARGS(&webview2));
				wil::com_ptr<ICoreWebView2Environment> environment;
				webview2->get_Environment(&environment);

				wil::com_ptr<ICoreWebView2WebResourceResponse> response;
				if (uri != nullptr && BuildForcedHtmlResponse(uri.get(), environment.get(), response.put()))
					args->put_Response(response.get());
				else
				{
					environment->CreateWebResourceResponse(nullptr, 404, L"Not Found", L"", &response);
					args->put_Response(response.get());
				}
				return S_OK;
			})
			.Get(),
		&m_forcedHtmlToken);
}

// Map an evh://<host>/<escapedRel> URI to <folder>/<rel>, read the file, and
// build a WebResourceResponse whose Content-Type is text/html for .xml/.xhtml
// (so forced documents render as HTML). Returns false if the host is unmapped
// or the file is unreadable.
bool WebView2Backend::BuildForcedHtmlResponse(
	const wchar_t* uri,
	ICoreWebView2Environment* environment,
	ICoreWebView2WebResourceResponse** outResponse)
{
	*outResponse = nullptr;

	// Strip the scheme+authority: "evh://<host>/<path>".
	const std::wstring full = uri;
	const size_t p = full.find(L"//");
	if (p == std::wstring::npos)
		return false;
	const std::wstring rest = full.substr(p + 2);
	const size_t slash = rest.find(L'/');
	std::wstring host = rest;
	std::wstring escapedPath;
	if (slash != std::wstring::npos)
	{
		host = rest.substr(0, slash);
		escapedPath = rest.substr(slash + 1);
	}

	std::filesystem::path folder;
	{
		std::lock_guard lock(m_hostFoldersMutex);
		auto it = m_hostFolders.find(host);
		if (it == m_hostFolders.end())
			return false;
		folder = it->second;
	}

	std::wstring rel;
	if (!UrlDecodeInto(rel, escapedPath))
		return false;

	// Basic containment: the decoded relative path must not escape the folder.
	const std::wstring foldLower = [&] { std::wstring s = folder.wstring(); for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c)); return s; }();
	const std::filesystem::path filePath = folder / rel;
	const std::wstring fileLower = [&] { std::wstring s = filePath.wstring(); for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c)); return s; }();
	if (foldLower.empty() || fileLower.rfind(foldLower, 0) != 0)
		return false;

	// Read the file into memory and wrap it in a stream for the response.
	std::ifstream f(filePath, std::ios::binary);
	if (!f)
		return false;
	std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

	wil::com_ptr<IStream> stream;
	CreateStreamOnHGlobal(nullptr, TRUE, &stream);
	{
		ULONG written = 0;
		if (!bytes.empty())
			stream->Write(bytes.data(), static_cast<ULONG>(bytes.size()), &written);
		LARGE_INTEGER zero{};
		stream->Seek(zero, STREAM_SEEK_SET, nullptr);
	}

	const std::wstring headers = L"Content-Type: " + MimeForPath(filePath) + L"\r\n";
	environment->CreateWebResourceResponse(stream.get(), 200, L"OK", headers.c_str(), outResponse);
	return true;
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
	// A fresh file load resets the auto-detection latch so a newly opened
	// HTML file gets one auto-detection pass again.
	m_autoAlreadyApplied = false;

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
void WebView2Backend::SetEncodingOverrideSupported(bool supported)
{
	m_encodingOverrideSupported = supported;
}
//------------------------------------------------------------------------
void WebView2Backend::SetEncodingOverrideHtml(bool isHtml)
{
	m_encodingOverrideHtml = isHtml;
	// A loader view (MHT) is a fresh logical load that owns its own page-side
	// detection (loader.html posts CMD_AUTO_ENCODING[_REPORT]); clear the host
	// re-post latch so a stale HTML auto-apply on a reused backend does not
	// block MHT's single auto pass. HTML views rely on SetRawFileBytes (which
	// runs before navigation) for the same reset — deliberately NOT reset here
	// for HTML, because the HTML auto re-render would otherwise clear the latch
	// and re-trigger.
	if (!isHtml)
		m_autoAlreadyApplied = false;
}
//------------------------------------------------------------------------
void WebView2Backend::SetHtmlBaseHref(const std::string& baseHref)
{
	// Retain the HTML file's <base href> so an encoding override re-decode
	// can rebuild relative-ref resolution on its embedded re-render.
	m_baseUri = baseHref;
}
//------------------------------------------------------------------------
void WebView2Backend::SetCurrentFileDirectory(const std::filesystem::path& path)
{
	// Retain the opened file's directory so the EML attachment save flow
	// can default the folder picker to the message's folder.
	m_currentFileDir = path;
}
//------------------------------------------------------------------------
std::filesystem::path WebView2Backend::GetCurrentFileDirectory() const
{
	return m_currentFileDir;
}
//------------------------------------------------------------------------
void WebView2Backend::ApplyCharsetOverride(const std::wstring& tag)
{
	// A pick via the Encoding menu is a USER choice: auto-detection must
	// not re-fire for this view, and the "Auto-detect (X)" hint clears.
	// EXCEPTION: empty tag = the user explicitly chose "Auto-detect" to
	// re-enable auto-detection, so reset the latches and let it re-fire
	// (mirrors the Linux QtWebEngineBackend::ApplyCharsetOverride).
	if (tag.empty())
	{
		m_userPicked = false;
		m_autoApplied = false;
		m_autoAlreadyApplied = false;
		m_autoSuggestedTag.clear();
	}
	else
	{
		m_userPicked = true;
		m_autoApplied = false;
		m_autoSuggestedTag.clear();
	}

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
		return;
	}

	// Empty tag = the user reselected "Auto-detect". For an HTML file that
	// was loaded via a real Navigate(), go back to that URL so the engine
	// re-sniffs the pristine bytes — never the embedded Latin-1 byte-map
	// fallback below, which corrupts non-ASCII (byte->codepoint -> UTF-8).
	if (tag.empty() && !m_lastNavigateUri.empty())
	{
		Navigate(m_lastNavigateUri);
		return;
	}

	std::wstring decoded;
	if (!tag.empty() && CharsetOverride::TranscodeBytes(tag, m_rawFileBytes, decoded))
	{
		std::wstring doc = L"<base href=\"" + to_utf16(m_baseUri) + L"\">\n";
		doc += decoded;
		NavigateToString(doc, m_baseUri);
		m_activeEncodingTag = tag; // NavigateToString reset it; re-assert
		return;
	}

	// Unknown/undecodable label: the chosen code page could not be applied.
	// Fall back to a fresh engine sniff of the pristine bytes by
	// re-navigating to the real file URL (never the byte->codepoint
	// Latin-1 map, which corrupts non-ASCII). Every HTML view records a
	// URL via Navigate(); if none exists there is nothing to sniff, so
	// render blank rather than corrupt the text.
	if (!m_lastNavigateUri.empty())
	{
		// This pick could not be applied, so hand the file back to
		// auto-detection: re-arm the charset latches before the re-navigate.
		// Navigate() clears the user-pick state, but the one-shot auto latch
		// (m_autoAlreadyApplied) survives it and would swallow the re-navigated
		// document's CMD_AUTO_ENCODING_REPORT — leaving a bare "Auto-detect"
		// instead of restoring the "Auto: <tag>" hint. Mirror the empty-tag
		// (explicit Auto-detect) branch above.
		m_userPicked = false;
		m_autoAlreadyApplied = false;
		Navigate(m_lastNavigateUri);
		return;
	}
	NavigateToString(L"", m_baseUri);
	m_activeEncodingTag.clear(); // nothing applied; return to auto-detect
}
//------------------------------------------------------------------------
std::wstring WebView2Backend::GetActiveEncodingTag() const
{
	return m_activeEncodingTag;
}
//------------------------------------------------------------------------
void WebView2Backend::ApplyAutoDetectedEncoding(const std::wstring& tag)
{
	// Provisional auto re-decode. Never overrides a user's manual pick, and
	// only ever fires ONCE per logical file load: the auto re-render creates
	// a fresh document whose window.__evAutoDetectDone resets, which would
	// re-post; this latch survives that (only SetRawFileBytes / a new load
	// clears it).
	// Gate on encodingOverrideSupported (HTML AND MHT): MHT re-decodes
	// PAGE-SIDE via ApplyCharsetOverride's loader dispatch, so it needs no
	// cached raw bytes host-side; other loader processors (Markdown, RST,
	// ...) never post auto-detection and stay guarded here.
	if (m_userPicked || m_autoAlreadyApplied || tag.empty() || !m_encodingOverrideSupported)
		return;
	m_autoAlreadyApplied = true;

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
}
//------------------------------------------------------------------------
std::wstring WebView2Backend::GetAutoSuggestedTag() const
{
	return m_autoApplied && !m_userPicked ? m_autoSuggestedTag : L"";
}
//------------------------------------------------------------------------
void WebView2Backend::ReportAutoDetectedEncoding(const std::wstring& tag)
{
	// Display-only auto report (CMD_AUTO_ENCODING_REPORT): the engine already
	// decoded the file with the detected code page (detector agreed, or a
	// genuine declared charset), so no re-decode is needed. We only record the
	// suggestion so the Encoding submenu can show "Auto: <codepage>" instead of
	// a bare "Auto-detect". Same latches as ApplyAutoDetectedEncoding: a user
	// pick wins, and the report only runs against the triggering logical load.
	if (m_userPicked || m_autoAlreadyApplied || tag.empty() || !m_encodingOverrideSupported)
		return;
	m_autoAlreadyApplied = true;
	m_activeEncodingTag.clear();    // Auto-detect stays the checked entry
	m_autoApplied = true;
	m_autoSuggestedTag = tag;
}
//------------------------------------------------------------------------
void WebView2Backend::OnEncodingApplyFailed()
{
	// A page-side (MHT loader) re-decode of the user's chosen code page could
	// not be applied. The loader optimistically had its tag marked active, so
	// abandon that pick and hand the view back to auto-detection: clear the
	// checked entry, drop the user-pick state, and re-arm the one-shot auto
	// latch. The MHT loader re-runs its detection, whose fresh report
	// (CMD_AUTO_ENCODING_REPORT) now passes the gate and restores the
	// "Auto: <tag>" hint on the menu.
	m_activeEncodingTag.clear();
	m_userPicked = false;
	m_autoApplied = false;
	m_autoAlreadyApplied = false;
	m_autoSuggestedTag.clear();
}
//------------------------------------------------------------------------
