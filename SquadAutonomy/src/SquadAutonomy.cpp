#include "SquadAutonomy.h"
#include "SquadAutonomyPanel.h"
#include "SquadAutonomySettings.h"
#include "SquadAutonomyModSettingsUI.h"
#include "SquadAutonomyButton.h"
#include "SquadAutonomyLocalization.h"
#include "SquadAutonomyModSettings.h"
#include "SquadAutonomyMainBarButton.h"
#include "SquadAutonomyLog.h"

#include <Debug.h>

#include <ogre/OgreStringConverter.h>
#include <ogre/OgreUTFString.h>
#include <boost/scoped_ptr.hpp>
#include <ogre/OgrePrerequisites.h>
#include <kenshi/util/OgreUnordered.h>

#include <kenshi/Kenshi.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/MainBarGUI.h>
#include <kenshi/gui/OptionsWindow.h>
#include <kenshi/gui/OrdersPanel.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <kenshi/gui/ToolTip.h>
#include <kenshi/gui/SquadManagementScreen.h>
#include <kenshi/gui/DialogueWindow.h>

#include <kenshi/util/hand.h>
#include <kenshi/util/PerfTimer.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/GameData.h>
#include <kenshi/RootObject.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Character.h>
#include <kenshi/CharMovement.h>
#include <kenshi/CharBody.h>
#include <kenshi/RaceData.h>
#include <kenshi/AI/AI.h>
#include <kenshi/AI/AITaskSystem.h>
#include <kenshi/AI/AIPackage.h>
#include <kenshi/AI/Blackboard.h>
#include <kenshi/Tasker.h>
#include <kenshi/Faction.h>
#include <kenshi/Inventory.h>
#include <kenshi/FactionRelations.h>
#include <kenshi/Platoon.h>
#include <kenshi/StateBroadcastData.h>
#include <kenshi/SaveManager.h>
#include <kenshi/SaveFileSystem.h>
#include <kenshi/SharedKing.h>
#include <kenshi/Town.h>
#include <kenshi/Building/Building.h>
#include <kenshi/Building/UseableStuff.h>
#include <kenshi/Building/StorageBuilding.h>
#include <kenshi/Building/FarmBuilding.h>
#include <kenshi/Building/ProductionBuilding.h>
#include <kenshi/Building/GatewayBuilding.h>
#include <kenshi/Building/DoorStuff.h>
#include <kenshi/Building/ResearchBuilding.h>
#include <kenshi/Research.h>

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Delegate.h>

#include <core/Functions.h>
#include <fstream>

#include <codecvt>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>



/*class TaskPathfinder 
{
public:
    class Node;
};
bool solve(std::deque<TaskPathfinder::Node*>& output, TaskType desiredTask, const hand& target, const Ogre::Vector3& loc, const hand& _debugHandle, bool playerOrder);// public RVA = 0x50F080
bool (*TaskPathfinder_Node_solve)(TaskPathfinder::Node*, std::deque<TaskPathfinder::Node*>& output, 
    TaskType desiredTask, const hand& target, const Ogre::Vector3 loc, const hand& _debugHandle, bool playerOrder) = nullptr;
TaskPathfinder::Node* createNode(TaskData* dat, const hand& subj, const Ogre::Vector3& loc, TaskPathfinder::Node* _creator);
TaskPathfinder::Node* (*TaskPathfinder_Node_createNode)(TaskPathfinder::Node* self, TaskData* dat, const hand& subj, const Ogre::Vector3& loc, TaskPathfinder::Node* _creator);
bool (*TaskRepertoire_hasTask)(TaskRepertoire* self, TaskType key) = nullptr;*/

namespace SquadAutonomy
{
    // functions/variables that aren't available in KenshiLib
    // we assign addresses for them in startPlugin
    ogre_unordered_map<TaskType, TaskData*>::type* taskTypetaskData = nullptr;
    const TaskData* (*getTaskDataConst)(TaskType key) = nullptr;
    std::map<hand, float>* rentedBeds = nullptr;
    std::string* _MainColorCode = nullptr;
    std::string* _GreyedColorCode = nullptr;
    bool (*EscMenu_openedOtherWindows)(class EscMenu*) = nullptr;
    bool (*Package_WanderingTrader_signalStart)(AIPackage*) = nullptr;
    bool (*CharBody_NV_setCurrentAction)(CharBody*, Tasker*) = nullptr;
    Building* (*Task_ManTheGate_FindGate)(Tasker*, Character*, hand) = nullptr;
    void (*Task_ManTheGate_Update)(Tasker*, CharBody*) = nullptr;
    void (*CharMovement_NV_setDestination)(CharBody*, Ogre::Vector3, UpdatePriority, bool) = nullptr;
    void (*Task_MoveToDoor_Gate_ChooseSide_gatePosition)(Tasker*, Ogre::Vector3&, CharBody*) = nullptr;
    void (*Task_OpenDoor_StartAction)(Tasker*, CharBody*) = nullptr;
    void (*Task_OpenDoor_Update)(Tasker*, CharBody*) = nullptr;
    Tasker* (*GOAPTaskMgr_createNewTask)(TaskRepertoire*, TaskType key, const hand& subject, taskPriority priority, float weight, Ogre::Vector3 location, int startTime, int endTime) = nullptr;

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    std::wstring saveName = L"SquadAutonomy.save";
    std::wstring modPath = L"";
    std::wstring settingsSavePath = L"";

    bool shouldSave = false;
    bool shouldLoad = false;
    bool loadNextCall = false;

    // for use in closeGate and stayInsideGate logic
    Building* destGate = nullptr;
    float closeGateCD = 1.0;
    float closeGateCDTimer = 0;
    bool closeGateTimerOn = false;

    // for changing a TaskDatas durations
    // TaskDatas are initialised in GOAPTaskMgr::setupGOAP() iirc
    // usually when the game create a new Tasker, it'll use TaskData from a static ogre_unordered_map<TaskType, TaskData*>::type taskData
    // which means that changing the duration of a taskData = changing it for whatever uses the same taskData in the future
    // so we have to revert it back once the game is done setting up the tasker with our custom duration data
    class OriginalTaskDataDuration
    {
    public:
        float durationMin;
        float durationFuzz;
        bool isDurationBased;
        bool endsAfterTime;
        OriginalTaskDataDuration(float min, float fuzz, bool based, bool ends)
        {
            durationMin = min;
            durationFuzz = fuzz;
            isDurationBased = based;
            endsAfterTime = ends;
        }
    };
    std::unordered_map<TaskType, OriginalTaskDataDuration*> taskTypeOrigDataDuration;

    std::wstring GetCurrentDLLDirectory() {
        wchar_t path[MAX_PATH];
        HMODULE hModule = NULL;

        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&GetCurrentDLLDirectory, &hModule)) {

            GetModuleFileNameW(hModule, path, MAX_PATH);
            std::wstring fullPath(path);

            size_t lastSlash = fullPath.find_last_of(L"\\/");
            if (std::wstring::npos != lastSlash) {
                return fullPath.substr(0, lastSlash + 1);
            }
            return fullPath;
        }
        return L"";
    }
    
    bool SetAI(Platoon* platoon, const std::map<int, std::vector<GameData*>>& aiPackages, bool endAction)
    {
        //DebugLog("Set AI");
        if (!platoon || !platoon->activePlatoon)
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
            return false;
        }
        for (auto iter = platoon->activePlatoon->things.begin(); iter != platoon->activePlatoon->things.end(); ++iter)
        {
            auto obj = reinterpret_cast<Character*>(*iter);
            //obj->getMovement()->halt();
            obj->clearAllAIGoals();
            //obj->ai->resultsCache.resetAllCaches();
            if (endAction && obj->getBody())
            {
                CharBody* body = obj->getBody();
                StateBroadcastData* stateBroadcast = obj->getStateBroadcast();
                if (stateBroadcast && (stateBroadcast->isSleeping || stateBroadcast->isSitting))
                { }
                else body->endAction();
            }
        }
        Blackboard* bb = platoon->getBlackboard();
        bb->clearAllPackages();
        /*if (aiPackages.size() < 1)
        {
            //ou->showPlayerAMessage("No AI packages to enable", true);
            return true;
        }*/
        for (auto pack = aiPackages.begin(); pack != aiPackages.end(); ++pack)
        {
            auto data = pack->second;
            for (int i = 0; i < data.size(); ++i)
            {
                bb->_addPackage(data[i], pack->first);
                //DebugLog("Adding " + data[i]->name + " to " + platoon->activePlatoon->getName());
            }
        }
        return true;
    }
    bool ResetAI(Platoon* platoon, bool endAction)
    {
        //DebugLog("Reset AI");
        if (!platoon || !platoon->activePlatoon)
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
            return false;
        }

        Blackboard* bb = platoon->getBlackboard();
        bb->clearAllPackages();
        bb->addFallbackPackages(platoon->squadTemplate);
        bb->replaceAIPackage(platoon->squadTemplate);
        for (auto iter = platoon->activePlatoon->things.begin(); iter != platoon->activePlatoon->things.end(); ++iter)
        {
            auto obj = reinterpret_cast<Character*>(*iter);
            //obj->getMovement()->halt();
            obj->clearAllAIGoals();
            //obj->ai->resultsCache.resetAllCaches();
            if (endAction && obj->getBody())
            {
                CharBody* body = obj->getBody();
                StateBroadcastData* stateBroadcast = obj->getStateBroadcast();
                if (stateBroadcast && (stateBroadcast->isSleeping || stateBroadcast->isSitting))
                {}
                else body->endAction();
            }
        }
        return true;
    }

    void ClearTask(Platoon* platoon, TaskType key)
    {
        if (!platoon || !platoon->activePlatoon)
        {
            return;
        }
        for (auto iter = platoon->activePlatoon->things.begin(); iter != platoon->activePlatoon->things.end(); ++iter)
        {
            auto obj = reinterpret_cast<Character*>(*iter);
            AI* ai = obj->ai;
            AITaskSytem* taskSystem = nullptr;
            if(ai) taskSystem = ai->getTaskSystem();
            Tasker* currentGoal = nullptr;
            if (taskSystem)
            {
                currentGoal = taskSystem->tryToGetCurrentGoal();
                if (currentGoal && currentGoal->key() == key)
                {
                    CharMovement* movement = obj->getMovement();
                    if (movement)
                    {
                        movement->setDestination(currentGoal->getLocation(), HIGH_PRIORITY, true);
                    }
                }
            }
        }
    }

    void OpenSquadAutonomyPanel(Platoon* platoon)
    {
        if (platoon) 
        {
            SquadAutonomyPanel::getSingletonPtr()->selectSquad(platoon);
            if (SquadAutonomyPanel::getSingletonPtr()->isVisible())
            {
                SquadAutonomyPanel::getSingletonPtr()->hide();
            }
            else
            {
                SquadAutonomyPanel::getSingletonPtr()->show();
            }
        }
    }
    void OpenSquadAutonomyPanelMainBar()
    {
        Platoon* platoon = ou->player->getCurrentPlatoon();
        if (platoon)
        {
            SquadAutonomyPanel::getSingletonPtr()->selectSquad(platoon);
            if (SquadAutonomyPanel::getSingletonPtr()->isVisible())
            {
                SquadAutonomyPanel::getSingletonPtr()->hide();
            }
            else
            {
                SquadAutonomyPanel::getSingletonPtr()->show();
            }
        }
    }

    void (*ForgottenGUI_changeFontSize_orig)();
    void ForgottenGUI_changeFontSize_hook()
    {
        ForgottenGUI_changeFontSize_orig();
        if (SquadAutonomyPanel::initialized)
            SquadAutonomyPanel::getSingletonPtr()->create();

        ModSettingsUI::getSingleton().create();
    }

    void ShowModSettings(MyGUI::Widget* sender)
    {
        auto& modSettingsUI = ModSettingsUI::getSingleton();
        if (modSettingsUI.isVisible())
        {
            modSettingsUI.hide();
        }
        else
        {
            modSettingsUI.show();
        }
    }

    void (*OptionsWindow_create_orig)(OptionsWindow* thisptr);
    void OptionsWindow_create_hook(OptionsWindow* thisptr)
    {
        OptionsWindow_create_orig(thisptr);
        if (!thisptr) return;
        ModSettingsUI::getSingleton().setOptionsWindow(thisptr);

        Logger::log("Create settings Button in Options", Logger::Info, false);
        auto tabCount = thisptr->tabs->getItemCount();
        std::vector<int> catList(tabCount);
        int maxCat = 0;
        int cat = 0x0;
        DataPanelLine* modLine = nullptr;
        DatapanelGUI* settingsPanel = nullptr;
        for (size_t i = 0; i < tabCount; i++)
        {
            auto panel = *(thisptr->tabs->getItemDataAt<DatapanelGUI*>(i, false));
            //From KEP: mod category = 0x0
            if (panel && panel->getCurrentCategory() == cat)
            {
                //cat = panel->getCurrentCategory();
                settingsPanel = panel;
                break;
            }
        }
        if (settingsPanel)
        {
            //DebugLog(Ogre::StringConverter::toString(settingsPanel->getNumLines(cat)));
            std::string identifier = "SquadAutonomy";
            for (int i = 0; i < settingsPanel->getNumLines(cat); ++i)
            {
                auto line = settingsPanel->getLineByNum(cat, i);
                if (line)
                {
                    std::string key = line->keyValue;
                    int slash = -1;
                    std::string modName = "";
                    key.erase(0, key.find_first_not_of(" \t"));
                    slash = key.find('-');
                    if (slash == std::string::npos)
                    {
                        continue;
                    }

                    modName = key.substr(slash + 1);
                    modName.erase(0, modName.find_first_not_of(" \t"));
                    //DebugLog(modName);
                    if (modName == identifier)
                    {
                        modLine = line;
                        break;
                    }
                }
            }
            if (modLine && modLine->w2)
            {
                //DebugLog(Ogre::StringConverter::toString(modLine->getNumWidgets()));
                //DebugLog(modLine->w1->getCaption().asUTF8());
                //DebugLog(modLine->w2->getCaption().asUTF8());
                if (settingsPanel->getWidget()->getSize().height > 0 && settingsPanel->getWidget()->getSize().width > 0)
                {
                    float right = static_cast<float>(modLine->w2->getRight() - modLine->w2->getTextSize().width) / settingsPanel->getWidget()->getSize().width;
                    float left = right - 0.1;
                    float top = static_cast<float>(modLine->w2->getTop()) / settingsPanel->getWidget()->getSize().height;
                    float height = static_cast<float>(modLine->w2->getHeight()) / settingsPanel->getWidget()->getSize().height;
                    float extendedHeight = height * 1.5;
                    //DebugLog("left: " + Ogre::StringConverter::toString(left) + " top: " + Ogre::StringConverter::toString(top) + " height " + Ogre::StringConverter::toString(height));
                    auto btn = settingsPanel->getWidget()->createWidgetReal<MyGUI::Button>("Kenshi_Button1", left, top - (extendedHeight - height) * 0.5, 0.1, extendedHeight, MyGUI::Align::Top | MyGUI::Align::Left, "SquadAutonomySettingsBtn");
                    btn->setCaption(Localization::gettext("Settings"));
                    btn->eventMouseButtonClick += MyGUI::newDelegate(ShowModSettings);
                }
            }
        }
    }

    void (*saveOptions_orig)(OptionsWindow* thisptr);
    void saveOptions_hook(OptionsWindow* thisptr)
    {
        saveOptions_orig(thisptr);
        if (!thisptr) return;
        ModSettingsUI::getSingleton().saveSettings();
    }

    void (*closeButton_orig)(OptionsWindow* thisptr, MyGUI::Widget* _sender);
    void closeButton_hook(OptionsWindow* thisptr, MyGUI::Widget* _sender)
    {
        auto& modSettingsUI = ModSettingsUI::getSingleton();
        const bool modSettingsVisible = modSettingsUI.isVisible();

        closeButton_orig(thisptr, _sender);
        if (!thisptr) return;
        if (modSettingsVisible && !modSettingsUI.isVisible())
        {
            modSettingsUI.show();
        }
    }

    std::vector<std::unique_ptr<SquadAutonomyButton>> squadAutButtons;

    MainBarGUI* (*MainbarGUICONSTRUCTOR_orig)(MainBarGUI* thisptr);
    MainBarGUI* MainbarGUICONSTRUCTOR_hook(MainBarGUI* thisptr)
    {
        MainBarGUI* orig = MainbarGUICONSTRUCTOR_orig(thisptr);

        if (orig->getWidget())
        {
            MainBarButton::getSingleton().createButton(*orig->getWidget());
        }

        return orig;
    }

    void (*MainbarGUIDESTRUCTOR_orig)(MainBarGUI* thisptr);
    void MainbarGUIDESTRUCTOR_hook(MainBarGUI* thisptr)
    {
        if (thisptr->getWidget())
        {
            MainBarButton::getSingleton().destroyButton(*thisptr->getWidget());
        }

        MainbarGUIDESTRUCTOR_orig(thisptr);
    }

    void (*_NV_update_orig)(MainBarGUI* thisptr);
    void _NV_update_hook(MainBarGUI* thisptr)
    {
        _NV_update_orig(thisptr);

        MainBarButton::getSingleton().updateButton();
    }

    void (*_NV_autoChangeSelectedObject_orig)(MainBarGUI* thisptr, const hand& obj);
    void _NV_autoChangeSelectedObject_hook(MainBarGUI* thisptr, const hand& obj)
    {
        _NV_autoChangeSelectedObject_orig(thisptr, obj);
        if (SquadAutonomyPanel::getSingletonPtr()->isVisible())
        {
            Platoon* platoon = ou->player->getCurrentPlatoon();
            if (platoon)
            {
                SquadAutonomyPanel::getSingletonPtr()->selectSquad(platoon);
                SquadAutonomyPanel::getSingletonPtr()->refresh();
                SquadAutonomyPanel::getSingletonPtr()->getOptionsTab()->refresh();
            }
        }
    }

    void (*tabPlatoonChange_orig)(MainBarGUI* thisptr, MyGUI::TabControl* _sender, unsigned __int64 _index);
    void tabPlatoonChange_hook(MainBarGUI* thisptr, MyGUI::TabControl* _sender, unsigned __int64 _index)
    {
        tabPlatoonChange_orig(thisptr, _sender, _index);

        if (SquadAutonomyPanel::getSingletonPtr()->isVisible())
        {
            Platoon* platoon = ou->player->getCurrentPlatoon();
            if (platoon)
            {
                SquadAutonomyPanel::getSingletonPtr()->selectSquad(platoon);
                SquadAutonomyPanel::getSingletonPtr()->refresh();
                SquadAutonomyPanel::getSingletonPtr()->getOptionsTab()->refresh();
            }
        }
    }

    void (*cycleSquad_orig)(PlayerInterface* thisptr);
    void cycleSquad_hook(PlayerInterface* thisptr)
    {
        // your code before/instead of the original
        cycleSquad_orig(thisptr);
        if (SquadAutonomyPanel::getSingletonPtr()->isVisible())
        {
            Platoon* platoon = ou->player->getCurrentPlatoon();
            if (platoon)
            {
                SquadAutonomyPanel::getSingletonPtr()->selectSquad(platoon);
                SquadAutonomyPanel::getSingletonPtr()->refresh();
                SquadAutonomyPanel::getSingletonPtr()->getOptionsTab()->refresh();
            }
        }
    }
    
    void (*SquadCellView_update_orig)(SquadManagementScreen::SquadCellView* thisptr, const MyGUI::IBDrawItemInfo& _info, SquadManagementScreen::SquadData* _data);
    void SquadCellView_update_hook(SquadManagementScreen::SquadCellView* thisptr, const MyGUI::IBDrawItemInfo& _info, SquadManagementScreen::SquadData* _data)
    {
        SquadCellView_update_orig(thisptr, _info, _data);

        const auto& settingsValues = ModSettings::getSingleton().getValues();

        bool createNew = true;
        for (int i = 0; i < squadAutButtons.size(); ++i)
        {
            if (squadAutButtons[i]->getSquadCellView() == thisptr)
            {
                createNew = false;
            }

            squadAutButtons[i]->setVisible(settingsValues.showInSquad);
        }

        if(createNew)
        {
            squadAutButtons.emplace_back(new SquadAutonomyButton(*thisptr));
            squadAutButtons.back()->setVisible(settingsValues.showInSquad);
        }
    }


    void (*removeSquad_orig)(SquadManagementScreen* thisptr, SquadManagementScreen::SquadData* squad);
    void removeSquad_hook(SquadManagementScreen* thisptr, SquadManagementScreen::SquadData* squad)
    {
        auto& settings = SquadAutonomySettings::getSingleton();
        Platoon* platoon = nullptr;
        if (squad->platoon) platoon = squad->platoon->me;
        if (platoon)
        {
            settings.removeSquadSettings(platoon);
        }
        removeSquad_orig(thisptr, squad);
    }

    bool (*EscMenu_openedOtherWindows_orig)(void*);
    bool EscMenu_openedOtherWindows_hook(void* self)
    {
        auto out = EscMenu_openedOtherWindows_orig(self);
        if (out)
        {
            SquadAutonomyPanel::getSingletonPtr()->hide();
        }
        else
        {
            auto settingsPanel = SquadAutonomyPanel::getSingletonPtr();
            if (settingsPanel->isVisible())
            {
                settingsPanel->hide();
                out = true;
            }
        }
        return out;
    }

    int (*saveGame_orig)(SaveManager* thisptr, const std::string& location, const std::string& name);
    int saveGame_hook(SaveManager* thisptr, const std::string& location, const std::string& name)
    {
        auto& settings = SquadAutonomySettings::getSingleton();
        for (int i = 0; i < settings.squadSettings.size(); ++i)
        {
            SquadSettingsInfo* squadSettings = settings.squadSettings[i].get();
            if (!squadSettings->isEnabled()) continue;
            ResetAI(squadSettings->getSquad());
            squadSettings->unassignSquadHome();
        }
        shouldSave = true;
        settingsSavePath = converter.from_bytes(location) + converter.from_bytes(name) + L'/' + saveName;
        
        int result = saveGame_orig(thisptr, location, name);

        return result;
    }

    int (*loadGame_orig)(SaveManager* thisptr, const std::string& location, const std::string& name);
    int loadGame_hook(SaveManager* thisptr, const std::string& location, const std::string& name)
    {
        int result = loadGame_orig(thisptr, location, name);
        if (result != 0) return result;
        shouldLoad = true;
        SquadAutonomySettings::getSingleton().initialized = false;
        settingsSavePath = converter.from_bytes(location) + converter.from_bytes(name) + L'/' + saveName;
        return result;
    }

    void (*execute_orig)(SaveManager* thisptr);
    void execute_hook(SaveManager* thisptr)
    {
        execute_orig(thisptr);
        if (shouldSave)
        {
            auto& settings = SquadAutonomySettings::getSingleton();
            if (settings.saveSettings(settingsSavePath))
            {
                SquadAutonomySettings::getSingleton().loadSettings(settingsSavePath);
                shouldSave = false;
            }
        }
        else if (shouldLoad)
        {
            if (!ou->isLoadingFromASaveGame())
            {
                SquadAutonomySettings::getSingleton().loadSettings(settingsSavePath);
                SquadAutonomySettings::getSingleton().initialized = true;
                shouldLoad = false;
            }
        }
    }


    void SetNearestFriendlyTownAsHome(Platoon* squad)
    {
        if (!squad) return;
        /*float score = std::numeric_limits<float>::max();
        auto factions = *ou->factionMgr->getAllFactions();*/
        Character* leader = squad->getSquadLeader();
        TownBase* currentTown = nullptr;
        Blackboard* bb = squad->getBlackboard();
        ActivePlatoon* activeSquad = squad->activePlatoon;
        if (bb)
        {
            currentTown = bb->getCurrentTownLocation();
        }
        if (currentTown)
        {
            Building* home = nullptr;
            if (leader && leader->isInsideBuilding)
            {
                Building* building = leader->isInsideBuilding.getBuilding();
                Faction* buildingFaction = nullptr;
                Faction* myFaction = leader->getFaction();
                if (building)
                {
                    buildingFaction = building->getFaction();
                    if (buildingFaction && myFaction)
                    {
                        if (myFaction == buildingFaction || (buildingFaction->relations && buildingFaction->relations->isAlly(myFaction)))
                        {
                            home = building;
                        }
                    }
                }
            }
            //if (home) DebugLog("set home to: " + home->displayName);
            //else DebugLog("no home");
            squad->getOwnerships()->setHomeBuilding(home, squad->getSquadType());
            squad->getOwnerships()->setHomeTown(currentTown, squad->getSquadType());
            if (activeSquad)
            {
                for (auto it = activeSquad->things.begin(); it != activeSquad->things.end(); ++it)
                {
                    Character* obj = reinterpret_cast<Character*>(*it);
                    StateBroadcastData* state = obj->getStateBroadcast();
                    if (state) state->homeBuilding = home;
                }
            }
            //DebugLog("set town to: " + currentTown->getKnownName());
            return;
        }
        else
        {
            //DebugLog("no town");
            squad->getOwnerships()->setHomeBuilding(nullptr, squad->getSquadType());
            squad->getOwnerships()->setHomeTown(nullptr, squad->getSquadType());
            if (activeSquad)
            {
                for (auto it = activeSquad->things.begin(); it != activeSquad->things.end(); ++it)
                {
                    Character* obj = reinterpret_cast<Character*>(*it);
                    StateBroadcastData* state = obj->getStateBroadcast();
                    if (state) state->homeBuilding = nullptr;
                }
            }
        }
    }


    UseableStuff* FindOptimalBed(Character* character, AI* ai, bool usePaidBeds, const std::vector<UseableStuff*> beds, bool ownFactionOnly = false)
    {
        float minDist = std::numeric_limits<float>::max();
        float maxEfficiency = std::numeric_limits<float>::lowest();
        float minCost = std::numeric_limits<float>::max();
        UseableStuff* optimalBed = nullptr;
        Faction* ownFaction = character->getFaction();
        AITaskSytem* taskSystem = ai->getTaskSystem();
        if (!taskSystem) return nullptr;
        for (uint32_t i = 0; i < beds.size(); ++i)
        {
            UseableStuff* b = beds[i];
            if (taskSystem->isPathImpossible(b)) continue;
            if (b->getBuildState() == 0) continue;

            if (b->getOccupant())
            {
                if (b->getOccupant() == character->getHandle())
                {
                    return b;
                }
                continue;
            }
            if (ownFactionOnly)
            {
                Faction* faction = b->getFaction();
                if (ownFaction && faction)
                {
                    if (faction != ownFaction)
                    {
                        continue;
                    }
                }
            }
            if (!b->data) continue;

            float efficiency = -1;
            auto it = b->data->fdata.find("output rate");
            if (b->data->fdata.size() > 0 && it != b->data->fdata.end())
                efficiency = it->second;

            int cost = 0;
            cost = b->getCostToUse(character);

            //DebugLog("rentedBeds : " + Ogre::StringConverter::toString((*rentedBeds).size()));
            if (rentedBeds)
            {
                for (auto it = (*rentedBeds).begin(); it != (*rentedBeds).end(); ++it)
                {
                    UseableStuff* rentedBed = nullptr;
                    Building* building = nullptr;
                    if (it->first) building = it->first.getBuilding();
                    if (building) rentedBed = building->getUseableStuff();
                    if (rentedBed)
                    {
                        if (rentedBed == b)
                        {
                            //TODO: check time as well
                            //DebugLog("Bed already rented!");
                            float currentTime = ou->getTimeStamp_inGameHours().getTotalHours();
                            //DebugLog("Time now: " + Ogre::StringConverter::toString(currentTime) + " rented time: " + Ogre::StringConverter::toString(it->second));
                            if (currentTime - it->second < 24.0)
                            {
                                //DebugLog("Bed was rented!");
                                cost = 0;
                                break;
                            }
                        }
                    }
                }
            }

            if (cost == -1 || (!usePaidBeds && cost > 0)) continue;

            float distanceScore = b->getPosition().squaredDistance(character->getPosition());

            bool better = false;
            if (efficiency > maxEfficiency) {
                better = true;
            }
            else if (efficiency == maxEfficiency) {
                if (cost < minCost) {
                    better = true;
                }
                else if (cost == minCost && distanceScore < minDist) {
                    better = true;
                }
            }

            if (better) {
                optimalBed = b;
                maxEfficiency = efficiency;
                minCost = cost;
                minDist = distanceScore;
            }
        }
        if (optimalBed)
        {
            if (optimalBed->getOccupant()) return nullptr;
            else return optimalBed;
        }
        return nullptr;
    }

    hand FindOptimalBed(Character* character, bool usePaidBeds)
    {
        if (!character) return nullptr;
        AI* ai = character->ai;
        if (!ai) return nullptr;
        UseableStuff* optimalBed = nullptr;

        RaceData* race = character->getRace();
        std::vector<UseableStuff*> beds;

        TownBase* currentTown = character->getCurrentTownLocation();
        if (currentTown)
        {
            // TODO: Prioritize homebuilding
            BuildingFunction bf = BF_BED;
            if (race && race->robot)
            {
                bf = BF_SKELETON_BED;
            }
            lektor<Building*>* bedBuildings = currentTown->findAllBuildingsWithFunction(bf, character);
            if (!bedBuildings) return nullptr;
            for (int i = 0; i < bedBuildings->size(); ++i)
            {
                if (!bedBuildings->at(i)) continue;
                UseableStuff* bed = bedBuildings->at(i)->getUseableStuff();
                if (bed) beds.push_back(bed);
            }
            optimalBed = FindOptimalBed(character, ai, false, beds, true);
            if (optimalBed) return optimalBed->getHandle();
            optimalBed = FindOptimalBed(character, ai, usePaidBeds, beds);
        }
        if (optimalBed) return optimalBed->getHandle();
        else return nullptr;

    }

    StorageBuilding* FindResourceStorageBuildingFor(AI* ai, GameData* needed, StorageBuilding* skip)
    {
        lektor<Building*> buildings;
        ai->getAllLocalMachines(buildings, 67118134); //buildingfunction bitmask (I think?) copied from AI::findResourceStorageBulidingFor
        AITaskSytem* taskSystem = ai->getTaskSystem();
        float minDist = std::numeric_limits<float>::max();
        StorageBuilding* best = nullptr;
        for (int i = 0; i < buildings.size(); ++i)
        {
            StorageBuilding* storage = buildings[i]->getFunctionStuff();
            if (!storage) continue;
            if (storage == skip) continue;
            if (!storage->hasItem(needed)) continue;

            ProductionBuilding* production = storage->getProductionBuilding();
            if (production)
            {
                bool consumeItem = false;
                for (int j = 0; j < production->getNumConsumtionItems(); ++j)
                {
                    if (production->getConsumtionItems(j)->item == needed)
                    {
                        consumeItem = true;
                        break;
                    }
                }
                if (consumeItem) continue;
            }
            if (taskSystem && taskSystem->isPathImpossible(storage)) continue;
            float dist = ai->getPosition().squaredDistance(storage->getPosition());
            if (dist < minDist)
            {
                minDist = dist;
                best = storage;
            }
        }
        return best;
    }

    Character* NearestSquadMemberHaul(Character* character, Building* to, Building* from)
    {
        if (!character || !to || !from) return nullptr;
        ActivePlatoon* squad = character->getPlatoon();
        Character* nearest = nullptr;
        float minDist = std::numeric_limits<float>::max();
        for (auto it = squad->things.begin(); it != squad->things.end(); ++it)
        {
            Character* obj = reinterpret_cast<Character*>(*it);
            CharBody* body = obj->getBody();
            AI* ai = obj->getAI();
            if (body)
            {
                Tasker* currentAction = body->getCurrentAction();
                if (currentAction && currentAction->key() == OPERATE_STORAGE)
                {
                    if (currentAction->subject == to->getHandle())
                    {
                        Logger::log(obj->displayName + " is already doing " + to->displayName, Logger::Debug, false);
                        return nullptr;
                    }
                }
            }
            if (!ai) continue;
            AITaskSytem* taskSystem = ai->getTaskSystem();
            if (!taskSystem) continue;
            if (from && taskSystem->isPathImpossible(from)) continue;

            float dist = obj->pos.squaredDistance(from->pos);
            if (dist < minDist)
            {
                nearest = obj;
                minDist = dist;
            }
        }
        if (nearest && nearest->getOrdersReciever())
        {
            Logger::log(nearest->displayName + " nearest to " + to->displayName, Logger::Debug, false);
            nearest->getOrdersReciever()->addOrder(OPERATE_STORAGE, to, from->getPosition(), true, false);
        }
        return nearest;
    }

    bool NoOneElseIsHauling(Character* character, Building* to)
    {
        if (!character || !to) return false;
        ActivePlatoon* squad = character->getPlatoon();
        for (auto it = squad->things.begin(); it != squad->things.end(); ++it)
        {
            Character* obj = reinterpret_cast<Character*>(*it);
            if (character == obj) continue;
            CharBody* body = obj->getBody();
            AI* ai = obj->getAI();
            if (body)
            {
                Tasker* currentAction = body->getCurrentAction();
                if (currentAction && currentAction->key() == OPERATE_STORAGE)
                {
                    if (currentAction->subject == to->getHandle())
                    {
                        Logger::log(obj->displayName + " is already doing " + to->displayName, Logger::Debug, false);
                        return false;
                    }
                }
            }
        }
        return true;
    }

    UseableStuff* FindOptimalLabourFromList(Character* character, AI* ai, std::unordered_map<UseableStuff*,int>* buildings, SquadSettingsInfo* settings)
    {
        if (!character || !ai) return nullptr;
        UseableStuff* optimalLabour = nullptr;
        float minDist = std::numeric_limits<float>::max();
        int minOperators = std::numeric_limits<int>::max();
        int minPriority = 0;
        AITaskSytem* taskSystem = ai->getTaskSystem();
        if (!taskSystem) return nullptr;
        //bool prioritizeNotFull = false;
        Logger::log("Find optimal Labour from list", Logger::Severity::Info, true);
        for (auto it = buildings->begin(); it != buildings->end(); ++it)
        {
            //DebugLog(it->first->displayName);
            auto useable = it->first;
            int operators = it->second;
            float dist = useable->getPosition().squaredDistance(character->getPosition());
            ProductionBuilding* production = useable->getProductionBuilding();
            if (taskSystem->isPathImpossible(useable)) continue;
            if (useable->getBuildState() == 0) continue;
            if (settings->getLabourPriority(useable) < minPriority) continue;
            if (production)
            {
                if (production->isProductionFull())
                {
                    //no storage
                    StorageBuilding* storage = FindResourceStorageBuildingFor(ai, production->getProductionItemData(), production);
                    if (storage)
                    {
                        Logger::log(character->displayName + " -> " + production->displayName + " production full. Can haul to : " + storage->displayName, Logger::Debug, false);
                        auto nearest = NearestSquadMemberHaul(character, storage, production);
                        if (nearest == character)
                        {
                            return nullptr;
                        }
                    }
                    continue;
                }
                lektor<GameData*> out;
                production->getResourcesNeededBecauseEmpty(out);
                if (out.size() == 0)
                {
                    if (production->isProductionEmpty())
                    {
                        continue;
                    }
                    StorageBuilding* storage = FindResourceStorageBuildingFor(ai, production->getProductionItemData(), production);
                    if (!storage) continue; 
                }
                else
                {
                    Logger::log(production->displayName + " has empty input resource", Logger::Debug, false);
                    for (int i = 0; i < out.size(); ++i)
                    {
                        StorageBuilding* storage = FindResourceStorageBuildingFor(ai, out[i], production);
                        if (storage)
                        {
                            Logger::log(production->displayName + " has input resource: " + storage->displayName, Logger::Debug, false);
                            auto nearest = NearestSquadMemberHaul(character, production, storage);
                            if (nearest == character)
                            {
                                return nullptr;
                            }
                        }
                    }
                    continue;
                    
                }
                
                    /*FarmBuilding* farmBuilding = dynamic_cast<FarmBuilding*>(production);
                    if (farmBuilding)
                    {
                        if (farmBuilding->grown < 1.0)
                        {
                            if (farmBuilding->consumptionRate > 0.0)
                            {
                                continue;
                            }
                            if (farmBuilding->isProductionEmpty())
                            {
                                continue;
                            }
                            else
                            {
                                StorageBuilding* storage = ai->findResourceStorageBulidingFor(farmBuilding->getProductionItemData(), farmBuilding);
                                if (storage && storage->canHaveSomeOfThese(production->getProductionItemData()))
                                {
                                }
                                else continue;
                            }
                        }
                    }
                    */
                   
            }
            Logger::log(useable->displayName + ", " + Ogre::StringConverter::toString(operators) + ", " + Ogre::StringConverter::toString(dist));
            if (useable->numOperatorsMax <= 0)
            {
                float scoreAutoMachine = ai->scoreAutoMachinery(production, production->getPosition());
                Logger::log(character->displayName + " -> " + production->displayName + " Automachine Score: " + Ogre::StringConverter::toString(scoreAutoMachine), Logger::Debug, false);
                if (scoreAutoMachine <= 0) continue;

            }
            else if (operators < minOperators)
            {
                optimalLabour = useable;
                minOperators = operators;
                minDist = dist;
                minPriority = settings->getLabourPriority(useable);
                continue;
            }
            else if (operators > minOperators) continue; 

            if (minDist < dist)
            {
                continue;
            }
            optimalLabour = useable;
            minOperators = operators;
            minDist = dist;
            minPriority = settings->getLabourPriority(useable);
        }
        if (optimalLabour) Logger::log(character->displayName + " found labour : " + optimalLabour->displayName + ", " + Ogre::StringConverter::toString(minOperators) + ", " + Ogre::StringConverter::toString(minDist), Logger::Info, false);
        return optimalLabour;
    }

    hand FindLabourToDo(Character* character, SquadSettingsInfo* settings)
    {
        if (!character) return nullptr;
        AI* ai = character->getAI();
        if (!ai) return nullptr;
        hand optimalLabour = nullptr;
        std::unordered_map<UseableStuff*, int> labourCandidates;
        TownBase* currentTown = character->getCurrentTownLocation();
        Logger::log(character->displayName + " find Labour To Do", Logger::Info, true);
        if (currentTown)
        {
            if (ou->player && ou->player->technology && ou->player->technology->current.size() > 0)
            {
                lektor<Building*>* researchBuildings = currentTown->findAllBuildingsWithFunction(BF_RESEARCH, character);
                if (researchBuildings)
                {
                    for (int i = 0; i < researchBuildings->size(); ++i)
                    {
                        auto useable = (*researchBuildings)[i]->getUseableStuff();
                        int priority = settings->getLabourPriority(useable);
                        if (priority <= 0) continue;
                        auto operators = useable->currentOperators;
                        int opCount = operators.size();
                        if (operators.find(character) != operators.end())
                        {
                            opCount -= 1;
                        }
                        if (opCount < useable->numOperatorsMax)
                        {
                            labourCandidates[useable] = opCount;
                        }
                    }
                }
            }
            lektor<Building*>* refineryBuildings = currentTown->findAllBuildingsWithFunction(BF_REFINERY, character);
            if (refineryBuildings)
            {
                for (int i = 0; i < refineryBuildings->size(); ++i)
                {
                    auto production = (*refineryBuildings)[i]->getProductionBuilding();
                    int priority = settings->getLabourPriority(production);
                    if (priority <= 0) continue;
                    if (production->couldIOperate(character))
                    {
                        auto operators = production->currentOperators;
                        int opCount = operators.size();
                        if (operators.find(character) != operators.end())
                        {
                            opCount -= 1;
                        }
                        if (opCount < production->numOperatorsMax)
                        {
                            labourCandidates[production] = opCount;
                        }
                    }
                    else if (production->numOperatorsMax <= 0)
                    {
                        labourCandidates[production] = 0;
                    }
                }
            }
            lektor<Building*>* craftingBuildings = currentTown->findAllBuildingsWithFunction(BF_CRAFTING, character);
            if (craftingBuildings)
            {
                for (int i = 0; i < craftingBuildings->size(); ++i)
                {
                    auto production = (*craftingBuildings)[i]->getProductionBuilding();
                    int priority = settings->getLabourPriority(production);
                    if (priority <= 0) continue;
                    if (production->couldIOperate(character))
                    {
                        auto operators = production->currentOperators;
                        int opCount = operators.size();
                        if (operators.find(character) != operators.end())
                        {
                            opCount -= 1;
                        }
                        if (opCount < production->numOperatorsMax)
                        {
                            labourCandidates[production] = opCount;
                        }
                    }
                    else if (production->numOperatorsMax <= 0)
                    {
                        labourCandidates[production] = 0;
                    }
                }
            }
            lektor<Building*>* mineBuildings = currentTown->findAllBuildingsWithFunction(BF_MINE, character);
            if (mineBuildings)
            {
                for (int i = 0; i < mineBuildings->size(); ++i)
                {
                    auto production = (*mineBuildings)[i]->getProductionBuilding();
                    int priority = settings->getLabourPriority(production);
                    if (priority <= 0) continue;
                    if (production->couldIOperate(character))
                    {
                        auto operators = production->currentOperators;
                        int opCount = operators.size();
                        if (operators.find(character) != operators.end())
                        {
                            opCount -= 1;
                        }
                        if (opCount < production->numOperatorsMax)
                        {
                            labourCandidates[production] = opCount;
                        }
                    }
                    else if (production->numOperatorsMax <= 0)
                    {
                        labourCandidates[production] = 0;
                    }
                }
            }

            UseableStuff* optimalLabour = FindOptimalLabourFromList(character, ai, &labourCandidates, settings);
            if (optimalLabour)
            {
                ProductionBuilding* production = optimalLabour->getProductionBuilding();
                OrdersReceiver* order = character->getOrdersReciever();
                if (production)
                {
                    StorageBuilding* storage = FindResourceStorageBuildingFor(ai, production->getProductionItemData(), production);
                    if (production->isProductionFull())
                    {
                        if (storage && storage->canHaveSomeOfThese(production->getProductionItemData()))
                        {
                            Logger::log("Haul from " + production->displayName + " to " + storage->displayName, Logger::Debug, false);
                            order->addOrder(OPERATE_STORAGE, storage, storage->getPosition(), true, false);
                        }
                        return nullptr;
                    }
                    else if (optimalLabour->numOperatorsMax <= 0)
                    {
                        Logger::log("Automachine: " + production->displayName, Logger::Debug, false);
                        lektor<GameData*> out;
                        production->getResourcesNeededBecauseEmpty(out);
                        if (out.size() > 0)
                        {
                            Logger::log("Haul to " + production->displayName, Logger::Debug, false);
                            order->addOrder(OPERATE_STORAGE, production, production->getPosition(), true, false);
                        }
                        else if (!production->isProductionEmpty())
                        {
                            if (storage && storage->canHaveSomeOfThese(production->getProductionItemData()))
                            {
                                Logger::log("Haul from " + production->displayName + " to " + storage->displayName, Logger::Debug, false);
                                order->addOrder(OPERATE_STORAGE, storage, storage->getPosition(), true, false);
                            }
                        }
                        return nullptr;
                    }
                }
            }
        }
        return optimalLabour;
    }

    bool (*_NV_couldIOperate_orig)(UseableStuff* thisptr, const hand& h);
    bool _NV_couldIOperate_hook(UseableStuff* thisptr, const hand& h)
    {
        Character* character = h.getCharacter();
        Platoon* squad = nullptr;
        SquadSettingsInfo* settings = nullptr;
        if (character && character->getPlatoon()) squad = character->getPlatoon()->me;
        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            OrdersReceiver* order = character->getOrdersReciever();
            Tasker* currentGoal = nullptr;
            if (order) currentGoal = order->tryToGetCurrentGoal();
            if (currentGoal)
            {
                TaskType key = currentGoal->key();
                if (key == AUTO_LABOURING_MINES || key == STAY_IN_HOME)
                {
                    if (thisptr->getSpecialFunction() == BF_RESEARCH)
                    {
                        if (ou->player && ou->player->technology)
                        {
                            if (ou->player->technology->current.size() == 0) return false;
                        }
                    }
                    if (thisptr->getSpecialFunction() == BF_DOOR)
                    {
                        return false;
                    }
                }
            }
            if (thisptr->getSpecialFunction() == BF_TURRET)
            {
                if (!settings->getManTurrets())
                {
                    return false;
                }
            }
            
        }
        return _NV_couldIOperate_orig(thisptr, h);
    }

    float (*findKOIntruder_town_orig)(AI* thisptr, const hand& _a1, hand& out, bool justAsking);
    float findKOIntruder_town_hook(AI* thisptr, const hand& _a1, hand& out, bool justAsking)
    {
        float score = findKOIntruder_town_orig(thisptr, _a1, out, justAsking);
        if (out && out.getCharacter() && out.getCharacter()->isBeingCarried())
        {
            out = nullptr;
            return 0.0;
        }
        return score;
    }

    float (*findMineToWorkAt_orig)(AI* thisptr, const hand& in, hand& out, bool justAsking);
    float findMineToWorkAt_hook(AI* thisptr, const hand& in, hand& out, bool justAsking)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        if (thisptr)
        {
            character = thisptr->getCharacter();
            settings = SquadAutonomySettings::getSingleton().getSquadSettings(thisptr->getPlatoon());
        }
        if (settings && settings->isEnabled())
        {
            if (character)
            {
                out = FindLabourToDo(character, settings);
                if (out)
                {
                    //DebugLog("Found Labour " + out.getBuilding()->displayName);
                    return 1.0;
                }
                //else return 0.0;
            }
        }

        float score = findMineToWorkAt_orig(thisptr, in, out, justAsking);
        
        if (settings && settings->isEnabled())
        {
            if (out && out.getBuilding() && out.getBuilding()->getUseableStuff())
            {
                if (!settings->getLabourPriority(out.getBuilding()->getUseableStuff()))
                {
                    out = nullptr;
                    return 0.0;
                }
            }
        }
        return score;
    }

    float (*runTargetFinder_orig)(AI::AIResultsCacher* thisptr, float (AI::* func)(const hand&, hand&, bool), const TaskMatch& key, hand& out);
    float runTargetFinder_hook(AI::AIResultsCacher* thisptr, float (AI::* func)(const hand&, hand&, bool), const TaskMatch& key, hand& out)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        AI* ai = nullptr;
        if (thisptr) ai = thisptr->ai;
        if (ai && ai->getPlatoon())
        {
            settings = SquadAutonomySettings::getSingleton().getSquadSettings(ai->getPlatoon());
            character = ai->getCharacter();
        }

        if (settings && settings->isEnabled() && character)
        {
            TaskType type = key.key();
            Logger::log("RuntargetFind TaskType: " + Ogre::StringConverter::toString(static_cast<int>(type)), Logger::Info, true);
            if (type == GO_HOME_AND_GO_TO_BED || type == FIND_BED_AND_PUT_IN)
            {
                out = FindOptimalBed(character, settings->getUsePaidBeds());
                if (out) return 1.0;
                else return 0.0;
            }

        }
        float score = runTargetFinder_orig(thisptr, func, key, out);

        if (settings && settings->isEnabled() && character)
        {
            TaskType type = key.key();
            if (type == FIND_AND_RESCUE || type == FIND_AND_RESCUE_IF_THERES_BEDS || type == FIND_AND_RESCUE_LEADER)
            {
                if (out)
                {
                    Blackboard* bb = character->getBlackboard();
                    if (bb)
                    {
                        if (bb->currentPackage && bb->currentPackage->key == SIG_WANDERING_TOWN_TO_TOWN)
                        {
                            Character* target = out.getCharacter();
                            if (target && target->getPlatoon() && target->getPlatoon()->me != settings->getSquad())
                            {
                                out = nullptr;
                                return 0.0;
                            }
                        }
                    }
                }
            }
            Logger::log("EndRuntargetFind", Logger::Info, true);
        }
        return score;
    }

    void (*periodicUpdate_orig)(AITaskSytem* thisptr, float time);
    void periodicUpdate_hook(AITaskSytem* thisptr, float time)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        PlayerInterface* pi = nullptr;
        Faction* faction = nullptr;
        Platoon* platoon = nullptr;
        Tasker* currentGoal = nullptr;
        bool resetDuration = false;
        if (thisptr)
        {
            character = thisptr->character;
            currentGoal = thisptr->tryToGetCurrentGoal();
        }
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon) settings = SquadAutonomySettings::getSingleton().getSquadSettings(platoon);

        if (settings && settings->isEnabled())
        {
            Logger::log("PeriodicUpdate", Logger::Info, true);

            TaskData* data = taskTypetaskData->find(RELAX_IN_TOWN_PACKAGE)->second;
            data->setDurationBased(1.0, 4.0, false);
            data = taskTypetaskData->find(GO_HOME_AND_GO_TO_BED)->second;
            data->setDurationBased(4.0, 4.0, false);

            //if (character == gui->selectedObject.getCharacter()) DebugLog("PeriodUpdate: " + character->displayName);

            if (settings->isRestTime())
            {
                if (!settings->assignSquadHome(true))
                {
                    SetNearestFriendlyTownAsHome(platoon);
                }
            }
            else
            {
                if (!settings->assignSquadHome(false))
                {
                    if (!settings->assignSquadHome(true))
                    {
                        SetNearestFriendlyTownAsHome(platoon);
                    }
                }
            }
            faction = character->getFaction();
            if (faction)
            {
                faction->isPlayer = nullptr;
            }
            //DebugLog("End PeriodicUpdate");
        }
        /*========Orig Function=======*/
        periodicUpdate_orig(thisptr, time);
        /*========Orig Function=======*/
        if (settings && settings->isEnabled())
        {
            RevertTaskDuration(RELAX_IN_TOWN_PACKAGE);
            RevertTaskDuration(GO_HOME_AND_GO_TO_BED);
            if (faction) faction->isPlayer = settings->getPlayerInterface();
            Logger::log("End periodUpdate", Logger::Info, true);
        }
    }

    void (*choosePermaJob_orig)(AITaskSytem* thisptr, std::map<float, Tasker*, std::less<float>, Ogre::STLAllocator<std::pair<float const, Tasker*>, Ogre::GeneralAllocPolicy > >& orderedGoals, TaskMatch& alreadyHasGoal, bool urgentOnes, bool _jobsEnabled);
    void choosePermaJob_hook(AITaskSytem* thisptr, std::map<float, Tasker*, std::less<float>, Ogre::STLAllocator<std::pair<float const, Tasker*>, Ogre::GeneralAllocPolicy > >& orderedGoals, TaskMatch& alreadyHasGoal, bool urgentOnes, bool _jobsEnabled)
    {
        //non-urgent are autosit autosleep autoditch etc.
        Character* character = nullptr;
        if (thisptr) character = thisptr->character;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        Faction* faction = nullptr;
        bool origAutoSit = false;
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon)
        {
            faction = platoon->getFaction();
            settings = SquadAutonomySettings::getSingleton().getSquadSettings(platoon);
        }
        if (settings && settings->isEnabled())
        {
            Logger::log("PermaJob", Logger::Info, true);
            if (!urgentOnes)
            {
                if (faction && settings->getPlayerInterface())
                {
                    origAutoSit = settings->getPlayerInterface()->aiOptions.autoSit;
                    faction->isPlayer = settings->getPlayerInterface();
                    faction->isPlayer->aiOptions.autoSit = false;
                }
            }

            if (settings->isRestTime())
            {
                _jobsEnabled = false;
            }

        }
        /*========Orig Function=======*/
        choosePermaJob_orig(thisptr, orderedGoals, alreadyHasGoal, urgentOnes, _jobsEnabled);
        /*========Orig Function=======*/
        if (settings && settings->isEnabled())
        {
            if (faction && faction->isPlayer && settings->getPlayerInterface())
            {
                faction->isPlayer->aiOptions.autoSit = origAutoSit;
                faction->isPlayer = nullptr;
            }
            Logger::log("End PermaJob", Logger::Info, true);
            /*if (settings && settings->isEnabled())
            {
                auto currentGoals = thisptr->orderedGoals;
                if (currentGoals.size() > 0)
                {
                    for (auto it = currentGoals.begin(); it != currentGoals.end(); ++it)
                    {
                        if (it->second)
                        {
                            DebugLog("After Choose PermaJob: " + it->second->getDescription() + " score: " + Ogre::StringConverter::toString(it->first));
                        }
                    }
                }
            }*/
        }
    }
    

    void (*currentActionChecks_orig)(AITaskSytem* thisptr);
    void currentActionChecks_hook(AITaskSytem* thisptr)
    {
        currentActionChecks_orig(thisptr);
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        Tasker* currentAction = thisptr->tryToGetCurrentGoal();
        if (thisptr)  character = thisptr->character;
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon) settings = SquadAutonomySettings::getSingleton().getSquadSettings(platoon);
        if (settings && settings->isEnabled())
        {
            if (currentAction)
            {
                TaskType type = currentAction->key();

                //is player order?
                if (thisptr->hasPlayerOrder(type)) return;

                Logger::log("CurrentActionChecks TaskType: " + Ogre::StringConverter::toString(static_cast<int>(type)), Logger::Info, true);
                if (type == MAN_A_TURRET || type == MAN_A_TURRET_ON_BUILDING || type == MAN_THE_GATE || type == AUTO_LABOURING_MINES ||
                    type == STAND_AT_GUARD_NODE_HOMEBUILDING_INDOORS_ONLY || type == STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT ||
                    type == STAND_AT_GUARD_NODE_HOMETOWN_OUTSIDE || type == REPAIR || type == BUILD)
                {
                    if (settings->isRestTime())
                    {
                        thisptr->clearCurrentGoal(true);
                        return;
                    }
                }
                if (type == GO_HOME_AND_GO_TO_BED)
                {
                    bool wakeup = true;
                    if (settings->getRestUntilHealed())
                    {
                        MedicalSystem* medical = character->getMedical();
                        RaceData* race = character->getRace();
                        if (race && !race->robot && medical && medical->restedState < settings->getHealedThresholdP() && medical->scoreFirstAidNeed(false) < 0.1)
                        {
                            wakeup = false;
                        }
                    }
                    if (settings->getDoSleep() && settings->isRestTime())
                    {
                        wakeup = false;
                    }
                    if (wakeup)
                    {
                        thisptr->clearCurrentGoal(true);
                        return;
                    }
                }
                Logger::log("CurrentActionChecks End", Logger::Info, true);
            }
        }
    }

    void (*chooseGoal_orig)(AITaskSytem* thisptr, bool timedLockOnCurrentGoal);
    void chooseGoal_hook(AITaskSytem* thisptr, bool timedLockOnCurrentGoal)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        if (thisptr)  character = thisptr->character;
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon) settings = SquadAutonomySettings::getSingleton().getSquadSettings(platoon);
        if (settings && settings->isEnabled())
        {
            Logger::log("chooseGoal", Logger::Info, true);

        }
        /*========Orig Function=======*/
        chooseGoal_orig(thisptr, timedLockOnCurrentGoal);
        /*========Orig Function=======*/
        if (settings && settings->isEnabled())
        {
            Logger::log("chooseGoal End", Logger::Info, true);
        }
    }

    void (*clearCurrentGoal_orig)(OrdersReceiver* thisptr, bool force);
    void clearCurrentGoal_hook(OrdersReceiver* thisptr, bool force)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        if (thisptr) character = thisptr->me;
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon) settings = SquadAutonomySettings::getSingleton().getSquadSettings(platoon);
        if (settings && settings->isEnabled())
        {
            //Log("Clear current goal");
            auto currentGoal = thisptr->tryToGetCurrentGoal();
            if (currentGoal)
            {
                TaskType type = currentGoal->key();
                if (type == WANDERING_TRADER ||
                    type == TRAVEL_TO_TARGET_TOWN || type == TRAVEL_TO_TARGET_TOWN_FAST || type == TRAVEL_TO_TARGET_PACKAGE)
                {
                    return;
                }
                //Log("End Clear current goal");
            }
        }
        /*========Orig Function=======*/
        clearCurrentGoal_orig(thisptr, force);
        /*========Orig Function=======*/
    }

    void (*update4Frame_orig)(AITaskSytem* thisptr, Ogre::Vector3 position, float time);
    void update4Frame_hook(AITaskSytem* thisptr, Ogre::Vector3 position, float time)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        if (thisptr)
        {
            character = thisptr->character;
        }
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon)
        {
            settings = SquadAutonomySettings::getSingleton().getSquadSettings(platoon);
        }
        if (settings && settings->isEnabled())
        {
            Logger::log("update4Frame", Logger::Info, true);
            TaskData* data = taskTypetaskData->find(RELAX_IN_TOWN_PACKAGE)->second;
            data->setDurationBased(1.0, 4.0, false);
            data = taskTypetaskData->find(GO_HOME_AND_GO_TO_BED)->second;
            data->setDurationBased(4.0, 4.0, false);

            if (settings->getCloseGate())
            {
                auto currentTask = thisptr->getCurrentGoal();
                if (currentTask && currentTask.key() == MAN_THE_GATE && closeGateTimerOn)
                {
                    closeGateCDTimer -= time;
                    Logger::log("Close Gate Timer: " + Ogre::StringConverter::toString(closeGateCDTimer), Logger::Debug, false);
                }
            }
        }
        /*========Orig Function=======*/
        update4Frame_orig(thisptr, position, time);
        /*========Orig Function=======*/
        if (settings && settings->isEnabled())
        {
            RevertTaskDuration(RELAX_IN_TOWN_PACKAGE);
            RevertTaskDuration(GO_HOME_AND_GO_TO_BED);
        }
    }

    void RevertTaskDuration(TaskType type)
    {
        TaskData* data = taskTypetaskData->find(type)->second;
        if (taskTypeOrigDataDuration.find(type) != taskTypeOrigDataDuration.end())
        {
            //DebugLog("Reverting Duration!");
            auto orig = taskTypeOrigDataDuration.find(type)->second;
            data->durationMin = orig->durationMin;
            data->durationFuzz = orig->durationFuzz;
            data->isDurationBased = orig->isDurationBased;
            data->endsAfterTime = orig->endsAfterTime;
        }
    }

#pragma region MAN THE GATE target fix
    

    void (*Task_MoveToDoor_Gate_ChooseSide_gatePosition_orig)(Tasker* thisptr, Ogre::Vector3& val, CharBody* body);
    void Task_MoveToDoor_Gate_ChooseSide_gatePosition_hook(Tasker* thisptr, Ogre::Vector3& val, CharBody* body)
    {
        Platoon* squad = nullptr;
        Character* character = nullptr;
        if (body) character = body->getCharacter();
        if (character && character->getPlatoon()) squad = character->getPlatoon()->me;
        SquadSettingsInfo* settings = nullptr;
        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            Ownerships* own = squad->getOwnerships();
            thisptr->subject.setNull();
            if (own)
            {
                thisptr->subject = own->_homeBuilding;
                if (own->_homeBuilding)
                {
                    destGate = own->_homeBuilding.getBuilding();
                }
            }
        }
        Task_MoveToDoor_Gate_ChooseSide_gatePosition_orig(thisptr, val, body);
    }



    void (*Task_ManTheGate_Update_orig)(Tasker* thisptr, CharBody* body);
    void Task_ManTheGate_Update_hook(Tasker* thisptr, CharBody* body)
    {
        /*if (thisptr->subject && thisptr->subject.getBuilding())
        {
            DebugLog("Gate subject: " + thisptr->subject.getBuilding()->displayName);
        }
        if (body && body->getCharacter() && body->getCharacter()->getOwnerships())
        {
            Ownerships* own = body->getCharacter()->getPlatoon()->me->getOwnerships();
            if (own && own->_homeBuilding && own->_homeBuilding.getBuilding())
            {
                DebugLog("Home: " + own->_homeBuilding.getBuilding()->displayName);
            }
        }*/
        Task_ManTheGate_Update_orig(thisptr, body);
    }

    void (*_NV_setDestination_orig)(CharMovement* thisptr, const Ogre::Vector3& dest, UpdatePriority priority, bool notVertical);
    void _NV_setDestination_hook(CharMovement* thisptr, const Ogre::Vector3& dest, UpdatePriority priority, bool notVertical)
    {
        Platoon* squad = nullptr;
        Character* character = thisptr->getCharacter();
        if (character && character->getPlatoon()) squad = character->getPlatoon()->me;
        SquadSettingsInfo* settings = nullptr;
        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            Tasker* currentTask = nullptr;
            OrdersReceiver* order = character->getOrdersReciever();
            if (order) currentTask = order->tryToGetCurrentGoal();
            if (currentTask)
            {
                TaskType key = currentTask->key();
                if (key == STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT)
                {
                    Building* guardTarget = nullptr;
                    Ownerships* own = squad->getOwnerships();
                    if (own && own->_homeBuilding) guardTarget = own->_homeBuilding.getBuilding();
                    if (guardTarget)
                    {
                        //DebugLog("Static Guard: " + guardTarget->displayName);
                        //currentTask->subject = guardTarget;
                        DoorStuff* door = guardTarget->getDoor();
                        if (door)
                        {
                            Ogre::Vector3 doorPos = door->getDoorPosition();
                            if (settings->getStayInsideGate())
                            {
                                Ogre::Vector3 doorInside = door->getDoorPosInside_extraFarIn(4.0);
                                Ogre::Vector3 displace = doorInside - doorPos;
                                Ogre::Vector3 newPos = dest + displace;
                                currentTask->setLocation(newPos);
                                _NV_setDestination_orig(thisptr, newPos, priority, notVertical);
                                return;
                            }
                        }
                    }
                }
                if (key == MAN_THE_GATE)
                {
                    if (destGate)
                    {
                        DoorStuff* door = destGate->getDoor();
                        destGate = nullptr;
                        if (door)
                        {
                            Ogre::Vector3 doorPos = settings->getStayInsideGate() ? door->getDoorPosInside_extraFarIn(2.0) : door->getDoorPosOutside();
                            currentTask->setLocation(doorPos);
                            _NV_setDestination_orig(thisptr, doorPos, priority, notVertical);
                            return;
                        }
                    }
                }
            }
        }
        destGate = nullptr;
        _NV_setDestination_orig(thisptr, dest, priority, notVertical);
    }
    Building* (*Task_ManTheGate_FindGate_orig)(Tasker* thisptr, Character* character, hand subject);
    Building* Task_ManTheGate_FindGate_hook(Tasker* thisptr, Character* character, hand subject)
    {
        Platoon* squad = nullptr;
        if (character->getPlatoon()) squad = character->getPlatoon()->me;
        SquadSettingsInfo* settings = nullptr;
        
        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            //DebugLog("ManTheGate FIND GATE");
            Ownerships* own = squad->getOwnerships();
            Building* home = nullptr;
            TownBase* town = nullptr;
            if (own && own->_homeBuilding)
            {
                home = own->_homeBuilding.getBuilding();
            }
            lektor<Building*> gates;
            //thisptr->subject = nullptr;
            subject.setNull();
            thisptr->subject.setNull();
            if (home)
            {
                subject = home->getHandle();
                thisptr->subject = home->getHandle();
            }
        }

        auto gate = Task_ManTheGate_FindGate_orig(thisptr, character, subject);

        if (settings && settings->isEnabled())
        {
            destGate = gate;
        }
        return gate;
    }

    bool (*_isRequirementsComplete_orig)(TaskData* thisptr, AI* ai, const hand& target, const Ogre::Vector3& location, hand& subTarget, bool autoTargetFinder, StateType& failedOn);
    bool _isRequirementsComplete_hook(TaskData* thisptr, AI* ai, const hand& target, const Ogre::Vector3& location, hand& subTarget, bool autoTargetFinder, StateType& failedOn)
    {
        Platoon* squad = ai->getPlatoon();
        SquadSettingsInfo* settings = nullptr;
        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            Character* character = ai->getCharacter();
            if (thisptr)
            {
                TaskType key = thisptr->key;
                if (key == MAN_THE_GATE || (key == STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT && settings->getStayInsideGate()))
                {
                    //idk how this works
                    //from what I can tell, it keeps returning false because something about Task StateType requirement failing
                    //seemingly something to do with function AI::stateIsTrue and func 61e990 and DAT_141e45450
                    //CharMovement* movement = nullptr;
                    //if (character) movement = character->getMovement();
                    if (character->pos.squaredDistance(location) <= 25.0f)
                    //if (movement && movement->isDestinationReached())
                    {
                        return true;
                    }
                    else return false;
                }
            }
        }
        return _isRequirementsComplete_orig(thisptr, ai, target, location, subTarget, autoTargetFinder, failedOn);
    }

    void (*Task_OpenDoor_StartAction_orig)(Tasker* thisptr, CharBody* body);
    void Task_OpenDoor_StartAction_hook(Tasker* thisptr, CharBody* body)
    {
        Platoon* squad = nullptr;
        Character* character = nullptr;
        if (body) character = body->getCharacter();
        if (character && character->getPlatoon()) squad = character->getPlatoon()->me;
        SquadSettingsInfo* settings = nullptr;
        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            if (settings->getCloseGate())
            {
                OrdersReceiver* order = character->getOrdersReciever();
                if (order)
                {
                    Tasker* currentGoal = order->tryToGetCurrentGoal();
                    if (currentGoal && currentGoal->key() == MAN_THE_GATE && !order->hasPlayerOrder(OPEN_DOOR))
                    {
                        //DebugLog("Open Door");
                        return;
                    }
                }
            }
        }
        Task_OpenDoor_StartAction_orig(thisptr, body);
    }

    void (*Task_OpenDoor_Update_orig)(Tasker* thisptr, CharBody* body);
    void Task_OpenDoor_Update_hook(Tasker* thisptr, CharBody* body)
    {
        Platoon* squad = nullptr;
        Character* character = nullptr;
        if (body) character = body->getCharacter();
        if (character && character->getPlatoon()) squad = character->getPlatoon()->me;
        SquadSettingsInfo* settings = nullptr;

        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            if (settings->getCloseGate())
            {
                OrdersReceiver* order = character->getOrdersReciever();
                if (order)
                {
                    Tasker* currentGoal = order->tryToGetCurrentGoal();
                    if (currentGoal && currentGoal->key() == MAN_THE_GATE && !order->hasPlayerOrder(OPEN_DOOR))
                    {
                        //DebugLog("Open Door");
                        return;
                    }
                }
            }
        }
        Task_OpenDoor_Update_orig(thisptr, body);
    }

    void (*_NV_threadedUpdate_orig)(Character* thisptr);
    void _NV_threadedUpdate_hook(Character* thisptr)
    {
        _NV_threadedUpdate_orig(thisptr);
        Platoon* squad = nullptr;
        if (thisptr && thisptr->getPlatoon()) squad = thisptr->getPlatoon()->me;
        SquadSettingsInfo* settings = nullptr;
        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            Tasker* currentTask = nullptr;
            TaskRepertoire* repertoire = nullptr;
            AITaskSytem* order = thisptr->ai->getTaskSystem();
            CharBody* body = thisptr->getBody();
            if (order)
            {
                currentTask = order->tryToGetCurrentGoal();
                if (order->aiImplementation) repertoire = order->aiImplementation->taskRepertoire;
            }
            if (currentTask)
            {
                TaskType key = currentTask->key();
                if (key == MAN_THE_GATE || key == STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT)
                {
                    //DebugLog("ManTheGate");
                    //const TaskData* taskData = currentTask->getTaskData();
                    //if (taskData) DebugLog("Man The Gate Permajob Associate: " + Ogre::StringConverter::toString(taskData->getPermaJobType()) + ", " + Ogre::StringConverter::toString(taskData->getPermaJobAssociation()) + ", " + Ogre::StringConverter::toString(taskData->getPermaJobAssociation_secondary()));
                    Building* gate = nullptr;
                    if (squad->getOwnerships() && squad->getOwnerships()->_homeBuilding) gate = squad->getOwnerships()->_homeBuilding.getBuilding();
                    DoorStuff* door = nullptr;
                    if (gate)
                    {
                        door = gate->getDoor();
                    }
                    if (door)
                    {
                        //currentTask->subject = gate->getHandle();
                        //currentTask->setLocation(gate->pos);
                        if (door->isDamaged())
                        {
                            Tasker* newTask = GOAPTaskMgr_createNewTask(order->aiImplementation->taskRepertoire, REPAIR, door, currentTask->priority, currentTask->weight, door->getPosition(), settings->getStartWorkTime(), settings->getEndWorkTime());
                            if (body) body->setCurrentAction(newTask);
                            //order->addOrder(REPAIR, door, door->pos, true, false);
                            //order->getFirstOrder()->startTime = settings->getStartWorkTime();
                            //order->getFirstOrder()->endTime = settings->getEndWorkTime();
                        }
                        else if (key == MAN_THE_GATE && settings->getCloseGate())
                        {

                            bool isInsideGate = true;
                            if (settings->getStayInsideGate() && squad->activePlatoon)
                            {
                                for (auto it = squad->activePlatoon->things.begin(); it != squad->activePlatoon->things.end(); ++it)
                                {
                                    Character* obj = reinterpret_cast<Character*>(*it);
                                    if (obj->amInsideTownWalls() == 0)
                                    {
                                        isInsideGate = false;
                                        break;
                                    }
                                }
                            }
                            //DebugLog("IsInsideGate: " + Ogre::StringConverter::toString(isInsideGate));
                            if (isInsideGate)
                            {
                                if (door->getDoorState() == DOORSTATE_OPEN)
                                {
                                    //DebugLog("Closing GATE");
                                    if (!closeGateTimerOn)
                                    {
                                        closeGateTimerOn = true;
                                        closeGateCDTimer = closeGateCD;
                                    }
                                    else if (closeGateCDTimer <= 0.0)
                                    {
                                        //Tasker* newTask = GOAPTaskMgr_createNewTask(order->aiImplementation->taskRepertoire, CLOSE_DOOR, door, currentTask->priority, currentTask->weight, door->getPosition(), settings->getStartWorkTime(), settings->getEndWorkTime());
                                        //if (body) body->setCurrentAction(newTask);
                                        order->addOrder(CLOSE_DOOR , door, door->pos, true, false);
                                        order->getFirstOrder()->startTime = settings->getStartWorkTime();
                                        order->getFirstOrder()->endTime = settings->getEndWorkTime();
                                        closeGateCDTimer = 0.0;
                                        closeGateTimerOn = false;
                                    }
                                }
                                else
                                {
                                    closeGateCDTimer = 0.0;
                                    closeGateTimerOn = false;
                                }
                            }
                            else if (!isInsideGate && (door->getDoorState() == DOORSTATE_CLOSED))
                            {
                                //Tasker* newTask = GOAPTaskMgr_createNewTask(order->aiImplementation->taskRepertoire, OPEN_DOOR, door, currentTask->priority, currentTask->weight, door->getPosition(), settings->getStartWorkTime(), settings->getEndWorkTime());
                                //order->setCurrentGoal(newTask, order->currentGoalScore + 1.0, order->currentGoalPriority);
                                //if (body) body->setCurrentAction(newTask);
                                order->addOrder(OPEN_DOOR, door, door->pos, true, false);
                                order->getFirstOrder()->startTime = settings->getStartWorkTime();
                                order->getFirstOrder()->endTime = settings->getEndWorkTime();
                            }
                        }
                    }
                }
            }
        }
    }
#pragma endregion



    bool (*Package_WanderingTrader_signalStart_orig)(AIPackage* thisptr);
    bool Package_WanderingTrader_signalStart_hook(AIPackage* thisptr)
    {

        Platoon* squad = thisptr->squad;
        SquadSettingsInfo* settings = nullptr;
        ActivePlatoon* activePlatoon = nullptr;
        if (squad)
        {
            activePlatoon = squad->activePlatoon;
            settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        }
        if (settings && settings->isEnabled() && squad)
        {
            //DebugLog("periodicUpdate: Wandering");
            //check if all squads members are awake/active
            for (auto iter = activePlatoon->things.begin(); iter != activePlatoon->things.end(); ++iter)
            {
                auto obj = reinterpret_cast<Character*>(*iter);
                StateBroadcastData* stateBroadcast = obj->getStateBroadcast();
                MedicalSystem* medical = obj->getMedical();
                if (stateBroadcast && (stateBroadcast->isSleeping || stateBroadcast->unconcious))
                {
                    //DebugLog("Member is sleeping");
                    return false;
                }
                if (medical && (medical->restedState < settings->getHealedThresholdP()))
                {
                    return false;
                }
            }

            //DebugLog("wasASuccessEnd");
        }

        return Package_WanderingTrader_signalStart_orig(thisptr);
    }

    float (*score_orig)(Tasker* thisptr, AI* ai);
    float score_hook(Tasker* thisptr, AI* ai)
    {
        float score = score_orig(thisptr, ai);

        Platoon* squad = nullptr;
        SquadSettingsInfo* settings = nullptr;
        StateBroadcastData* stateBroadcast = nullptr;
        Character* character = nullptr;
        AITaskSytem* taskSystem = nullptr;
        if (ai)
        {
            squad = ai->getPlatoon();
            character = ai->getCharacter();
            stateBroadcast = ai->getStateBroadcast();
            taskSystem = ai->getTaskSystem();
        }
        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled() && thisptr)
        {
            TaskType type = thisptr->key();
            const TaskData* taskData = thisptr->getTaskData();
            if (taskData && taskData->isPermaJob()) return score;

            Logger::log("score TaskType: " +Ogre::StringConverter::toString(static_cast<int>(type)) + " - "  +Ogre::StringConverter::toString(score), Logger::Info, true);
            if (type == STAY_IN_HOME || type == SIT_AROUND)
            {
                if (!settings->isRestTime())
                {
                    return score * 0.001;
                }
            }
            else if (type == MAN_A_TURRET || type == MAN_A_TURRET_ON_BUILDING)
            {
                if (!settings->getManTurrets() || settings->isRestTime())
                {
                    return 0.0;
                }
            }
            else if (type == BODYGUARD || type == FOLLOW_SQUADLEADER)
            {
                Character* leader = squad->getSquadLeader();
                StateBroadcastData* leaderState = nullptr;
                OrdersReceiver* leaderOrder = nullptr;
                if (leader)
                {
                    leaderState = leader->getStateBroadcast();
                    leaderOrder = leader->getOrdersReciever();
                }
                if (leaderState && leaderState->isSleeping)
                {
                    return 0.0;
                }
                if (leaderOrder)
                {
                    auto currentGoal = leaderOrder->getCurrentGoal();
                    if (currentGoal && currentGoal.key() == RELAX_IN_TOWN_PACKAGE)
                    {
                        return 0.0;
                    }
                }
            }
            else if (type == MAN_THE_GATE || type == STAND_AT_GUARD_NODE_HOMEBUILDING_INDOORS_ONLY || type == STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT || type == STAND_AT_GUARD_NODE_HOMETOWN_OUTSIDE ||
                type == TERRITORIAL_AGGRESSION_BUT_DONT_LEAVE_HOME || type == ATTACK_ENEMIES ||
                type == AUTO_LABOURING_MINES || type == AUTO_LABOURING_MINES_PRETEND)
            {
                if (type == TERRITORIAL_AGGRESSION_BUT_DONT_LEAVE_HOME || type == ATTACK_ENEMIES)
                {
                    if (!settings->getAttackEnemies())
                    {
                        return 0.0;
                    }
                }
                if (settings->isRestTime())
                {
                    return 0.0;
                }
            }
            else if (type == PROTECT_ALLIES || type == PROTECT_ALLIES_STAY_IN_TOWN || type == PROTECT_OWN_SQUAD)
            {
                if (!settings->getProtectAllies())
                {
                    return 0.0;
                }
                if (stateBroadcast && stateBroadcast->isSleeping && !character->isLiterallyUnderMeleeAttackRightNowForSure())
                {
                    return 0.0;
                }
            }
            else if (type == JOB_MEDIC || type == JOB_REPAIR_ROBOT)
            {
                if (!settings->getDoMedic())
                {
                    return 0.0;
                }
                MedicalSystem* medical = nullptr;
                RaceData* race = nullptr;
                if (character)
                {
                    race = character->getRace();
                    medical = character->getMedical();
                }
                return (medical->scoreFirstAidNeed(race->robot)) * 2.0;
            }
            else if (type == PATROL_TOWN)
            {
                Tasker* currentTask = taskSystem->tryToGetCurrentGoal();
                if (currentTask)
                {
                    if (currentTask->key() == PATROL_TOWN)
                    {
                        float score = taskSystem->currentGoalScore;
                        score *= 0.95;
                        //DebugLog("Patrol score: " + Ogre::StringConverter::toString(score));
                        taskSystem->currentGoalScore = std::max(score, 0.0001f);
                        return taskSystem->currentGoalScore;
                    }
                }
                if (settings->isRestTime())
                {
                    if (character->isInsideBuilding)
                    {
                        return 0.0;
                    }
                }
            }
            else if (type == RELAX_IN_TOWN_PACKAGE)
            {
                Character* leader = squad->getSquadLeader();
                if (leader && character != leader)
                {
                    OrdersReceiver* leaderOrder = leader->getOrdersReciever();
                    if (leaderOrder)
                    {
                        const TaskMatch currentGoal = leaderOrder->getCurrentGoal();
                        if (currentGoal.key() == RELAX_IN_TOWN_PACKAGE)
                        {
                            return 1.0;
                        }
                    }

                }
            }

            /*else if (type == GET_OUT_OF_BED_IF_ITS_EMERGENCY || type == GET_OUT_OF_BED)
            {
                MedicalSystem* medical = nullptr;
                RaceData* race = nullptr;
                if (character)
                {
                    race = character->getRace();
                    medical = character->getMedical();
                }
                if (settings->getDoSleep())
                {
                    if (race && !race->robot && medical && medical->scoreFirstAidNeed(false) < 0.1 && !character->isLiterallyUnderMeleeAttackRightNowForSure())
                    {
                        if (settings->getRestUntilHealed() && medical->restedState < settings->getHealedThreshold())
                        {
                            return 0.0;
                        }
                        if (medical->restedState < settings->getRestThreshold())
                        {
                            return 0.0;
                        }
                    }
                }
            }*/
            else if (type == FIND_AND_RESCUE || type == FIND_AND_RESCUE_IF_THERES_BEDS || type == FIND_AND_RESCUE_LEADER)
            {
                if (!settings->getDoRescue())
                {
                    return 0.0;
                }
            }
            Logger::log("End score", Logger::Info, true);
        }
        return score;
    }

    float (*_NV_scoreGoToBed_orig)(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2);
    float _NV_scoreGoToBed_hook(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2)
    {
        //float score = _NV_scoreGoToBed_orig(thisptr, subject, _a2);
        Platoon* squad = nullptr;
        SquadSettingsInfo* settings = nullptr;
        StateBroadcastData* stateBroadcast = nullptr;
        Character* character = nullptr;
        AITaskSytem* taskSystem = nullptr;
        if (thisptr)
        {
            squad = thisptr->getPlatoon();
            taskSystem = thisptr->getTaskSystem();
            character = thisptr->getCharacter();
            stateBroadcast = thisptr->getStateBroadcast();
        }
        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            Logger::log("score-gotobed", Logger::Info, true);
            MedicalSystem* medical = nullptr;
            RaceData* race = nullptr;
            Faction* faction = nullptr;
            if (character)
            {
                race = character->getRace();
                medical = character->getMedical();
                faction = character->getFaction();
            }
            //for auto sleep task
            if (faction && faction->isPlayer)
            {
                //DebugLog(character->displayName + "AutoSleep");
                return _NV_scoreGoToBed_orig(thisptr, subject, _a2);
            }

            if (settings->getRestUntilHealed())
            {

                if (race && !race->robot && medical && medical->restedState < settings->getHealedThresholdP() && medical->scoreFirstAidNeed(false) < 0.1)
                {
                    if (stateBroadcast && stateBroadcast->isSleeping)
                    {
                        //DebugLog("GoToBed continue sleep until healed");
                        //DebugLog(character->displayName + " medical restedstate: " + Ogre::StringConverter::toString(medical->restedState));
                        return 5.0;
                    }
                    if (settings->isRestTime())
                    {
                        //DebugLog("GoToBed rest");
                        return 5.0;
                    }
                    if (medical->restedState <= settings->getRestThresholdP()) return 20.0;
                }
            }
            if (settings->getDoSleep())
            {
                if (settings->isRestTime())
                {
                    if (stateBroadcast && !stateBroadcast->isSleeping)
                    {
                        //DebugLog(character->displayName + "GoToBed normal");
                        float minuteSinceLastSlept = stateBroadcast->lastSlept.getHoursPassed() * 60.0;
                        if (minuteSinceLastSlept < 120.0)
                        {
                            return 0.0;
                        }
                        if (minuteSinceLastSlept < 480.0)
                        {
                            return 0.01;
                        }
                        if (minuteSinceLastSlept < 720.0)
                        {
                            return 0.1;
                        }
                        if (minuteSinceLastSlept < 1080.0)
                        {
                            return 0.5;
                        }
                        return 1.0;
                    }
                }
            }
            return 0.0;
            Logger::log("end gotobed", Logger::Info, true);
        }
        return _NV_scoreGoToBed_orig(thisptr, subject, _a2);
    }

    float (*_NV_scoreGetOutOfBed_orig)(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2);
    float _NV_scoreGetOutOfBed_hook(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2)
    {
        //float score = _NV_scoreGetOutOfBed_orig(thisptr, subject, _a2);
        Platoon* squad = nullptr;
        SquadSettingsInfo* settings = nullptr;
        StateBroadcastData* stateBroadcast = nullptr;
        Character* character = nullptr;
        AITaskSytem* taskSystem = nullptr;
        if (thisptr)
        {
            squad = thisptr->getPlatoon();
            taskSystem = thisptr->getTaskSystem();
        }

        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled() && thisptr)
        {
            Logger::log("score-getoutofbed", Logger::Info, true);
            character = thisptr->getCharacter();
            MedicalSystem* medical = nullptr;
            RaceData* race = nullptr;
            Faction* faction = nullptr;
            if (character)
            {
                race = character->getRace();
                medical = character->getMedical();
                faction = character->getFaction();
                stateBroadcast = character->getStateBroadcast();
            }
            Tasker* currentGoal = taskSystem->tryToGetCurrentGoal();
            if (currentGoal && (currentGoal->key() == GO_HOME_AND_GO_TO_BED || currentGoal->key() == GO_HOME_AND_GO_TO_BED_SECURE))
            {
                //if still injured and has get rest until healed
                if (character->isLiterallyUnderMeleeAttackRightNowForSure())
                {
                    //DebugLog("GetOutOfBed under melee attack");
                    return 1.0;
                }
                if (settings->getRestUntilHealed() && race && !race->robot && medical && medical->restedState < settings->getHealedThresholdP() && medical->scoreFirstAidNeed(false) < 0.1)
                {
                    //DebugLog(character->displayName + " medical restedstate: " + Ogre::StringConverter::toString(medical->restedState));
                    //DebugLog("GetOutOfBed rest until healed");
                    return 0.0;
                }
                if (settings->getDoSleep())
                {
                    if (!settings->isRestTime())
                    {
                        //DebugLog("GetOutOfBed Sleep outside of rest time");
                        return 1.0;
                    }
                }
            }
            Logger::log("End score-getoutofbed", Logger::Info, true);
        }
        return _NV_scoreGetOutOfBed_orig(thisptr, subject, _a2);
    }

    bool (*wantsToEatNow_orig)(Character* thisptr);
    bool wantsToEatNow_hook(Character* thisptr)
    {
        //this calls Character::amSomeoneWhoNeedsToEatToLive() which checks if the faction->isPlayer is true so we have to return PI for proper logic
        Platoon* squad = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Faction* faction = thisptr->getFaction();
        bool removePI = false;
        if (thisptr && thisptr->getPlatoon()) squad = thisptr->getPlatoon()->me;
        if (squad) settings = SquadAutonomySettings::getSingleton().getSquadSettings(squad);
        if (settings && settings->isEnabled())
        {
            if (faction && !faction->isPlayer)
            {
                faction->isPlayer = settings->getPlayerInterface();
                removePI = true;
            }
        }
        bool result = wantsToEatNow_orig(thisptr);
        if (settings && settings->isEnabled() && faction && removePI)
        {
            faction->isPlayer = nullptr;
        }
        return result;
    }

    bool (*initialisation_orig)(GameWorld* thisptr);
    bool initialisation_hook(GameWorld* thisptr)
    {
        bool result = initialisation_orig(thisptr);
        if (result)
        {
            Logger::log("Initialised gameworld data", Logger::Info, false);
            //taskData are initialised here
            auto relaxData = getTaskDataConst(RELAX_IN_TOWN_PACKAGE);
            OriginalTaskDataDuration* relaxOrig = new OriginalTaskDataDuration(relaxData->durationMin, relaxData->durationFuzz, relaxData->isDurationBased, relaxData->endsAfterTime);
            taskTypeOrigDataDuration[RELAX_IN_TOWN_PACKAGE] = relaxOrig;
            /*DebugLog("relax Task : " + Ogre::StringConverter::toString(relaxData->durationMin) + ", " + Ogre::StringConverter::toString(relaxData->durationFuzz)
                + ", " + Ogre::StringConverter::toString(relaxData->isDurationBased) + ", " + Ogre::StringConverter::toString(relaxData->endsAfterTime));*/
            auto goHomeToBedData = getTaskDataConst(GO_HOME_AND_GO_TO_BED);
            OriginalTaskDataDuration* goHomeToBedOrig = new OriginalTaskDataDuration(goHomeToBedData->durationMin, goHomeToBedData->durationFuzz, goHomeToBedData->isDurationBased, goHomeToBedData->endsAfterTime);
            taskTypeOrigDataDuration[GO_HOME_AND_GO_TO_BED] = goHomeToBedOrig;
            /*DebugLog("go bed Task : " + Ogre::StringConverter::toString(goHomeToBedData->durationMin) + ", " + Ogre::StringConverter::toString(goHomeToBedData->durationFuzz)
                + ", " + Ogre::StringConverter::toString(goHomeToBedData->isDurationBased) + ", " + Ogre::StringConverter::toString(goHomeToBedData->endsAfterTime));*/
        }
        return result;
    }

    // for preventing crash when using with NeedSleep (and any mod that call this in the main menu)
    bool (*getVisible_orig)(DialogueWindow* thisptr);
    bool getVisible_hook(DialogueWindow* thisptr)
    {
        if (!thisptr) return false;
        return getVisible_orig(thisptr);
    }

    void Init()
    {
        modPath = GetCurrentDLLDirectory();
        Localization::init();
        ModSettings::getSingleton().loadFromFile();
        SquadAutonomySettings::getSingleton();
    }

}


//void (*load)(class SaveManager*, const std::string &);
__declspec(dllexport) void startPlugin()
{
    
    auto versionInfo = KenshiLib::GetKenshiVersion();
    auto platform = versionInfo.GetPlatform();
    auto version = versionInfo.GetVersion();
    auto baseAddr = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));

    if (version == "1.0.65")
    {
        if (platform == 1)
        {
            *(uintptr_t*)&SquadAutonomy::EscMenu_openedOtherWindows = baseAddr + 0x916250;
            *(uintptr_t*)&SquadAutonomy::rentedBeds = baseAddr+0x212db18;
            *(uintptr_t*)&SquadAutonomy::Package_WanderingTrader_signalStart = baseAddr + 0x286960;
            *(uintptr_t*)&SquadAutonomy::_MainColorCode = baseAddr + 0x01f48238;
            *(uintptr_t*)&SquadAutonomy::_GreyedColorCode = baseAddr + 0x01f48350;
            *(uintptr_t*)&SquadAutonomy::taskTypetaskData = baseAddr + 0x1ce80f0;
            *(uintptr_t*)&SquadAutonomy::getTaskDataConst = baseAddr + 0x283F40;
            *(uintptr_t*)&SquadAutonomy::CharBody_NV_setCurrentAction = baseAddr + 0x5C6430;
            *(uintptr_t*)&SquadAutonomy::CharMovement_NV_setDestination = baseAddr + 0x6607E0;
            *(uintptr_t*)&SquadAutonomy::Task_ManTheGate_Update = baseAddr + 0x342bf0;
            *(uintptr_t*)&SquadAutonomy::Task_ManTheGate_FindGate = baseAddr + 0x338650;
            *(uintptr_t*)&SquadAutonomy::Task_MoveToDoor_Gate_ChooseSide_gatePosition = baseAddr + 0x332ef0;
            *(uintptr_t*)&SquadAutonomy::Task_OpenDoor_StartAction = baseAddr + 0x336fc0;
            *(uintptr_t*)&SquadAutonomy::Task_OpenDoor_Update = baseAddr + 0x3370b0;
            *(uintptr_t*)&SquadAutonomy::GOAPTaskMgr_createNewTask = baseAddr + 0x32eb90;
            //*(uintptr_t*)&SquadAutonomy::getTaskData = baseAddr + 0x519c10; //TaskRepertoire
            //*(uintptr_t*)&TaskPathfinder_Node_solve = baseAddr + 0x50ED70;
            //*(uintptr_t*)&TaskRepertoire_hasTask = baseAddr + 0x32dbb0;
            //*(uintptr_t*)&load = baseAddr + 0x47AC10;
        }
        else if (platform == 0)
        {
            *(uintptr_t*)&SquadAutonomy::EscMenu_openedOtherWindows = baseAddr + 0x915970;
            *(uintptr_t*)&SquadAutonomy::rentedBeds = baseAddr + 0x212BA58;
            *(uintptr_t*)&SquadAutonomy::Package_WanderingTrader_signalStart = baseAddr + 0x2864F0;
            *(uintptr_t*)&SquadAutonomy::_MainColorCode = baseAddr + 0x01f46248;
            *(uintptr_t*)&SquadAutonomy::_GreyedColorCode = baseAddr + 0x01f46360;
            *(uintptr_t*)&SquadAutonomy::taskTypetaskData = baseAddr + 0x1ce60F0;
            *(uintptr_t*)&SquadAutonomy::getTaskDataConst = baseAddr + 0x283AD0;
            *(uintptr_t*)&SquadAutonomy::CharBody_NV_setCurrentAction = baseAddr + 0x5C6740;
            *(uintptr_t*)&SquadAutonomy::CharMovement_NV_setDestination = baseAddr + 0x660AF0;
            *(uintptr_t*)&SquadAutonomy::Task_ManTheGate_Update = baseAddr + 0x3427a0;
            *(uintptr_t*)&SquadAutonomy::Task_ManTheGate_FindGate = baseAddr + 0x3381e0;
            *(uintptr_t*)&SquadAutonomy::Task_MoveToDoor_Gate_ChooseSide_gatePosition = baseAddr + 0x332a80;
            *(uintptr_t*)&SquadAutonomy::Task_OpenDoor_StartAction = baseAddr + 0x336b50;
            *(uintptr_t*)&SquadAutonomy::Task_OpenDoor_Update = baseAddr + 0x336c40;
            *(uintptr_t*)&SquadAutonomy::GOAPTaskMgr_createNewTask = baseAddr + 0x32E720;
            //*(uintptr_t*)&SquadAutonomy::getTaskData = baseAddr + 0x519F20;
            //*(uintptr_t*)&load = baseAddr + 0x47AD00;
            //*(uintptr_t*)&TaskPathfinder_Node_solve = baseAddr + 0x50F080;
            //*(uintptr_t*)&TaskRepertoire_hasTask = baseAddr + 0x32D740;

        }
    }
    SquadAutonomy::Init();
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&ForgottenGUI::changeFontSize), &SquadAutonomy::ForgottenGUI_changeFontSize_hook, &SquadAutonomy::ForgottenGUI_changeFontSize_orig))
        ErrorLog("Could not add ForgottenGUI::changeFontSize hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&MainBarGUI::_NV_update), &SquadAutonomy::_NV_update_hook, &SquadAutonomy::_NV_update_orig))
        ErrorLog("Could not add MainBarGUI::_NV_update constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&MainBarGUI::_CONSTRUCTOR), &SquadAutonomy::MainbarGUICONSTRUCTOR_hook, &SquadAutonomy::MainbarGUICONSTRUCTOR_orig))
        ErrorLog("Could not add &MainBarGUI::_CONSTRUCTOR constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&MainBarGUI::_DESTRUCTOR), &SquadAutonomy::MainbarGUIDESTRUCTOR_hook, &SquadAutonomy::MainbarGUIDESTRUCTOR_orig))
        ErrorLog("Could not add &MainBarGUI::_DESTRUCTOR hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&MainBarGUI::_NV_autoChangeSelectedObject), &SquadAutonomy::_NV_autoChangeSelectedObject_hook, &SquadAutonomy::_NV_autoChangeSelectedObject_orig))
        ErrorLog("Could not add MainBarGUI::_NV_autoChangeSelectedObject constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&MainBarGUI::tabPlatoonChange), &SquadAutonomy::tabPlatoonChange_hook, &SquadAutonomy::tabPlatoonChange_orig))
        ErrorLog("Could not add MainBarGUI::tabPlatoonChange constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&PlayerInterface::cycleSquad), &SquadAutonomy::cycleSquad_hook, &SquadAutonomy::cycleSquad_orig))
        ErrorLog("Could not add PlayerInterface::cycleSquad constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&DialogueWindow::getVisible), &SquadAutonomy::getVisible_hook, &SquadAutonomy::getVisible_orig))
        ErrorLog("Could not add DialogueWindow::getVisible constructor hook!");

    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OptionsWindow::create), &SquadAutonomy::OptionsWindow_create_hook, &SquadAutonomy::OptionsWindow_create_orig))
        ErrorLog("Could not add OptionsWindow::create constructor hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OptionsWindow::saveOptions), &SquadAutonomy::saveOptions_hook, &SquadAutonomy::saveOptions_orig))
        ErrorLog("Could not add OptionsWindow::saveOptions constructor hook!");*/
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OptionsWindow::closeButton), &SquadAutonomy::closeButton_hook, &SquadAutonomy::closeButton_orig))
        ErrorLog("Could not add OptionsWindow::closeButton constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&GameWorld::initialisation), &SquadAutonomy::initialisation_hook, &SquadAutonomy::initialisation_orig))
        ErrorLog("Could not add GameWorld::initialisation constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&Character::_NV_threadedUpdate), &SquadAutonomy::_NV_threadedUpdate_hook, &SquadAutonomy::_NV_threadedUpdate_orig))
        ErrorLog("Could not add Character::_NV_threadedUpdate constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&Character::wantsToEatNow), &SquadAutonomy::wantsToEatNow_hook, &SquadAutonomy::wantsToEatNow_orig))
        ErrorLog("Could not add Character::wantsToEatNow constructor hook!");

    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(SquadAutonomy::CharMovement_NV_setDestination, &SquadAutonomy::_NV_setDestination_hook, &SquadAutonomy::_NV_setDestination_orig))
        ErrorLog("Could not add CharMovement::_NV_setDestination hook!");

    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::saveGame), &SquadAutonomy::saveGame_hook, &SquadAutonomy::saveGame_orig))
		ErrorLog("Could not add SaveManager::saveGame hook!");
	if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::loadGame), &SquadAutonomy::loadGame_hook, &SquadAutonomy::loadGame_orig))
		ErrorLog("Could not add SaveManager::loadGame hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::execute), &SquadAutonomy::execute_hook, &SquadAutonomy::execute_orig))
        ErrorLog("Could not add SaveManager::execute hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SquadManagementScreen::SquadCellView::update), &SquadAutonomy::SquadCellView_update_hook, &SquadAutonomy::SquadCellView_update_orig))
        ErrorLog("Could not add SquadManagementScreen::SquadCellView::update hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SquadManagementScreen::removeSquad), &SquadAutonomy::removeSquad_hook, &SquadAutonomy::removeSquad_orig))
        ErrorLog("Could not add SquadManagementScreen::removeSquad hook!");

    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&Ownerships::setHomeBuilding), &SquadAutonomy::setHomeBuilding_hook, &SquadAutonomy::setHomeBuilding_orig))
        ErrorLog("Could not add Tasker::score hook!");*/
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&UseableStuff::_NV_couldIOperate), &SquadAutonomy::_NV_couldIOperate_hook, &SquadAutonomy::_NV_couldIOperate_orig))
        ErrorLog("Could not add UseableStuff::_NV_couldIOperate hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&Tasker::score), &SquadAutonomy::score_hook, &SquadAutonomy::score_orig))
        ErrorLog("Could not add Tasker::score hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&TaskData::_isRequirementsComplete), &SquadAutonomy::_isRequirementsComplete_hook, &SquadAutonomy::_isRequirementsComplete_orig))
        ErrorLog("Could not add TaskData::_isRequirementsComplete hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&TaskData::runTargetFind), &SquadAutonomy::runTargetFind_hook, &SquadAutonomy::runTargetFind_orig))
        ErrorLog("Could not add runTargetFind hook!");*/

    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::_NV_scoreGetOutOfBed), &SquadAutonomy::_NV_scoreGetOutOfBed_hook, &SquadAutonomy::_NV_scoreGetOutOfBed_orig))
        ErrorLog("Could not add AI::_NV_scoreGetOutOfBed hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::_NV_scoreGoToBed), &SquadAutonomy::_NV_scoreGoToBed_hook, &SquadAutonomy::_NV_scoreGoToBed_orig))
        ErrorLog("Could not add AI::_NV_scoreGoToBed hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::findMineToWorkAt), &SquadAutonomy::findMineToWorkAt_hook, &SquadAutonomy::findMineToWorkAt_orig))
        ErrorLog("Could not add AI::findMineToWorkAt hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::findKOIntruder_town), &SquadAutonomy::findKOIntruder_town_hook, &SquadAutonomy::findKOIntruder_town_orig))
        ErrorLog("Could not add AI::findKOIntruder_town hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::AIResultsCacher::runTargetFinder), &SquadAutonomy::runTargetFinder_hook, &SquadAutonomy::runTargetFinder_orig))
        ErrorLog("Could not add AI::AIResultsCacher::runTargetFinder hook!");

    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OrdersReceiver::clearCurrentGoal), &SquadAutonomy::clearCurrentGoal_hook, &SquadAutonomy::clearCurrentGoal_orig))
        ErrorLog("Could not add OrdersReceiver::clearCurrentGoal hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::periodicUpdate), &SquadAutonomy::periodicUpdate_hook, &SquadAutonomy::periodicUpdate_orig))
        ErrorLog("Could not add AITaskSytem::periodicUpdate hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::choosePermaJob), &SquadAutonomy::choosePermaJob_hook, &SquadAutonomy::choosePermaJob_orig))
        ErrorLog("Could not add AITaskSytem::choosePermaJob hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::currentActionChecks), &SquadAutonomy::currentActionChecks_hook, &SquadAutonomy::currentActionChecks_orig))
        ErrorLog("Could not add AITaskSytem::currentActionChecks hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::update4Frame), &SquadAutonomy::update4Frame_hook, &SquadAutonomy::update4Frame_orig))
        ErrorLog("Could not add AITaskSytem::update4Frame hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::runGoals), &SquadAutonomy::runGoals_hook, &SquadAutonomy::runGoals_orig))
        ErrorLog("Could not add runGoals hook!");*/
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::chooseGoalFrom), &SquadAutonomy::chooseGoalFrom_hook, &SquadAutonomy::chooseGoalFrom_orig))
        ErrorLog("Could not add chooseGoalFrom hook!");*/
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::chooseGoal), &SquadAutonomy::chooseGoal_hook, &SquadAutonomy::chooseGoal_orig))
        ErrorLog("Could not add AITaskSytem::chooseGoal hook!");*/
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::runGOAP) , &SquadAutonomy::runGOAP_hook, &SquadAutonomy::runGOAP_orig))
        ErrorLog("Could not add AITaskSytem::runGOAP hook!");*/
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::_NV_setCurrentGoal), &SquadAutonomy::_NV_setCurrentGoal_hook, &SquadAutonomy::_NV_setCurrentGoal_orig))
        ErrorLog("Could not add AITaskSytem::_NV_setCurrentGoal hook!");*/
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::setTaskExpiryTimer), &SquadAutonomy::setTaskExpiryTimer_hook, &SquadAutonomy::setTaskExpiryTimer_orig))
        ErrorLog("Could not add AITaskSytem::setTaskExpiryTimer hook!");*/

    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(SquadAutonomy::Package_WanderingTrader_signalStart, &SquadAutonomy::Package_WanderingTrader_signalStart_hook, &SquadAutonomy::Package_WanderingTrader_signalStart_orig))
        ErrorLog("Could not add Wandering::signal_start hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(SquadAutonomy::Task_MoveToDoor_Gate_ChooseSide_gatePosition, &SquadAutonomy::Task_MoveToDoor_Gate_ChooseSide_gatePosition_hook, &SquadAutonomy::Task_MoveToDoor_Gate_ChooseSide_gatePosition_orig))
        ErrorLog("Could not add Task_MoveToDoor_Gate_ChooseSide_gatePosition hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(SquadAutonomy::Task_ManTheGate_Update, &SquadAutonomy::Task_ManTheGate_Update_hook, &SquadAutonomy::Task_ManTheGate_Update_orig))
        ErrorLog("Could not add Task_ManTheGate_Update hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(SquadAutonomy::Task_ManTheGate_FindGate, &SquadAutonomy::Task_ManTheGate_FindGate_hook, &SquadAutonomy::Task_ManTheGate_FindGate_orig))
        ErrorLog("Could not add Task_ManTheGate_FindGate hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(SquadAutonomy::Task_OpenDoor_StartAction, &SquadAutonomy::Task_OpenDoor_StartAction_hook, &SquadAutonomy::Task_OpenDoor_StartAction_orig))
        ErrorLog("Could not add Task_OpenDoor_StartAction hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(SquadAutonomy::Task_OpenDoor_Update, &SquadAutonomy::Task_OpenDoor_Update_hook, &SquadAutonomy::Task_OpenDoor_Update_orig))
        ErrorLog("Could not add Task_OpenDoor_Update hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(SquadAutonomy::EscMenu_openedOtherWindows, &SquadAutonomy::EscMenu_openedOtherWindows_hook, &SquadAutonomy::EscMenu_openedOtherWindows_orig))
        ErrorLog("Could not add EscMenu hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(TaskPathfinder_Node_solve, &SquadAutonomy::TaskPathfinder_Node_solve_hook, &SquadAutonomy::TaskPathfinder_Node_solve_orig))
        ErrorLog("Could not add TaskPathfinder_Node_solve hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(TaskRepertoire_hasTask, &SquadAutonomy::TaskRepertoire_hasTask_hook, &SquadAutonomy::TaskRepertoire_hasTask_orig))
        ErrorLog("Could not add TaskRepertoire_hasTask hook!");*/
    KenshiLib::ApplyQueuedHooks();
    DebugLog("Mod started");
}

