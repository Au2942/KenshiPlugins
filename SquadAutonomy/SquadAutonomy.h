#pragma once
#include <kenshi/GameData.h>
#include <kenshi/AI/AIPackage.h>
#include <kenshi/Platoon.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/SquadManagementScreen.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_Button.h>

namespace SquadAutonomy
{
    extern ogre_unordered_map<TaskType, TaskData*>::type* taskTypetaskData;
    extern const TaskData* (*getTaskDataConst)(TaskType key);
    extern bool (*EscMenu_openedOtherWindows)(class EscMenu*);
    extern std::map<hand, float>* rentedBeds;
    extern bool (*Package_WanderingTrader_signalStart)(AIPackage*);
    extern std::string* _MainColorCode;
    extern bool showOnMain;
    extern bool lockPosition;
    extern const float defaultBtnWidth;
    extern const float defaultBtnHeight;
    extern const float defaultBtnLeft;
    extern const float defaultBtnTop;
    extern const float defaultBtnFontSize;
    extern float btnWidth;
    extern float btnHeight;
    extern float btnLeft;
    extern float btnTop;
    extern float btnFontSize;
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
    bool SetAI(Platoon*, std::map<int, lektor<GameData*>>, bool endAction = true);
    bool ResetAI(Platoon*, bool endAction = true);
    void OpenSquadAutonomyPanel(Platoon* platoon);
    void OpenSquadAutonomyPanelMainBar();
    void RevertTaskDuration(TaskType);
    extern bool shouldSave;
    extern bool shouldLoad;
    extern bool loadNextCall;

    extern MyGUI::Button* autBtn;
    extern MyGUI::Window* autBtnWindow;

    extern MyGUI::IntPoint mDragStart;
    extern MyGUI::IntPoint mWindowStart;
    extern bool mClicked;
    extern bool mDragged;
}