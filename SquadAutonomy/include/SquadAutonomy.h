#pragma once
#include <ogre/OgreStringConverter.h>
#include <kenshi/GameData.h>
#include <kenshi/AI/AIPackage.h>
#include <kenshi/Platoon.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/SquadManagementScreen.h>
#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_Button.h>
#include <codecvt>

namespace SquadAutonomy
{
    struct ShoppingItemInfo
    {
        ShoppingItemInfo(): quantity(0), onlyGoodDeal(false)
        {
        }
        ShoppingItemInfo(int _quantity) : quantity(_quantity), onlyGoodDeal(false)
        {
        }
        int quantity;
        bool onlyGoodDeal;
        std::vector<TownBase*> shopTowns;
    };

    extern ogre_unordered_map<TaskType, TaskData*>::type* taskTypetaskData;
    extern const TaskData* (*getTaskDataConst)(TaskType key);
    extern bool (*EscMenu_openedOtherWindows)(class EscMenu*);
    extern std::map<hand, float>* rentedBeds;
    extern bool (*Package_WanderingTrader_signalStart)(AIPackage*);
    extern std::string* _MainColorCode;
    extern std::string* _GreyedColorCode;
    std::wstring GetCurrentDLLDirectory();
    extern std::wstring saveName;
    extern std::wstring modPath;
    extern std::wstring settingsSavePath;
    extern std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    bool SetAI(Platoon*, const std::map<int, std::vector<GameData*>>& aiPackages, bool endAction = true);
    bool ResetAI(Platoon*, bool endAction = true);
    void SetNearestFriendlyTownAsHome(Platoon* squad);
    void ClearTask(Platoon*, TaskType);
    void OpenSquadAutonomyPanel(Platoon* platoon);
    void OpenSquadAutonomyPanelMainBar();
    void ShowModSettings(MyGUI::Widget* sender);
    void RevertTaskDuration(TaskType type);
    extern bool shouldSave;
    extern bool shouldLoad;
    extern bool loadNextCall;
    
    extern MyGUI::Window* autBtnWindow;
    typedef Ogre::StringConverter sc;
}
