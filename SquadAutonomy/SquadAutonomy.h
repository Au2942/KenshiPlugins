#pragma once
#include <kenshi/GameData.h>
#include <kenshi/AI/AIPackage.h>
#include <kenshi/Platoon.h>
#include <kenshi/gui/SquadManagementScreen.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_Button.h>

namespace SquadAutonomy
{
    extern bool (*EscMenu_openedOtherWindows)(class EscMenu*);
    extern std::map<hand, float>* rentedBeds;
    extern bool (*Package_WanderingTrader_signalStart)(AIPackage*);
    extern std::string* _MainColorCode;
    extern bool showOnMain;
    extern bool useFloatingPanel;
    extern bool showInSquad;
    extern bool enableLogging;
    void Init();
    std::string GetCurrentDLLDirectory();
    extern void Log(std::string line);
    extern std::string logFileName;
    extern std::string logBakFileName;
    extern std::string saveName;
    extern std::string modPath;
    extern std::string logPath;
    extern std::string logBakPath;
    extern std::ofstream logFile;
    extern std::string settingsSavePath;
    //static int bc;
    //const int buffer;
    bool SetAI(Platoon*, std::map<int, lektor<GameData*>>);
    bool ResetAI(Platoon*);
    void OpenSquadAutonomyPanel(Platoon* platoon);
    void OpenSquadAutonomyPanelMainBar(MyGUI::Widget* sender);
    extern bool shouldSave;
    extern bool shouldLoad;
    extern bool loadNextCall;

    extern MyGUI::Button* autBtn;
    extern MyGUI::Window* autBtnWindow;

    extern MyGUI::IntPoint mDragStart;
    extern MyGUI::IntPoint mWindowStart;
    extern bool mDragging;
}