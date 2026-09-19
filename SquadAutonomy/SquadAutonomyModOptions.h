#pragma once
#include <kenshi/gui/DataPanelLine.h>
#include <kenshi/gui/OptionsWindow.h>
#include <mygui/MyGUI_Gui.h>


namespace SquadAutonomy
{
    class SquadAutonomyModOptions
    {
    public:
        static SquadAutonomyModOptions* getSingletonPtr();
        static bool initialized;
        SquadAutonomyModOptions();
        ~SquadAutonomyModOptions();

        void create();
        void refresh();
        void show();
        void hide();
        bool isVisible();
        void resetSettingsToDefault(MyGUI::Widget*);
        void saveOptionsSettings();
        void close(MyGUI::Window*, const std::string&);

        /*bool showOnMain;
        bool lockPosition;
        float btnWidth;
        float btnHeight;
        float btnLeft;
        float btnTop;
        float btnFontSize;
        bool showInSquad;
        bool enableLogging;*/

        void setOptionsWindow(OptionsWindow*);

    private:
        int _category;
        DatapanelGUI* _panel;
        OptionsWindow* _optionsWindow;


    };
}