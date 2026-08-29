#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <utility>

#include "IWebView.h"

//------------------------------------------------------------------------
// Record-replay IWebView: captures every call so tests can assert on the
// sequence of (method, arguments) the processors issue. Only the 5 nav
// methods plus the lifecycle Close() are tracked; nothing escapes.
class MockWebView : public IWebView
{
public:
	struct HostMapping
	{
		std::wstring host;
		std::filesystem::path folder;
	};

	std::vector<std::wstring> navigateToStringHtml;
	std::vector<std::wstring> navigateUris;
	std::vector<std::wstring> executedScripts;
	std::vector<std::wstring> documentCreatedScripts;
	std::vector<std::string> baseHrefs;
	std::vector<HostMapping> hostMappings;
	std::vector<std::filesystem::path> currentFileDirs;
	int closeCount = 0;

	void NavigateToString(const std::wstring& html,
	                       const std::string& = "") override { navigateToStringHtml.push_back(html); }
	void Navigate(const std::wstring& uri) override { navigateUris.push_back(uri); }
	void SetHtmlBaseHref(const std::string& baseHref) override { baseHrefs.push_back(baseHref); }
	void SetCurrentFileDirectory(const std::filesystem::path& path) override { currentFileDirs.push_back(path); }
	void ExecuteScript(const std::wstring& js) override { executedScripts.push_back(js); }
	void AddScriptToExecuteOnDocumentCreated(const std::wstring& js) override { documentCreatedScripts.push_back(js); }
	void RegisterVirtualHost(const std::wstring& host, const std::filesystem::path& folder) override
	{
		hostMappings.push_back({ host, folder });
	}
	void Close() override { ++closeCount; }
	void ApplyCharsetOverride(const std::wstring& /*tag*/) override {}
	void ApplyAutoDetectedEncoding(const std::wstring& /*tag*/) override {}
	void ReportAutoDetectedEncoding(const std::wstring& /*tag*/) override {}

	// Assertion helpers
	bool hasHostMapping(const std::wstring& host) const
	{
		for (const auto& m : hostMappings)
			if (m.host == host) return true;
		return false;
	}

	const HostMapping* findHostMapping(const std::wstring& host) const
	{
		for (const auto& m : hostMappings)
			if (m.host == host) return &m;
		return nullptr;
	}
};
//------------------------------------------------------------------------
