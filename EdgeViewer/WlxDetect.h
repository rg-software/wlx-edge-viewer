#pragma once

#include <mini/ini.h>
#include <string>

// Build the Total Commander detect string from the [Extensions] section.
// Pure: no global state, no COM, no I/O — takes the ini structure as input.
std::string BuildDetectString(const mINI::INIStructure& ini);

// Fill `dest` (buffer of `maxlen` chars, NUL-terminated) with as many whole
// extension tokens from `str` as fit, stopping at a token boundary so no
// extension is partially written. Returns true if the entire string was
// copied; false if some tokens were omitted (dest then holds the maximal
// whole-token prefix).
// Pure: no global state, no COM, no I/O.
bool CopyDetectStringBounded(const std::string& str, char* dest, int maxlen);