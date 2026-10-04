#pragma once
#include <mygui/MyGUI.h>
#include <kenshi/gui/OptionsWindow.h>
#include <kenshi/gui/DatapanelGUI.h>

namespace MoreImmersiveBars
{
    class ModOptions
    {
    public:
        static ModOptions* getSingletonPtr();
        static bool initialized;
        ModOptions();
        ~ModOptions();

        void create();
        void refresh();
        void show();
        void hide();
        bool isVisible();
        void loadOptionsSettings();
        void saveOptionsSettings();
        void close(MyGUI::Window*, const std::string&);
        void setOptionsWindow(OptionsWindow*);

    private:
        int _category;
        DatapanelGUI* _panel;
        OptionsWindow* _optionsWindow;
    };
}