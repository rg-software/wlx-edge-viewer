#include "WebPolicy.h"

//------------------------------------------------------------------------
// Local = never leaves the machine:
//   - about:/data:/blob:  engine-internal pages and inline payloads
//     (NavigateToString documents, base64-embedded EML/MHT resources,
//     script-created blob workers);
//   - ev:                 the Linux backend's custom scheme, including
//     the ev://_close and ev://_cmd JS->host bridges;
//   - http(s)://assets.example and http(s)://local.example — the fixed
//     virtual hosts registered by ProcessorInterface::mapDomains;
//   - http(s)://lister.example — WebView2Backend's virtual host for
//     oversized (past NavigateToString's 2 MB cap) loader HTML served
//     from a temp file (see WebView2Backend.cpp).
// Everything else (remote http/https authorities, file:, ftp:, ws:, ...)
// is non-local; URIs without a parsable scheme fail closed.
//
// Only ASCII delimiters are inspected and compared against ASCII
// literals, so the narrow overload is authoritative and the wide one is
// a byte-widening of it (scheme/authority characters are ASCII by
// definition; other characters cannot affect the parse).
namespace
{
std::string asciiLower(std::string s)
{
	for (auto& c : s)
		if (c >= 'A' && c <= 'Z')
			c += 'a' - 'A';
	return s;
}
}
//------------------------------------------------------------------------
bool IsLocalUri(const std::string& uri)
{
	// scheme = leading run up to ':' (URI schemes are "ALPHA *( ALPHA /
	// DIGIT / + / - / . )" and case-insensitive)
	const auto colon = uri.find(':');
	if (colon == std::string::npos || colon == 0)
		return false;
	for (size_t i = 0; i < colon; ++i)
	{
		const char c = uri[i];
		const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(i > 0 && ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.'));
		if (!ok)
			return false;
	}

	const auto scheme = asciiLower(uri.substr(0, colon));
	if (scheme == "about" || scheme == "data" || scheme == "blob" || scheme == "ev")
		return true;
	if (scheme != "http" && scheme != "https")
		return false;

	// authority = between "://" and the first '/', '?' or '#'
	if (uri.size() < colon + 3 || uri[colon + 1] != '/' || uri[colon + 2] != '/')
		return false;
	const auto start = colon + 3;
	const auto end = uri.find_first_of("/?#", start);
	auto host = asciiLower(uri.substr(start, end == std::string::npos ? std::string::npos : end - start));

	// Be lenient about an explicit default port ("host:80")
	const auto portPos = host.find(':');
	if (portPos != std::string::npos)
		host.resize(portPos);

	return host == "assets.example" || host == "local.example" || host == "lister.example";
}
//------------------------------------------------------------------------
bool IsLocalUri(const std::wstring& uri)
{
	std::string narrow;
	narrow.reserve(uri.size());
	for (wchar_t c : uri)
		narrow.push_back(static_cast<char>(c));
	return IsLocalUri(narrow);
}
//------------------------------------------------------------------------
