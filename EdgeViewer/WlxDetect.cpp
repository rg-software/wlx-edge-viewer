#include "WlxDetect.h"
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