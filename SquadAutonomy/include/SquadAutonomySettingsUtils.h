#pragma once

#include <string>

namespace SquadAutonomy
{
namespace SettingsUtils
{
    bool parseBoolSetting(const std::string& stringValue, bool& outValue);
    bool parseFloatSetting(const std::string& stringValue, float& outValue);
}
}
