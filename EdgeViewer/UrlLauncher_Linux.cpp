// Linux-only — compiled only when Qt Web Engine headers are on the include path.
#include "UrlLauncher.h"
#include "Globals.h"
#include "WebPolicy.h"

#include <QDesktopServices>
#include <QString>
#include <QUrl>

#include <string>

//------------------------------------------------------------------------
// Linux implementation of the URL-launch surface declared in
// UrlLauncher.h, mirroring UrlLauncher_Win.cpp. The predicate and scan
// URL are intentionally identical to the Windows variant so both
// platforms deep-link to VirusTotal the same way; only the OS "open in
// default browser" call differs (QDesktopServices vs ShellExecuteW).
bool IsScannableWebLink(const std::wstring& link)
{
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
// Hand the URL to the OS's default browser via Qt's portable
// QDesktopServices::openUrl, the same call the backend uses to open the
// print-to-PDF output.
bool OpenInDefaultBrowser(const std::wstring& url)
{
	return QDesktopServices::openUrl(
		QUrl(QString::fromStdString(to_utf8(url))));
}
//------------------------------------------------------------------------