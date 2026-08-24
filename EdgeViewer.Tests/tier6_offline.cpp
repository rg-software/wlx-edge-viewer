#include "pch.h"
#include "WebPolicy.h"

//------------------------------------------------------------------------
// Tier 6: the [WebView] OfflineMode URI-classification policy shared by
// both backends (WebViewFactory's WebResourceRequested handler on
// Windows, QtWebEngineBackend's QWebEngineUrlRequestInterceptor on
// Linux). Pure function - no COM/Qt objects involved.

TEST_CASE("IsLocalUri allows engine-internal schemes", "[t6]")
{
	SECTION("about:") {
		REQUIRE(IsLocalUri(L"about:blank"));
	}
	SECTION("data:") {
		REQUIRE(IsLocalUri(L"data:text/html;base64,PGI+aGk8L2I+"));
	}
	SECTION("blob:") {
		REQUIRE(IsLocalUri(L"blob:https://example.org/9c3d"));
	}
	SECTION("ev: incl. the JS->host bridges") {
		REQUIRE(IsLocalUri(L"ev://assets.example/markdown/loader.html"));
		REQUIRE(IsLocalUri(L"ev://local.example/doc.md"));
		REQUIRE(IsLocalUri(L"ev://_close/42"));
		REQUIRE(IsLocalUri(L"ev://_cmd/7/CMD_ZOOM|1.5"));
	}
}

TEST_CASE("IsLocalUri allows the mapped virtual hosts only", "[t6]")
{
	SECTION("http(s) to assets.example / local.example") {
		REQUIRE(IsLocalUri(L"http://assets.example/html/github.css"));
		REQUIRE(IsLocalUri(L"https://local.example/sub/page.html"));
	}
	SECTION("case-insensitive") {
		REQUIRE(IsLocalUri(L"http://ASSETS.EXAMPLE/style.css"));
		REQUIRE(IsLocalUri(L"HTTP://LOCAL.EXAMPLE/style.css"));
	}
	SECTION("explicit default port is tolerated") {
		REQUIRE(IsLocalUri(L"http://assets.example:80/style.css"));
	}
	SECTION("other http(s) authorities are non-local") {
		REQUIRE_FALSE(IsLocalUri(L"http://html.example/page.html"));
		REQUIRE_FALSE(IsLocalUri(L"https://example.net/pic.png"));
		REQUIRE_FALSE(IsLocalUri(L"http://local.example.evil.tld/pic.png"));
	}
	SECTION("userinfo does not smuggle a local authority through") {
		REQUIRE_FALSE(IsLocalUri(L"http://user@assets.example/style.css"));
	}
}

TEST_CASE("IsLocalUri blocks remote and unknown schemes", "[t6]")
{
	SECTION("file / ftp / websocket") {
		REQUIRE_FALSE(IsLocalUri(L"file:///C:/Docs/page.html"));
		REQUIRE_FALSE(IsLocalUri(L"ftp://files.example.org/pub/x.zip"));
		REQUIRE_FALSE(IsLocalUri(L"ws://chat.example.com/socket"));
		REQUIRE_FALSE(IsLocalUri(L"wss://chat.example.com/socket"));
	}
	SECTION("unknown scheme fails closed") {
		REQUIRE_FALSE(IsLocalUri(L"gopher://old.example.org/x"));
	}
}

TEST_CASE("IsLocalUri fails closed on malformed input", "[t6]")
{
	REQUIRE_FALSE(IsLocalUri(L""));
	REQUIRE_FALSE(IsLocalUri(L"notauri"));
	REQUIRE_FALSE(IsLocalUri(L":noscheme"));
	REQUIRE_FALSE(IsLocalUri(L"http:no-slashes"));
	REQUIRE_FALSE(IsLocalUri(L"http:/single-slash.local.example"));
	REQUIRE_FALSE(IsLocalUri(L"1http://assets.example"));
}

TEST_CASE("IsLocalUri narrow and wide overloads agree", "[t6]")
{
	const std::string samples[] = {
		"http://assets.example/html/github.css",
		"https://example.net/pic.png",
		"ev://_close/42",
		"data:text/plain,x",
		"file:///etc/passwd",
		"",
	};
	for (const auto& s : samples)
	{
		std::wstring wide;
		wide.reserve(s.size());
		for (unsigned char c : s)
			wide.push_back(static_cast<wchar_t>(c));
		REQUIRE(IsLocalUri(s) == IsLocalUri(wide));
	}
}
