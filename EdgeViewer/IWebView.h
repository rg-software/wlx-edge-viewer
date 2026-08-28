#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

//------------------------------------------------------------------------
// Shared, platform-agnostic web view interface. The concrete backend
// (WebView2 on Windows, Qt Web Engine on Linux) lives in a platform-only
// translation unit; processors and Navigator call only these methods.
class IWebView
{
public:
	virtual ~IWebView() = default;

	virtual void NavigateToString(const std::wstring& html,
	                               const std::string& baseUri = "") = 0;
	virtual void Navigate(const std::wstring& uri) = 0;
	virtual void ExecuteScript(const std::wstring& js) = 0;
	virtual void AddScriptToExecuteOnDocumentCreated(const std::wstring& js) = 0;
	virtual void RegisterVirtualHost(const std::wstring& host, const std::filesystem::path& folder) = 0;
	virtual void Print() { ExecuteScript(L"window.print();"); }
	virtual void Close() = 0;

	// Pre-fetch: inject raw file bytes (base64-encoded) into the page
	// as window.__evRawFileBytesB64 so the encoding override JS can
	// re-decode without fetch() — custom ev:// schemes don't support
	// fetch() from the page context in Qt Web Engine.  Must be called
	// before Navigate(); default no-op (Windows WebView2 doesn't need it).
	virtual void SetRawFileBytes(const std::vector<uint8_t>&) {}

	// The HTML file's <base href> (e.g. "http://local.example/<urlDir>/"),
	// used by the host-side re-decode (ApplyCharsetOverride) to rebuild
	// relative-ref resolution when it re-renders the pristine bytes as an
	// embedded string. The processor sets it during OpenIn; the backend
	// retains it for the override/auto-detect re-render. Default no-op.
	virtual void SetHtmlBaseHref(const std::string& baseHref) {}

	// Manual encoding selection (issue #66): the host-side native
	// "Encoding" submenu is only meaningful on views whose processor can
	// re-decode its source bytes (HTML, MHT). The processor reports this
	// through supportsEncodingOverride() during OpenIn so the backend can
	// gate the menu without guessing from the page URL. Default no-op:
	// WebView2 gates via the processor pointer directly, Qt Web Engine
	// stores the flag here.
	virtual void SetEncodingOverrideSupported(bool) {}

	// Distinguishes the TWO host-visible re-decode schemes under the
	// single Encoding submenu (issue #66 / html-charset-override):
	//   - HTML views re-decode HOST-SIDE: ApplyCharsetOverride splices a
	//     <meta charset> into the pristine cached bytes and re-renders.
	//   - MHT views re-decode PAGE-SIDE: the mhtml loader defines
	//     window.__evEncodingApply and the backend dispatches the tag to
	//     it via ExecuteScript. ApplyCharsetOverride on an MHT view MUST
	//     therefore take that branch, never the byte-splice.
	// Processors report which during OpenIn (HtmlProcessor -> true;
	// BaseFileProcessor/MHT -> false). Default no-op.
	virtual void SetEncodingOverrideHtml(bool) {}

	// Host-side HTML charset override (html-charset-override change,
	// design D3/D4): the backend re-splices `<meta charset="<tag>">` plus
	// the same `<base href>` the processor used into its cached pristine
	// source bytes and performs a fresh embedded-string render, so the
	// engine's own HTML parser does the decoding. Empty `tag` = "Auto-detect"
	// (re-render pristine bytes with engine sniffing). On MHT views
	// (SetEncodingOverrideHtml(false)) the tag is instead dispatched to the
	// loader's window.__evEncodingApply. Both backends implement it.
	virtual void ApplyCharsetOverride(const std::wstring& tag) = 0;

	// Active encoding override for the current view, used to check the
	// right entry in the Encoding submenu. Empty string = "Auto-detect"
	// (engine sniffing is active). Transient: resets to empty on every
	// Navigate/NavigateToString. Default returns empty (both backends
	// override it).
	virtual std::wstring GetActiveEncodingTag() const { return L""; }

	// Provisional auto-detected encoding (charset-autodetect change): called
	// by the host when the page posts CMD_AUTO_ENCODING (<tag>). Applies the
	// same host-side transcode as ApplyCharsetOverride but marks the result
	// as AUTO (not user-picked), so a later explicit menu pick still wins and
	// the Auto-detect menu item can show the suggestion. Empty tag = no-op.
	virtual void ApplyAutoDetectedEncoding(const std::wstring& tag) = 0;

	// Display-only counterpoint to ApplyAutoDetectedEncoding: called by the
	// host when the page posts CMD_AUTO_ENCODING_REPORT (<tag>) because the
	// engine already decoded the bytes with the detected code page (data shown
	// as-is, or a genuine declared charset), so no re-decode is needed. Records
	// the tag so the Encoding submenu can display "Auto: <codepage>" even though
	// nothing was re-rendered. Never mutates the view. Empty tag = no-op.
	virtual void ReportAutoDetectedEncoding(const std::wstring& tag) = 0;

	// The encoding auto-detection suggested for the current view ("" if none
	// or if the user picked manually). Used by the menu to render e.g.
	// "Auto-detect (Windows-1251)". Default returns empty.
	virtual std::wstring GetAutoSuggestedTag() const { return L""; }
};
//------------------------------------------------------------------------
