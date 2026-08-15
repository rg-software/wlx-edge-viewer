#pragma once

#include <mini/ini.h>
#include <string>

class IniBuilder {
public:
    IniBuilder& with(const std::string& section, const std::string& key, const std::string& value) {
        mIni[section][key] = value;
        return *this;
    }
    mINI::INIStructure build() { return std::move(mIni); }
private:
    mINI::INIStructure mIni;
};