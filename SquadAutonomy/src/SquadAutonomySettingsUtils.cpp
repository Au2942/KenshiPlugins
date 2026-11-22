#include "SquadAutonomySettingsUtils.h"

#include <sstream>

namespace SquadAutonomy
{
    bool SettingsUtils::parseBoolSetting(rapidjson::Value& member, const std::string& key, bool& outValue)
    {
        auto itr = member.FindMember(key.c_str());
        if (itr != member.MemberEnd())
        {
            outValue = (itr->value.GetBool());
            return true;
        }
        return false;
    }
    bool SettingsUtils::parseFloatSetting(rapidjson::Value& member, const std::string& key, float& outValue)
    {
        auto itr = member.FindMember(key.c_str());
        if (itr != member.MemberEnd())
        {
            outValue = (itr->value.GetFloat());
            return true;
        }
        return false;
    }
    bool SettingsUtils::parseIntSetting(rapidjson::Value& member, const std::string& key, int& outValue)
    {
        auto itr = member.FindMember(key.c_str());
        if (itr != member.MemberEnd())
        {
            outValue = (itr->value.GetInt());
            return true;
        }
        return false;
    }
    bool SettingsUtils::parseBoolSetting(const std::string &stringValue, bool &outValue)
    {
        std::string stringValueCopy = stringValue;
        stringValueCopy.erase(0, stringValueCopy.find_first_not_of(" \t"));

        if (stringValueCopy == "true" || stringValueCopy == "false")
        {
            outValue = stringValueCopy == "true";
            return true;
        }

        return false;
    }

    bool SettingsUtils::parseFloatSetting(const std::string &stringValue, float &outValue)
    {
        if (stringValue.empty())
        {
            return false;
        }

        std::stringstream ss(stringValue);
        float tempOutValue = 0;
        ss >> tempOutValue;
        if (ss)
        {
            outValue = tempOutValue;
            return true;
        }

        return false;
    }
}
