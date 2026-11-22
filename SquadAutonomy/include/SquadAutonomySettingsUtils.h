#pragma once

#include "rapidjson/document.h"
#include <string>

namespace SquadAutonomy
{
namespace SettingsUtils
{
    bool parseBoolSetting(rapidjson::Value& member, const std::string& key, bool& outValue);
    bool parseFloatSetting(rapidjson::Value& member, const std::string& key, float& outValue);
    bool parseIntSetting(rapidjson::Value& member, const std::string& key, int& outValue);
    bool parseBoolSetting(const std::string& stringValue, bool& outValue);
    bool parseFloatSetting(const std::string& stringValue, float& outValue);
}
}
