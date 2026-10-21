#include "Globals.h"
#include "Platform.h"

#include <fstream>
#include <sstream>
#include <format>
#include <codecvt>
#include <locale>
#include <algorithm>

std::map<void*, std::shared_ptr<IWebView>> gs_Views;
#ifdef _WIN32
HINSTANCE gs_PluginInstance;
#endif
bool gs_IsDarkMode;
std::map<const ProcessorInterface*, double> gs_ZoomFactor;
std::vector<std::wstring> gs_tempFiles;
//------------------------------------------------------------------------
std::string to_utf8(const std::wstring& in)
{
	// C++23 still deprecates wstring_convert, but both toolchains (MSVC
	// stdcpplatest, GCC -std=c++23) accept it; it is the only
	// <codecvt> entry point that works on both platforms.
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	return conv.to_bytes(in);
}
//------------------------------------------------------------------------
std::wstring to_utf16(const std::string& in)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
	return conv.from_bytes(in);
}
//------------------------------------------------------------------------
int to_int(const std::string& in)
{
    return atoi(in.c_str());
}
//------------------------------------------------------------------------
void RemoveTempFiles()
{
    for (const auto& path : gs_tempFiles)
        fs::remove(path);
}
//------------------------------------------------------------------------
namespace
{
// A configuration file is usable when it parses into at least one section.
// An absent, unreadable or section-less file (empty file, comments only) is
// not usable: it would leave the plugin claiming no file types at all.
bool LoadIniFile(const fs::path& path, mINI::INIStructure& ini)
{
    std::error_code ec;
    if (!fs::exists(path, ec))
        return false;

    mINI::INIFile file(to_utf8(path.wstring()));
    if (!file.read(ini))
        return false;

    return ini.size() > 0;
}
} // anonymous namespace
//------------------------------------------------------------------------
// The configuration file the plugin will parse: the user's edgeviewer.ini
// when it yields a usable configuration, otherwise the shipped
// edgeviewer.ini.sample (a read-only plugin directory cannot host a
// generated ini), otherwise none.
fs::path ResolveConfigSource(const fs::path& pluginDir)
{
    mINI::INIStructure probe;
    for (const auto* name : { INI_NAME, INI_SAMPLE_NAME })
    {
        const auto candidate = pluginDir / name;
        if (LoadIniFile(candidate, probe))
            return candidate;
    }
    return {};
}
//------------------------------------------------------------------------
// Best-effort creation of edgeviewer.ini from edgeviewer.ini.sample, so a
// writable install gives the user an editable, commented config next to the
// DLL. An existing ini is never modified (plugin updates must not discard
// user customizations). A failure is not an error: a read-only plugin
// directory is a legitimate install layout, and the configuration then
// falls back to the sample itself.
bool BootstrapUserIni(const fs::path& pluginDir)
{
    const auto iniFile = pluginDir / INI_NAME;
    const auto sampleFile = pluginDir / INI_SAMPLE_NAME;

    std::error_code ec;
    if (fs::exists(iniFile, ec))
        return true;
    if (!fs::exists(sampleFile, ec))
        return false;

    ec.clear();
    fs::copy_file(sampleFile, iniFile, fs::copy_options::skip_existing, ec);
    return !ec;
}
//------------------------------------------------------------------------
mINI::INIStructure& GlobalSettings()
{
    static mINI::INIStructure ini;
    static bool loaded = false;

    if (!loaded)
    {
        loaded = true;

        const auto pluginDir = fs::path(GetModulePath());

        BootstrapUserIni(pluginDir);

        // Config source order: the user's ini, then the shipped sample,
        // then nothing (no configuration -> no file types claimed).
        const auto source = ResolveConfigSource(pluginDir);
        if (!source.empty())
        {
            mINI::INIFile file(to_utf8(source.wstring()));
            file.read(ini);
        }

        if (!ini["WebView"].has("UserDir"))
            ini["WebView"]["UserDir"] = to_utf8(pluginDir.wstring());
    }

    return ini;
}
//------------------------------------------------------------------------
std::string ReadFile(const fs::path& path)  // presumed to be utf-8
{
    std::ifstream t(path);
    std::stringstream buffer;
    buffer << t.rdbuf();
    return buffer.str();
}
//------------------------------------------------------------------------
// Decode base64 that may use the URL-safe alphabet (`-`/`_` in place of
// `+`/`/`). Padding `=` is optional. Returns the raw bytes; on malformed
// input returns an empty vector (which callers treat as a save failure).
std::vector<uint8_t> DecodeBase64UrlSafe(const std::string& in)
{
	static const char table[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
	std::vector<uint8_t> out;
	out.reserve((in.size() / 4) * 3);
	uint32_t acc = 0;
	int bits = 0;
	for (char c : in)
	{
		if (c == '=')
			break; // padding ends the payload
		size_t pos = std::find(table, table + 64, c) - table;
		if (pos == 64)
			continue; // whitespace/newline — ignore
		acc = (acc << 6) | static_cast<uint32_t>(pos);
		bits += 6;
		if (bits >= 8)
		{
			bits -= 8;
			out.push_back(static_cast<uint8_t>((acc >> bits) & 0xFF));
		}
	}
	return out;
}
//------------------------------------------------------------------------
// Strip characters that would corrupt a filesystem path or be unsafe on
// the target OS, mirroring the loader's JS-side sanitize.
std::wstring SanitizeAttachmentName(const std::wstring& name)
{
	std::wstring out = name;
	out.erase(0, out.find_first_not_of(L" ."));
	size_t end = out.find_last_not_of(L" .");
	if (end != std::wstring::npos)
		out.resize(end + 1);
	for (size_t i = 0; i < out.size(); ++i)
	{
		wchar_t c = out[i];
		if (c == L'|' || c == L'\"' || c == L'\r' || c == L'\n' || c == L'\t' ||
		    c == L'\\' || c == L'/' || c == L':' || c == L'*' || c == L'?' ||
		    c == L'<' || c == L'>')
			out[i] = L'_';
	}
	return out.empty() ? L"attachment" : out;
}
//------------------------------------------------------------------------
// Build the JS that reports a save result to the loader's
// `window.__emlSaveResult(status, message)` callback. The message is
// escaped for embedding inside a JS single-quoted string: backslashes,
// single quotes, newlines and tabs become escapes so no quote
// terminates the JS early.
std::wstring BuildSaveResultScript(const std::wstring& status, const std::wstring& message)
{
	std::wstring escaped;
	for (wchar_t c : message)
	{
		switch (c)
		{
		case L'\\': escaped += L"\\\\"; break;
		case L'\'': escaped += L"\\'";  break;
		case L'\n': escaped += L"\\n";  break;
		case L'\r': escaped += L"\\r";  break;
		case L'\t': escaped += L"\\t";  break;
		default:    escaped += c;       break;
		}
	}

	std::wstring script = L"window.__emlSaveResult && window.__emlSaveResult('";
	script += status;
	script += L"','";
	script += escaped;
	script += L"');";
	return script;
}
//------------------------------------------------------------------------
// Write `bytes` to `<folder>/<filename>`, sanitizing the name first.
bool SaveAttachmentToFolder(const std::wstring& folder, const std::wstring& filename,
                            std::vector<uint8_t>& bytes)
{
	std::wstring safeName = SanitizeAttachmentName(filename);
	if (folder.empty() || safeName.empty())
		return false;

	fs::path target = fs::path(folder) / safeName;
	std::ofstream out(target, std::ios::binary | std::ios::trunc);
	if (!out)
		return false;
	out.write(reinterpret_cast<const char*>(bytes.data()),
	          static_cast<std::streamsize>(bytes.size()));
	return out.good();
}
//------------------------------------------------------------------------
