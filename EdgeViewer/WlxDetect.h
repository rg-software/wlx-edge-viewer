#pragma once

#include <mini/ini.h>
#include <string>

// Build the Total Commander detect string from the [Extensions] section.
// Pure: no global state, no COM, no I/O — takes the ini structure as input.
std::string BuildDetectString(const mINI::INIStructure& ini);