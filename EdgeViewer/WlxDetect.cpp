#include "WlxDetect.h"
#include <cstring>
#include <regex>
#include <vector>
#include <cstdlib>

//------------------------------------------------------------------------
std::string BuildDetectString(const mINI::INIStructure& ini)
{
	// convert ext1,ext2,ext3 into EXT="ext1"|EXT="ext2"|EXT="ext3"

	// NOTE(mm): all type sections should be listed here!
	static const std::vector<std::string> secs = { "HTML", "Markdown", "AsciiDoc", "URL", "MHTML", "EML", "RST", "Images", "Other" };

	const auto& extIni = ini.get("Extensions");
	auto exts = "EXT=\"" + extIni.get(secs[0]);

	for (auto v = secs.begin() + 1; v != secs.end(); ++v)
		exts += "," + extIni.get(*v);

	if (std::atoi(extIni.get("Dirs").c_str()))
		exts += ",";	// directories match the empty extension

	exts += "\"";

	return std::regex_replace(exts, std::regex(","), "\"|EXT=\"");
}
//------------------------------------------------------------------------
bool CopyDetectStringBounded(const std::string& str, char* dest, int maxlen)
{
	// Token format is fixed by BuildDetectString: `EXT="..."` tokens joined
	// by '|' with no '|' inside a token (extension lists are comma-separated
	// and never contain '|'). So cutting after the last '|' within the
	// buffer keeps only whole extensions. If that assumption ever changes,
	// this boundary scan must be revisited.
	if (!dest || maxlen <= 0)
		return false;

	const size_t limit = static_cast<size_t>(maxlen) - 1;	// room for the NUL

	if (str.size() <= limit)
	{
		std::memcpy(dest, str.c_str(), str.size());
		dest[str.size()] = '\0';
		return true;
	}

	if (limit == 0)
	{
		dest[0] = '\0';
		return false;
	}

	// Largest '|' boundary strictly before the limit (ending exactly at the
	// boundary would leave no room for the NUL).
	const size_t cut = str.find_last_of('|', limit - 1);
	if (cut == std::string::npos)
	{
		// Even one token doesn't fit; emit an empty (harmless) string.
		dest[0] = '\0';
		return false;
	}

	size_t len = cut + 1;
	if (str[len - 1] == '|')
		--len;	// drop the trailing boundary separator
	std::memcpy(dest, str.c_str(), len);
	dest[len] = '\0';
	return false;
}
//------------------------------------------------------------------------