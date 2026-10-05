#pragma once

#include <string>
#include <vector>

namespace SquadAutonomy
{
    class ModSettings
    {
    public:
        struct SettingsValues
        {
            SettingsValues();

            bool showOnMain;
            bool lockPosition;
            bool showInSquad;
            bool enableLogging;
            float btnWidth;
            float btnHeight;
            float btnLeft;
            float btnTop;
            float btnFontSize;
        };

        static ModSettings& getSingleton();

        const SettingsValues& getValues() const;
        SettingsValues& getValuesMutable();
        static SettingsValues& getDefaultValues();

        void resetToDefault();

        const std::vector<std::string>& getConfigPackages() const;

        void loadFromFile();
        void saveToFile() const;

    private:
        // Make default constructor private to only allow creation as the singleton.
        ModSettings();

        // Disable copy and move operations for the singleton by declaring them private.
        ModSettings(const ModSettings& other);
        ModSettings(ModSettings&& other);
        ModSettings& operator=(const ModSettings& other);
        ModSettings& operator=(ModSettings&& other);

        static bool parseBoolSetting(const std::wstring& stringValue, bool& outValue);
        static bool parseFloatSetting(const std::wstring& stringValue, float& outValue);

        SettingsValues _settingsValues;
        std::vector<std::string> _configPackages;

        std::wstring _configFileName;
        std::wstring _configFilePath;
    };
}
