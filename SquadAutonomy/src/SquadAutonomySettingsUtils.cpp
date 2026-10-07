#include "SquadAutonomySettingsUtils.h"

#include <sstream>

namespace SquadAutonomy
{
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
