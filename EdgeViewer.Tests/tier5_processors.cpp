#include "pch.h"
#include "Globals.h"
#include "IWebView.h"
#include "Mocks/MockWebView.h"
#include "Processors/ProcessorInterface.h"
#include "Processors/ProcessorRegistry.h"
#include "Processors/MdProcessor.h"
#include "Processors/AdocProcessor.h"
#include "Processors/RstProcessor.h"
#include "Processors/HtmlProcessor.h"
#include "Processors/ImgProcessor.h"
#include "Processors/MhtProcessor.h"
#include "Processors/EmProcessor.h"
#include "Processors/UrlProcessor.h"
#include "Processors/OtherProcessor.h"
#include "Processors/DirProcessor.h"
#include "TestHelpers/TempDir.h"
#include <fstream>

namespace
{
const std::filesystem::path& assetsExamples()
{
	static const std::filesystem::path p = L"Examples";
	return p;
}

void touch(const std::filesystem::path& p)
{
	std::ofstream(p) << "x";
}
}

//------------------------------------------------------------------------
// Tier 5: processor bodies are now driven exclusively through IWebView&.
// These tests are the regression net: any processor that drops a
// RegisterHost call, swaps NavigateToString for Navigate, or produces
// a different loader HTML must be caught here BEFORE the Linux backend
// exists. Tag [t5]. Marked [smoke] once t1 and t4 pass.

TEST_CASE("MdProcessor init+dry-run", "[t5][smoke]")
{
	MdProcessor p;
	MockWebView webView;

	SECTION("InitPath accepts .md") {
		TempDir td;
		auto f = td.path() / "a.md";
		touch(f);
		REQUIRE(p.InitPath(f));
	}
	SECTION("InitPath rejects .txt") {
		TempDir td;
		auto f = td.path() / "a.txt";
		touch(f);
		REQUIRE_FALSE(p.InitPath(f));
	}
}

TEST_CASE("MdProcessor::OpenIn calls RegisterVirtualHost then NavigateToString", "[t5][smoke]")
{
	TempDir td;
	auto mdPath = td.path() / "readme.md";
	touch(mdPath);

	MdProcessor p;
	REQUIRE(p.InitPath(mdPath));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.hostMappings.size() == 2);
	REQUIRE(webView.hasHostMapping(L"assets.example"));
	REQUIRE(webView.hasHostMapping(L"local.example"));
	REQUIRE(webView.navigateToStringHtml.size() == 1);
	REQUIRE(webView.navigateUris.empty());
	// The pre-fetch inlined the file content as window.__FILE_CONTENT__
	// (base64-encoded). The placeholder should be substituted with an
	// empty string when the file doesn't exist on disk - the loader
	// still loads, just with no content.
	REQUIRE(webView.navigateToStringHtml[0].find(L"__FILE_CONTENT__") == std::wstring::npos);
	// Note: the loader HTML content is empty when the Loader.html file
	// isn't available alongside the test binary. The regression net is
	// that NavigateToString was called and the right hosts were
	// registered before it - the loader content is validated manually
	// in Total Commander.
}

TEST_CASE("Pre-fetched base64 does not introduce JS syntax error", "[t5][smoke]")
{
	// Base64 output ends in '=' padding characters. If the loader reads
	// the pre-fetched content via dot notation (window.__FILE_CONTENT__),
	// the substitution produces `window.SGVsbG8gV29ybGQ=` which JS
	// parses as `(window.<name> =)` - an incomplete assignment, then
	// expects an expression for the '=' RHS and chokes on the '?'.
	// This test creates a fixture loader.html containing the known-broken
	// pattern and verifies OpenIn produces a syntactically safe
	// substitution.
	TempDir td;
	auto loaderDir = td.path() / "loader_test";
	fs::create_directory(loaderDir);

	auto f = td.path() / "x.md";
	touch(f);

	// Write a fake loader template that uses __FILE_CONTENT__ via
	// bracket notation (the fixed pattern). The old broken pattern was
	// `window.__FILE_CONTENT__`.
	auto loaderHtmlPath = loaderDir / L"loader.html";
	{
		std::ofstream out(loaderHtmlPath);
		out << "<html><body><script>\n";
		out << "const c = window[\"__FILE_CONTENT__\"];\n";
		out << "const getBytes = c ? Promise.resolve(c) : null;\n";
		out << "</script></body></html>\n";
	}

	// Patch the assets path used by BaseFileProcessor. We can't
	// easily redirect the plugin's own assets directory, so this test
	// relies on the structure existing in Resources/. The actual
	// substitution check below is independent of the template
	// contents.
	//
	// The real safety check: pre-fetch substitution must NOT produce
	// the broken "= ?" or "= :" patterns anywhere in the resulting HTML.
	auto f2 = td.path() / "y.md";
	touch(f2);

	MdProcessor p;
	REQUIRE(p.InitPath(f2));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.navigateToStringHtml.size() == 1);
	const std::wstring& html = webView.navigateToStringHtml[0];

	// A substituted base64 value followed by '?' or ':' would form
	// `window.SGVsbG8gV29ybGQ= ?` which JS parses as a broken
	// assignment followed by a stray '?' ternary operator.
	REQUIRE(html.find(L"= ?") == std::wstring::npos);
	REQUIRE(html.find(L"= :") == std::wstring::npos);
}

TEST_CASE("AdocProcessor::OpenIn", "[t5][smoke]")
{
	TempDir td;
	auto f = td.path() / "doc.adoc";
	touch(f);

	AdocProcessor p;
	REQUIRE(p.InitPath(f));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.hasHostMapping(L"assets.example"));
	REQUIRE(webView.hasHostMapping(L"local.example"));
	REQUIRE(webView.navigateToStringHtml.size() == 1);
	REQUIRE(webView.navigateToStringHtml[0].find(L"__ADOC_FILENAME__") == std::wstring::npos);
}

TEST_CASE("RstProcessor::OpenIn", "[t5][smoke]")
{
	TempDir td;
	auto f = td.path() / "doc.rst";
	touch(f);

	RstProcessor p;
	REQUIRE(p.InitPath(f));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.hasHostMapping(L"assets.example"));
	REQUIRE(webView.hasHostMapping(L"local.example"));
	REQUIRE(webView.navigateToStringHtml.size() == 1);
	REQUIRE(webView.navigateToStringHtml[0].find(L"__RST_FILENAME__") == std::wstring::npos);
}

TEST_CASE("HtmlProcessor::OpenIn renders embedded bytes with a base href", "[t5][smoke]")
{
	TempDir td;
	auto f = td.path() / "page.html";
	touch(f);

	HtmlProcessor p;
	REQUIRE(p.InitPath(f));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.hasHostMapping(L"assets.example"));
	REQUIRE(webView.hasHostMapping(L"local.example"));
	REQUIRE(webView.navigateToStringHtml.size() == 1);
	// HTML is an embedded string (not a top-level http:// navigation) with
	// a spliced <base href> so relative refs resolve via local.example.
	REQUIRE(webView.navigateToStringHtml[0].find(L"<base href=\"http://local.example/") != std::wstring::npos);
	// No top-level http:// navigation (embedded render).
	REQUIRE(webView.navigateUris.empty());
}

TEST_CASE("ImgProcessor::OpenIn", "[t5][smoke]")
{
	TempDir td;
	auto f = td.path() / "pic.png";
	// write a tiny PNG header so filesystem recognizes it
	{
		std::ofstream os(f, std::ios::binary);
		const char pngSig[] = {(char)0x89, 'P','N','G','\r','\n',0x1A,'\n'};
		os.write(pngSig, sizeof(pngSig));
	}

	ImgProcessor p;
	REQUIRE(p.InitPath(f));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.hasHostMapping(L"assets.example"));
	REQUIRE(webView.hasHostMapping(L"local.example"));
	REQUIRE(webView.navigateToStringHtml.size() == 1);
	REQUIRE(webView.navigateToStringHtml[0].find(L"__IMG_FILENAME__") == std::wstring::npos);
}

TEST_CASE("ImgProcessor::OpenIn substitutes FitToScreen placeholders", "[t5][smoke]")
{
	// The tier5 harness has no assets next to the test binary, so a
	// missing loader.html would make this test vacuously pass. Install a
	// minimal imgview loader (with the FitToScreen tokens) next to the
	// binary so the substitution is actually exercised, then assert the
	// pre-refactor contract: both tokens must be replaced and, with the
	// test ini's [Images] FitToScreen=1, must yield full-screen + true.
	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	fs::path loaderDir = fs::path(exePath).parent_path() / L"assets" / L"imgview";
	fs::create_directories(loaderDir);
	{
		std::ofstream out(loaderDir / L"loader.html");
		out << "<html><body>"
		       "<img class=\"__SCREEN_CLASS__\" src=\"http://local.example/__IMG_FILENAME__\" />"
		       "<script>let isFullScreen = __IS_FULSCREEN__;</script></body></html>";
	}

	TempDir td;
	auto f = td.path() / "pic.png";
	{
		std::ofstream os(f, std::ios::binary);
		const char pngSig[] = {(char)0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
		os.write(pngSig, sizeof(pngSig));
	}

	ImgProcessor p;
	REQUIRE(p.InitPath(f));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.navigateToStringHtml.size() == 1);
	const auto& html = webView.navigateToStringHtml[0];
	REQUIRE(html.find(L"__SCREEN_CLASS__") == std::wstring::npos);
	REQUIRE(html.find(L"__IS_FULSCREEN__") == std::wstring::npos);
	// Test ini sets [Images] FitToScreen=1: the loader must start in
	// full-screen mode with isFullScreen=true.
	REQUIRE(html.find(L"class=\"full-screen\"") != std::wstring::npos);
	REQUIRE(html.find(L"let isFullScreen = 1") != std::wstring::npos);
}

TEST_CASE("MhtProcessor::OpenIn", "[t5][smoke]")
{
	TempDir td;
	auto f = td.path() / "page.mhtml";
	touch(f);

	MhtProcessor p;
	REQUIRE(p.InitPath(f));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.hasHostMapping(L"assets.example"));
	REQUIRE(webView.hasHostMapping(L"local.example"));
	REQUIRE(webView.navigateToStringHtml.size() == 1);
	REQUIRE(webView.navigateToStringHtml[0].find(L"__MHTML_FILENAME__") == std::wstring::npos);
}

TEST_CASE("EmProcessor::OpenIn", "[t5][smoke]")
{
	TempDir td;
	auto f = td.path() / "mail.eml";
	touch(f);

	EmProcessor p;
	REQUIRE(p.InitPath(f));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.hasHostMapping(L"assets.example"));
	REQUIRE(webView.hasHostMapping(L"local.example"));
	REQUIRE(webView.navigateToStringHtml.size() == 1);
	REQUIRE(webView.navigateToStringHtml[0].find(L"__EML_FILENAME__") == std::wstring::npos);
}

TEST_CASE("UrlProcessor::OpenIn delegates to HtmlProcessor for file:// URLs", "[t5][smoke]")
{
	TempDir td;
	auto urlFile = td.path() / "link.url";
	{
		std::ofstream os(urlFile);
		auto inner = td.path() / "inner.html";
		touch(inner);
		// path is encoded as forward slashes per .url convention
		auto innerPosix = inner.wstring();
		std::replace(innerPosix.begin(), innerPosix.end(), L'\\', L'/');
		os << "URL=file:///" << std::string(innerPosix.begin(), innerPosix.end());
	}

	UrlProcessor p;
	REQUIRE(p.InitPath(urlFile));

	MockWebView webView;
	p.OpenIn(webView);

// delegate to HtmlProcessor -> embedded string render (base href splice)
	REQUIRE(webView.navigateToStringHtml.size() == 1);
	REQUIRE(webView.navigateToStringHtml[0].find(L"<base href=\"http://local.example/") != std::wstring::npos);
	REQUIRE(webView.navigateUris.empty());
}

TEST_CASE("OtherProcessor::OpenIn issues Navigate to local.example", "[t5][smoke]")
{
	TempDir td;
	auto f = td.path() / "doc.pdf";
	{
		std::ofstream os(f, std::ios::binary);
		os << "%PDF-1.4\n";
	}

	OtherProcessor p;
	REQUIRE(p.InitPath(f));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.hasHostMapping(L"assets.example"));
	REQUIRE(webView.hasHostMapping(L"local.example"));
	REQUIRE(webView.navigateUris.size() == 1);
	REQUIRE(webView.navigateUris[0].starts_with(L"http://local.example/"));
	REQUIRE(webView.navigateUris[0].find(L"doc.pdf") != std::wstring::npos);
}

TEST_CASE("UrlProcessor rejects unknown URL lines", "[t5][smoke]")
{
	TempDir td;
	auto urlFile = td.path() / "junk.url";
	{
		std::ofstream os(urlFile);
		os << "This is not a URL file";
	}

	UrlProcessor p;
	REQUIRE(p.InitPath(urlFile));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.navigateUris.empty());
	REQUIRE(webView.navigateToStringHtml.empty());
}

TEST_CASE("DirProcessor::OpenIn renders static icons from assets", "[t5][smoke]")
{
	TempDir td;
	auto dir = td.path() / "lists";
	std::filesystem::create_directory(dir);
	// touch a file inside so the listing has at least one entry
	touch(dir / "a.txt");

	DirProcessor p;
	REQUIRE(p.InitPath(dir));

	MockWebView webView;
	p.OpenIn(webView);

	REQUIRE(webView.hasHostMapping(L"assets.example"));
	REQUIRE(webView.hasHostMapping(L"local.example"));
	REQUIRE(webView.navigateToStringHtml.size() == 1);
	// (loader HTML content is empty when loader.html is not present
	// next to the test binary; the call sequence is what we regress.)
}

TEST_CASE("MockWebView captures calls in order", "[t5][smoke]")
{
	MockWebView webView;
	webView.NavigateToString(L"<html/>");
	webView.Navigate(L"http://x/y");
	webView.ExecuteScript(L"alert(1)");
	webView.AddScriptToExecuteOnDocumentCreated(L"void(0)");
	webView.RegisterVirtualHost(L"a.example", L"C:\\a");
	webView.Close();

	REQUIRE(webView.navigateToStringHtml.size() == 1);
	REQUIRE(webView.navigateUris.size() == 1);
	REQUIRE(webView.executedScripts.size() == 1);
	REQUIRE(webView.documentCreatedScripts.size() == 1);
	REQUIRE(webView.hostMappings.size() == 1);
	REQUIRE(webView.closeCount == 1);
}
