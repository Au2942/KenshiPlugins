#pragma once

#include <kenshi/gui/DataPanelLine.h>
#include <kenshi/gui/OptionsWindow.h>
#include <mygui/MyGUI_Gui.h>

namespace SquadAutonomy
{
    class ModSettingsUI
    {
    public:
        static ModSettingsUI& getSingleton();
        ~ModSettingsUI();

        void create();
        void refresh();
        void show();
        void hide();
        bool isVisible();
        void resetSettingsToDefault(MyGUI::Widget*);
        void updateLogSeverity(MyGUI::ComboBox*, size_t);
        void updatePanel(DataPanelLine*);
        void saveSettings();
        void close(MyGUI::Window*, const std::string&);

        void setOptionsWindow(OptionsWindow*);

    private:
        // Make default constructor private to only allow creation as the singleton.
        ModSettingsUI();

        // Disable copy and move operations for the singleton by declaring them private.
        ModSettingsUI(const ModSettingsUI& other);
        ModSettingsUI(ModSettingsUI&& other);
        ModSettingsUI& operator=(const ModSettingsUI& other);
        ModSettingsUI& operator=(ModSettingsUI&& other);

        DataPanelLine_CheckBox* addCheckbox(const std::string &label, const std::string &tooltip, bool& value);
        DataPanelLine_SliderEditable* addSlider(const std::string &label, const std::string &tooltip, float& value, float min, float max, int precision);

        int _category;
        DatapanelGUI* _panel;
        OptionsWindow* _optionsWindow;
        int _logSeverityIndex;
    };
}
