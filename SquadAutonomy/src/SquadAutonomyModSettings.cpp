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
    if (modPath != L"")
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
    std::wifstream cfgFile(_configFilePath);
    if (!cfgFile.is_open())
    {
        DebugLog("Load: Cannot open config file");
        return;
    }
    DebugLog("Reading config file...");

    std::wstring wline;
    std::string token;
    while (std::getline(cfgFile, wline))
    {


        if (wline == L"<Options>")
        {
            while (std::getline(cfgFile, wline))
            {
                if (wline == L"</Options>")
                {
                    break;
                }

                wline.erase(0, wline.find_first_not_of(L" \t"));
                const auto colon = wline.find(L':');
                if (colon == std::string::npos)
                {
                    continue;
                }

                const std::wstring type = wline.substr(0, colon);
                const std::wstring dataLine = wline.substr(colon + 1);
                std::wstring dataLineW = wline.substr(colon + 1);
                //DebugLog(type);
                if (type == L"ShowOnMain")
                {
                    parseBoolSetting(dataLineW, _settingsValues.showOnMain);
                    continue;
                }
                if (type == L"LockPosition")
                {
                    parseBoolSetting(dataLineW, _settingsValues.lockPosition);
                    continue;
                }
                if (type == L"BtnWidth")
                {
                    parseFloatSetting(dataLineW, _settingsValues.btnWidth);
                    continue;
                }
                if (type == L"BtnHeight")
                {
                    parseFloatSetting(dataLineW, _settingsValues.btnHeight);
                    continue;
                }
                if (type == L"BtnLeft")
                {
                    parseFloatSetting(dataLineW, _settingsValues.btnLeft);
                    continue;
                }
                if (type == L"BtnTop")
                {
                    parseFloatSetting(dataLineW, _settingsValues.btnTop);
                    continue;
                }
                if (type == L"BtnFontSize")
                {
                    parseFloatSetting(dataLineW, _settingsValues.btnFontSize);
                    continue;
                }
                if (type == L"ShowInSquad")
                {
                    parseBoolSetting(dataLineW, _settingsValues.showInSquad);
                    continue;
                }
                if (type == L"EnableLogging")
                {
                    parseBoolSetting(dataLineW, _settingsValues.enableLogging);
                    continue;
                }
            }
        }
        else if (wline == L"<Packages>")
        {
            while (std::getline(cfgFile, wline))
            {
                // TODO: replace with wline
                std::string line = converter.to_bytes(wline);
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
    std::wofstream tempFile(L"temp.txt");
    if (!tempFile.is_open())
    {
        DebugLog("Config Save: Cannot open file");
        return;
    }

    tempFile
        << std::boolalpha // output bools as text
        << L"<Options>" << L'\n'
        << L"ShowOnMain: " << _settingsValues.showOnMain << L'\n'
        << L"LockPosition: " << _settingsValues.lockPosition << L'\n'
        << L"BtnWidth: " << _settingsValues.btnWidth << L'\n'
        << L"BtnHeight: " <<_settingsValues.btnHeight << L'\n'
        << L"BtnLeft: " << _settingsValues.btnLeft << L'\n'
        << L"BtnTop: " << _settingsValues.btnTop << L'\n'
        << L"BtnFontSize: " << _settingsValues.btnFontSize << L'\n'
        << L"ShowInSquad: " << _settingsValues.showInSquad << L'\n'
        << L"EnableLogging: " << _settingsValues.enableLogging << L'\n'
        << L"</Options>" << L'\n';

    tempFile << L"<Packages>" << L'\n';
    for (auto it = _configPackages.begin(); it != _configPackages.end(); ++it)
    {
        tempFile << L'"' << converter.from_bytes(*it) << L'"' << L'\n';
    }
    tempFile << L"</Packages>";

    tempFile.close();

    _wremove(_configFilePath.c_str());
    _wrename(L"temp.txt", _configFilePath.c_str());
}

bool ModSettings::parseBoolSetting(const std::wstring &stringValue, bool &outValue)
{
    std::wstring stringValueCopy = stringValue;
    stringValueCopy.erase(0, stringValueCopy.find_first_not_of(L" \t"));

    if (stringValueCopy == L"true" || stringValueCopy == L"false")
    {
        outValue = stringValueCopy == L"true";
        return true;
    }

    return false;
}

bool ModSettings::parseFloatSetting(const std::wstring &stringValue, float &outValue)
{
    if (stringValue.empty())
    {
        return false;
    }

    std::wstringstream ss(stringValue);
    float tempOutValue = 0;
    ss >> tempOutValue;
    if (ss)
    {
        outValue = tempOutValue;
        return true;
    }

    return false;
}


ModSettings::SettingsValues::SettingsValues() :
    showOnMain(true),
    lockPosition(true),
    showInSquad(true),
    enableLogging(false),
    btnWidth(1.50),
    btnHeight(3.58),
    btnLeft(0.69),
    btnTop(0.788),
    btnFontSize(12.0)
{
}
