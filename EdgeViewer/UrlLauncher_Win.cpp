#include "UrlLauncher.h"
#include "Globals.h"
#include "WebPolicy.h"

#include <windows.h>
#include <string>

//------------------------------------------------------------------------
// Windows implementation of the URL-launch surface declared in
// UrlLauncher.h. This TU pulls in <windows.h> for ShellExecuteW, so it
// lives only in the Win32/x64 build (EdgeViewer.vcxproj), mirroring
// Platform_Win.cpp.
bool IsScannableWebLink(const std::wstring& link)
{
	// Scheme prefix, ASCII case-insensitive. A link whose href lacks a
	// scheme (relative reference) is engine-resolved by the time it
	// reaches the context menu, so it always carries one.
	const std::wstring low = [&] {
		std::wstring s = link;
		for (auto& c : s)
			if (c >= L'A' && c <= L'Z')
				c += L'a' - L'A';
		return s;
	}();
	const bool webScheme =
		low.starts_with(L"http://") || low.starts_with(L"https://");

	// The plugin's own local surface (virtual hosts serving local files,
	// the ev:// and evh:// bridges) is never scannable: IsLocalUri is the
	// shared OfflineMode classification of exactly those refs.
	return webScheme && !IsLocalUri(link);
}
//------------------------------------------------------------------------
// Build the VirusTotal deep link for a URL scan. The current web UI
// accepts the target URL as a URL-safe base64 token (no padding) in the
// path — the form the official VirusTotal context-menu extensions use
// (e.g. https://www.virustotal.com/gui/url/<b64>/detection). The target
// is encoded as UTF-8 bytes so non-ASCII links survive the round trip.
std::wstring BuildVirusTotalUrl(const std::wstring& link)
{
	static constexpr char kAlphabet[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

	const std::string utf8 = to_utf8(link);

	// URL-safe base64 (RFC 4648 §5), no padding.
	std::string b64;
	b64.reserve(((utf8.size() + 2) / 3) * 4);
	for (size_t i = 0; i < utf8.size(); i += 3)
	{
		const uint32_t b0 = static_cast<uint8_t>(utf8[i]);
		const uint32_t b1 = (i + 1 < utf8.size()) ? static_cast<uint8_t>(utf8[i + 1]) : 0;
		const uint32_t b2 = (i + 2 < utf8.size()) ? static_cast<uint8_t>(utf8[i + 2]) : 0;
		const uint32_t t = (b0 << 16) | (b1 << 8) | b2;
		b64 += kAlphabet[(t >> 18) & 0x3F];
		b64 += kAlphabet[(t >> 12) & 0x3F];
		if (i + 1 < utf8.size())
			b64 += kAlphabet[(t >> 6) & 0x3F];
		if (i + 2 < utf8.size())
			b64 += kAlphabet[t & 0x3F];
	}

	return L"https://www.virustotal.com/gui/url/" + to_utf16(b64) + L"/detection";
}
//------------------------------------------------------------------------
// Hand the URL to the OS's default browser (the same "open in browser"
// path the plugin uses for print output and the sample File menu apps).
// ShellExecuteW returns a process handle; error codes are <= 32.
bool OpenInDefaultBrowser(const std::wstring& url)
{
	// ShellExecuteW returns a HINSTANCE; the low values 0..32 signal an
	// error, every real success is a large opaque handle value.
	const std::uintptr_t result = reinterpret_cast<std::uintptr_t>(
		ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
	return result > 32;
}
//------------------------------------------------------------------------