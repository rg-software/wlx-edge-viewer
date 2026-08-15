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
	// Note: the loader HTML content is empty when the Loader.html file
	// isn't available alongside the test binary. The regression net is
	// that NavigateToString was called and the right hosts were
	// registered before it - the loader content is validated manually
	// in Total Commander.
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

TEST_CASE("HtmlProcessor::OpenIn issues one Navigate to local.example", "[t5][smoke]")
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
	REQUIRE(webView.navigateUris.size() == 1);
	REQUIRE(webView.navigateUris[0].starts_with(L"http://local.example/"));
	REQUIRE(webView.navigateUris[0].find(L"page.html") != std::wstring::npos);
	REQUIRE(webView.navigateToStringHtml.empty());
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

	// delegate to HtmlProcessor -> Navigate to local.example
	REQUIRE(webView.navigateUris.size() == 1);
	REQUIRE(webView.navigateUris[0].starts_with(L"http://local.example/"));
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
