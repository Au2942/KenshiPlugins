#include "SquadAutonomyModSettings.h"

#include <fstream>
#include <memory>
#include <Debug.h>
#include <OgreStringConverter.h>
#include "SquadAutonomy.h"

using namespace SquadAutonomy;

ModSettings& ModSettings::getSingleton()
{
    static std::unique_ptr<ModSettings> singleton;
    if (!singleton)
    {
        singleton.reset(new ModSettings());
    }

    return *singleton.get();
}

ModSettings::ModSettings() :
    _configFileName(L"SquadAutonomy.cfg")
{
    if (!modPath.empty())
    {
        _configFilePath = modPath + _configFileName;
    }
    else
    {
        _configFilePath = GetCurrentDLLDirectory() + _configFileName;
    }
}

const ModSettings::SettingsValues& ModSettings::getValues() const
{
    return _settingsValues;
}

ModSettings::SettingsValues& ModSettings::getValuesMutable()
{
    return _settingsValues;
}

ModSettings::SettingsValues& ModSettings::getDefaultValues()
{
    static SettingsValues defaultSettingsValues;

    return defaultSettingsValues;
}

void ModSettings::resetToDefault()
{
    _settingsValues = getDefaultValues();
}

const std::vector<std::string> &ModSettings::getConfigPackages() const
{
    return _configPackages;
}

void ModSettings::loadFromFile()
{
    DebugLog("Finding Config file at: " + converter.to_bytes(_configFilePath));
    std::ifstream cfgFile(_configFilePath);
    if (!cfgFile.is_open())
    {
        DebugLog("Load: Cannot open config file");
        return;
    }
    DebugLog("Reading config file...");

    std::string line;
    std::string token;
    while (std::getline(cfgFile, line))
    {
        if (line == "<Options>")
        {
            while (std::getline(cfgFile, line))
            {
                if (line == "</Options>")
                {
                    break;
                }

                line.erase(0, line.find_first_not_of(" \t"));
                const auto colon = line.find(':');
                if (colon == std::string::npos)
                {
                    continue;
                }

                const std::string type = line.substr(0, colon);
                const std::string dataLine = line.substr(colon + 1);
                //DebugLog(type);
                if (type == "ShowOnMain")
                {
                    parseBoolSetting(dataLine, _settingsValues.showOnMain);
                    continue;
                }
                if (type == "LockPosition")
                {
                    parseBoolSetting(dataLine, _settingsValues.lockPosition);
                    continue;
                }
                if (type == "BtnWidth")
                {
                    parseFloatSetting(dataLine, _settingsValues.btnWidth);
                    continue;
                }
                if (type == "BtnHeight")
                {
                    parseFloatSetting(dataLine, _settingsValues.btnHeight);
                    continue;
                }
                if (type == "BtnLeft")
                {
                    parseFloatSetting(dataLine, _settingsValues.btnLeft);
                    continue;
                }
                if (type == "BtnTop")
                {
                    parseFloatSetting(dataLine, _settingsValues.btnTop);
                    continue;
                }
                if (type == "BtnFontSize")
                {
                    parseFloatSetting(dataLine, _settingsValues.btnFontSize);
                    continue;
                }
                if (type == "ShowInSquad")
                {
                    parseBoolSetting(dataLine, _settingsValues.showInSquad);
                    continue;
                }
                if (type == "EnableLogging")
                {
                    parseBoolSetting(dataLine, _settingsValues.enableLogging);
                    continue;
                }
                if (type == "LogSeverity")
                {
                    parseLogSeveritySetting(dataLine, _settingsValues.logSeverity);
                    continue;
                }
                if (type == "LogSpam")
                {
                    parseBoolSetting(dataLine, _settingsValues.logSpam);
                    continue;
                }
            }
        }
        else if (line == "<Packages>")
        {
            while (std::getline(cfgFile, line))
            {
                if (line == "</Packages>")
                {
                    DebugLog("Done reading packages in config");
                    break;
                }
                bool insideQuote = false;
                std::stringstream ssline(line);
                while (std::getline(ssline, token, '"'))
                {
                    if (!token.empty())
                    {
                        if (insideQuote)
                        {
                            _configPackages.push_back(token);
                            DebugLog("Detected package in config: " + token);
                        }
                    }
                    insideQuote = !insideQuote;
                }
            }
        }
    }
}

void ModSettings::saveToFile() const
{
    std::ofstream tempFile("temp.txt");
    if (!tempFile.is_open())
    {
        DebugLog("Config Save: Cannot open file");
        return;
    }

    tempFile
        << std::boolalpha // output bools as text
        << "<Options>" << '\n'
        << "ShowOnMain: " << _settingsValues.showOnMain << '\n'
        << "LockPosition: " << _settingsValues.lockPosition << '\n'
        << "BtnWidth: " << _settingsValues.btnWidth << '\n'
        << "BtnHeight: " <<_settingsValues.btnHeight << '\n'
        << "BtnLeft: " << _settingsValues.btnLeft << '\n'
        << "BtnTop: " << _settingsValues.btnTop << '\n'
        << "BtnFontSize: " << _settingsValues.btnFontSize << '\n'
        << "ShowInSquad: " << _settingsValues.showInSquad << '\n'
        << "EnableLogging: " << _settingsValues.enableLogging << '\n'
        << "LogSeverity: " << static_cast<int>(_settingsValues.logSeverity) << '\n'
        << "LogSpam: " << _settingsValues.logSpam << '\n'
        << "</Options>" << '\n';

    tempFile << "<Packages>" << '\n';
    for (auto it = _configPackages.begin(); it != _configPackages.end(); ++it)
    {
        tempFile << '"' << *it << '"' << '\n';
    }
    tempFile << "</Packages>";

    tempFile.close();

    _wremove(_configFilePath.c_str());
    _wrename(L"temp.txt", _configFilePath.c_str());
}

bool ModSettings::parseBoolSetting(const std::string &stringValue, bool &outValue)
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

bool ModSettings::parseFloatSetting(const std::string &stringValue, float &outValue)
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

bool ModSettings::parseLogSeveritySetting(const std::string& stringValue, Logger::Severity& outValue)
{
    if (stringValue.empty())
    {
        return false;
    }

    std::stringstream ss(stringValue);
    int tempOutValue = 0;
    ss >> tempOutValue;
    if (ss)
    {
        switch (tempOutValue)
        {
        case static_cast<int>(Logger::None):
        case static_cast<int>(Logger::Error):
        case static_cast<int>(Logger::Warning):
        case static_cast<int>(Logger::Info):
        case static_cast<int>(Logger::Debug):
            outValue = static_cast<Logger::Severity>(tempOutValue);
            return true;
        default:
            outValue = Logger::None;
            return false;
        }
    }
    return false;
}


ModSettings::SettingsValues::SettingsValues() :
    showOnMain(true),
    lockPosition(true),
    showInSquad(true),
    enableLogging(false),
    logSeverity(Logger::None),
    logSpam(false),
    btnWidth(1.50),
    btnHeight(3.58),
    btnLeft(0.69),
    btnTop(0.788),
    btnFontSize(12.0)
{
}
