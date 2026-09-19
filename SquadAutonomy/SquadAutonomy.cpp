#include "SquadAutonomy.h"
#include "SquadAutonomyPanel.h"
#include "SquadAutonomySettings.h"
#include "SquadAutonomyModOptions.h"

#include <Debug.h>

#include <ogre/OgreStringConverter.h>
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
#include <kenshi/FactionRelations.h>
#include <kenshi/Platoon.h>
#include <kenshi/StateBroadcastData.h>
#include <kenshi/SaveManager.h>
#include <kenshi/SaveFileSystem.h>
#include <kenshi/SharedKing.h>
#include <kenshi/Town.h>
#include <kenshi/Building/Building.h>
#include <kenshi/Building/UseableStuff.h>

#include <mygui/MyGUI_Gui.h>
#include <mygui/MyGUI_Window.h>
#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_Delegate.h>

#include <core/Functions.h>
#include <fstream>

#include "lektorExtension.h"

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
    ogre_unordered_map<TaskType, TaskData*>::type* taskTypetaskData = nullptr;
    const TaskData* (*getTaskDataConst)(TaskType key) = nullptr;
    std::map<hand, float>* rentedBeds = nullptr;
    std::string* _MainColorCode = nullptr;
    bool (*EscMenu_openedOtherWindows)(class EscMenu*) = nullptr;
    bool (*Package_WanderingTrader_signalStart)(AIPackage*) = nullptr;
    bool showOnMain = true;
    bool lockPosition = true;
    bool showInSquad = true;
    bool enableLogging = false;

    DatapanelGUI* optionsSettings;
    const float defaultBtnWidth = 1.50;
    const float defaultBtnHeight = 3.58;
    const float defaultBtnLeft = 0.69;
    const float defaultBtnTop = 0.788;
    const float defaultBtnFontSize = 12.0;
    float btnWidth = 1.50;
    float btnHeight = 3.58;
    float btnLeft = 0.69;
    float btnTop = 0.788;
    float btnFontSize = 12.0;
    std::string logFileName = "SquadAutonomy.log";
    std::string logBakFileName = "SquadAutonomyLog.bak";
    std::string saveName = "SquadAutonomy.save";
    std::string modPath = "";
    std::string logPath = "";
    std::string logBakPath = "";
    std::ofstream logFile;
    std::string settingsSavePath = "";
    static int lc = 0;
    const int maxLC = 2000;
    //static int bc = 0;
    //const int buffer = 100;
    bool shouldSave = false;
    bool shouldLoad = false;
    bool loadNextCall = false;

    MyGUI::Button* autBtn = nullptr;
    MyGUI::Window* autBtnWindow = nullptr;
    MyGUI::IntPoint mDragStart(0, 0);
    MyGUI::IntPoint mWindowStart(0, 0);
    bool mClicked = false;
    bool mDragged = false;

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

    std::string GetCurrentDLLDirectory() {
        char path[MAX_PATH];
        HMODULE hModule = NULL;

        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&GetCurrentDLLDirectory, &hModule)) {

            GetModuleFileNameA(hModule, path, MAX_PATH);
            std::string fullPath(path);

            size_t lastSlash = fullPath.find_last_of("\\/");
            if (std::string::npos != lastSlash) {
                return fullPath.substr(0, lastSlash + 1);
            }
            return fullPath;
        }
        return "";
    }

    void Log(std::string line)
    {
        if (!enableLogging) return;
        if (!logFile.is_open())
        {
            logFile.open(logPath, std::fstream::out | std::fstream::app);
            if (!logFile.is_open())
            {
                DebugLog("Log: Cannot open file");
                return;
            }
        }
        if (lc > maxLC)
        {
            logFile.close();
            std::remove(logBakPath.c_str());
            if (std::rename(logPath.c_str(), logBakPath.c_str()) != 0)
            {
                DebugLog("Error backing up logfile");
                return;
            }
            logFile.open(logPath, std::fstream::out | std::fstream::app);
            lc = 0;
        }
        logFile << Ogre::StringConverter::toString(ou->timeStamper.stampTime()) << ' ';
        logFile << line << '\n';
        lc++;
        logFile.flush();
    }
    
    bool SetAI(Platoon* platoon, std::map<int, lektor<GameData*>> aiPackages, bool endAction)
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
            if (endAction) obj->getBody()->_endAction();
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
            for (int i = 0; i < data.count; ++i)
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
            if (endAction) obj->getBody()->_endAction();
        }
        return true;
    }


    UseableStuff* FindOptimalBed(Character* character, bool usePaidBeds, lektor<UseableStuff*> beds, bool ownFactionOnly = false)
    {
        float minDist = std::numeric_limits<float>::max();
        float maxEfficiency = std::numeric_limits<float>::lowest();
        float minCost = std::numeric_limits<float>::max();
        UseableStuff* optimalBed = nullptr;
        Faction* ownFaction = character->getFaction();

        for (uint32_t i = 0; i < beds.size(); ++i)
        {
            UseableStuff* b = beds[i];
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

            float distanceScore = character->ai->scoreDistanceTo(b, false);

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
        lektor<UseableStuff*> beds;
        
        TownBase* currentTown = character->getCurrentTownLocation();
        if (currentTown)
        {
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
                if (bed) lektorEx::push_back_unique(beds, bed);
            }
            if (currentTown->isTown() && currentTown->isTown()->playerHasBuildingsInThisTown)
            {
                optimalBed = FindOptimalBed(character, false, beds, true);
            }
            if (optimalBed) return optimalBed->getHandle();

        }
        Building* currentBuilding = nullptr;
        if (character->isInsideBuilding)
        {
            currentBuilding = character->isInsideBuilding.getBuilding();
            BuildingFunction bf = BF_BED;
            if (race && race->robot)
            {
                bf = BF_SKELETON_BED;
            }
            lektor<Building*> bedBuildings;
            currentBuilding->findAllFurnitureWithFunction(bedBuildings, bf);
            for (int i = 0; i < bedBuildings.size(); ++i)
            {
                if (!bedBuildings[i]) continue;
                UseableStuff* bed = bedBuildings[i]->getUseableStuff();
                if (bed) lektorEx::push_back_unique(beds, bed);
            }
            optimalBed = FindOptimalBed(character, usePaidBeds, beds);
        }
        if (optimalBed) return optimalBed->getHandle();
        else return nullptr;
        
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
        if (SquadAutonomyModOptions::initialized)
            SquadAutonomyModOptions::getSingletonPtr()->create();

    }

    void ShowModOptions(MyGUI::Widget* sender)
    {
        auto modOptions = SquadAutonomyModOptions::getSingletonPtr();
        if (modOptions && modOptions->initialized)
        {
            if (modOptions->isVisible())
            {
                modOptions->hide();
            }
            else
            {
                modOptions->show();
            }
        }
    }

    void (*OptionsWindow_create_orig)(OptionsWindow* thisptr);
    void OptionsWindow_create_hook(OptionsWindow* thisptr)
    {
        OptionsWindow_create_orig(thisptr);
        SquadAutonomyModOptions* modOptions = SquadAutonomyModOptions::getSingletonPtr();
        if (!modOptions) return;
        //DebugLog("mod options initialized!");
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
            if (modLine)
            {
                modOptions->setOptionsWindow(thisptr);
                //DebugLog(Ogre::StringConverter::toString(modLine->getNumWidgets()));
                //DebugLog(modLine->w1->getCaption().asUTF8());
                //DebugLog(modLine->w2->getCaption().asUTF8());
                float right = static_cast<float>(modLine->w2->getRight() - modLine->w2->getTextSize().width)/settingsPanel->getWidget()->getSize().width;
                float left = right - 0.1;
                float top = static_cast<float>(modLine->w2->getTop())/settingsPanel->getWidget()->getSize().height;
                float height = static_cast<float>(modLine->w2->getHeight()) / settingsPanel->getWidget()->getSize().height;
                //DebugLog("left: " + Ogre::StringConverter::toString(left) + " top: " + Ogre::StringConverter::toString(top) + " height " + Ogre::StringConverter::toString(height));
                auto btn = settingsPanel->getWidget()->createWidgetReal<MyGUI::Button>("Kenshi_Button1", left, top, 0.1, height, MyGUI::Align::Top | MyGUI::Align::Left, "SquadAutonomySettingsBtn");
                btn->setCaption("Settings");
                btn->eventMouseButtonClick += MyGUI::newDelegate(ShowModOptions);
            }
        }
    }

    void (*saveOptions_orig)(OptionsWindow* thisptr);
    void saveOptions_hook(OptionsWindow* thisptr)
    {
        saveOptions_orig(thisptr);
        SquadAutonomyModOptions::getSingletonPtr()->saveOptionsSettings();
    }

    /*void (*hide_orig)(OptionsWindow* thisptr);
    void hide_hook(OptionsWindow* thisptr)
    {
        bool modOptionsVisible = false;
        auto modOptions = SquadAutonomyModOptions::getSingletonPtr();
        if (modOptions)
        {
            modOptionsVisible = modOptions->isVisible();
        }
        hide_orig(thisptr);
        if (modOptions && modOptionsVisible && !modOptions->isVisible())
        {
            modOptions->show();
        }
    }*/

    void (*closeButton_orig)(OptionsWindow* thisptr, MyGUI::Widget* _sender);
    void closeButton_hook(OptionsWindow* thisptr, MyGUI::Widget* _sender)
    {
        bool modOptionsVisible = false;
        auto modOptions = SquadAutonomyModOptions::getSingletonPtr();
        if (modOptions)
        {
            modOptionsVisible = modOptions->isVisible();
        }
        closeButton_orig(thisptr, _sender);
        if (modOptions && modOptionsVisible && !modOptions->isVisible())
        {
            modOptions->show();
        }
    }

    class AutonomyButton
    {
    public:
        MyGUI::Button* button;
        SquadManagementScreen::SquadCellView* cellview;
        AutonomyButton(MyGUI::Button* b, SquadManagementScreen::SquadCellView* c)
        {
            button = b;
            cellview = c;
            button->eventMouseButtonClick += MyGUI::newDelegate(this, &AutonomyButton::OnClick);
        }
        void OnClick(MyGUI::WidgetPtr sender)
        {
            OpenSquadAutonomyPanel(cellview->squad->platoon->me);
        }
    private:
    };
    lektor<AutonomyButton*> autButtons;

    void onPressed(
        MyGUI::Widget* sender,
        int left,
        int top,
        MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left)
            return;

        mClicked = true;

        mDragStart = MyGUI::IntPoint(left, top);
        mWindowStart = autBtn->getPosition();
        sender->_setRootMouseFocus(true);
    }

    void onDrag(
        MyGUI::Widget* sender,
        int left,
        int top,
        MyGUI::MouseButton id)
    {
        if (lockPosition || !mClicked || id != MyGUI::MouseButton::Left)
            return;

        const int dx = left - mDragStart.left;
        const int dy = top - mDragStart.top;
        if (dx != 0 || dy != 0)
        {
            mDragged = true;
            btnLeft = static_cast<float>(mWindowStart.left + dx) / autBtn->getParentSize().width;
            btnTop = static_cast<float>(mWindowStart.top + dy) / autBtn->getParentSize().height;
            autBtn->setRealPosition(
                btnLeft,
                btnTop
            );
            //btnLeft = static_cast<float>(autBtn->getLeft()) / autBtn->getParentSize().width;
            //btnTop = static_cast<float>(autBtn->getTop()) / autBtn->getParentSize().height;
        }
    }

    void onReleased(MyGUI::Widget* sender,
        int left,
        int top,
        MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left)
            return;
        if (mClicked && !mDragged) OpenSquadAutonomyPanelMainBar();
        mClicked = false;
        mDragged = false;
        sender->_setRootMouseFocus(false);
    }

    MainBarGUI* (*MainbarGUICONSTRUCTOR_orig)(MainBarGUI* thisptr);
    MainBarGUI* MainbarGUICONSTRUCTOR_hook(MainBarGUI* thisptr)
    {
        MainBarGUI* orig = MainbarGUICONSTRUCTOR_orig(thisptr);
        autBtn = orig->getWidget()->createWidgetReal<MyGUI::Button>("Kenshi_Button1", btnLeft, btnTop, btnWidth/100.0, btnHeight/100.0, MyGUI::Align::Center, "AUTBtn");
        autBtn->setCaption("AUT");
        autBtn->setFontHeight(btnFontSize);
        //autBtn->eventMouseButtonClick += MyGUI::newDelegate(OpenSquadAutonomyPanelMainBar);
        autBtn->eventMouseButtonPressed +=
            MyGUI::newDelegate(onPressed);

        autBtn->eventMouseDrag +=
            MyGUI::newDelegate(onDrag);
        
        autBtn->eventMouseButtonReleased +=
            MyGUI::newDelegate(onReleased);
        
        autBtn->setDepth(0);



        /*MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();

        autBtnWindow = orig->getWidget()->createWidgetReal<MyGUI::Window>(
            "",
            0.655, 0.727,
            0.039, 0.065,
            MyGUI::Align::Center,
            //"Modal",
            "AUTBtnWindow"
        );

        autBtnWindow->changeWidgetSkin("Kenshi_BuildPanelSkin");
        autBtnWindow->setMovable(false);
        autBtnWindow->setDepth(0);
        autBtnWindow->setSnap(true);
        MyGUI::Widget* dragArea = autBtnWindow->createWidgetReal<MyGUI::Widget>(
            "PanelEmpty",
            0, 0,
            1.0, 1.0,
            MyGUI::Align::Stretch,
            "DragArea"
        );
        dragArea->eventMouseButtonPressed +=
            MyGUI::newDelegate(onDragPressed);

        dragArea->eventMouseDrag +=
            MyGUI::newDelegate(onDrag);
        MyGUI::Button* btn = dragArea->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.2, 0.2, 0.6, 0.6, MyGUI::Align::Center, "AUTBtnWindowBtn");
        btn->setCaption("AUT");
        //btn->setCaption("AUT");
        //btn->eventMouseButtonClick += MyGUI::newDelegate(OpenSquadAutonomyPanelMainBar);*/
        if (!showOnMain)
        {
            autBtn->setVisible(false);
            //autBtnWindow->setVisible(false);
        }
        return orig;
    }

    void (*_NV_update_orig)(MainBarGUI* thisptr);
    void _NV_update_hook(MainBarGUI* thisptr)
    {
        _NV_update_orig(thisptr);
        if (autBtn)
        {
            if (showOnMain)
            {
                autBtn->setVisible(true);
                autBtn->setRealPosition(btnLeft, btnTop);
                autBtn->setRealSize(btnWidth / 100.0, btnHeight / 100.0);
                autBtn->setFontHeight(btnFontSize);
                autBtn->setDepth(0);
            }
            else
            {
                autBtn->setVisible(false);
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

    
    void (*SquadCellView_update_orig)(SquadManagementScreen::SquadCellView* thisptr, const MyGUI::IBDrawItemInfo& _info, SquadManagementScreen::SquadData* _data);
    void SquadCellView_update_hook(SquadManagementScreen::SquadCellView* thisptr, const MyGUI::IBDrawItemInfo& _info, SquadManagementScreen::SquadData* _data)
    {
        SquadCellView_update_orig(thisptr, _info, _data);

        bool createNew = true;
        for (int i = 0; i < autButtons.size(); ++i)
        {
            if (autButtons[i]->cellview == thisptr)
            {
                createNew = false;
            }
            autButtons[i]->button->setVisible(showInSquad);
        }
        if(createNew)
        {
            //DebugLog("Create AUT button");
            MyGUI::Widget* parent = thisptr->txtName->getParent();
            int left = thisptr->txtName->getRight();
            int width = thisptr->txtSquadSize->getLeft() - left;
            int top = thisptr->txtName->getTop();
            int height = thisptr->txtName->getHeight();
            MyGUI::Button* autonomyButton = parent->createWidgetReal<MyGUI::Button>("Kenshi_Button1", static_cast<float>(left) / parent->getWidth() + 0.05,
                static_cast<float>(top) / parent->getHeight(), static_cast<float>(width) / parent->getWidth() - 0.1, static_cast<float>(height) / parent->getHeight(), MyGUI::Align::Center, "AutonomyButton");
            autonomyButton->setCaption("AUT");
            autonomyButton->setVisible(showInSquad);
            AutonomyButton* autButton = new AutonomyButton(autonomyButton, thisptr);
            lektorEx::push_back_unique(autButtons, autButton);
        }
    }


    void (*removeSquad_orig)(SquadManagementScreen* thisptr, SquadManagementScreen::SquadData* squad);
    void removeSquad_hook(SquadManagementScreen* thisptr, SquadManagementScreen::SquadData* squad)
    {
        auto settings = SquadAutonomySettings::getSingletonPtr();
        Platoon* platoon = nullptr;
        if (squad->platoon) platoon = squad->platoon->me;
        if (settings && platoon)
        {
            settings->removeSquadSettings(platoon);
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
        settingsSavePath = location + name + '/' + saveName;
        auto settings = SquadAutonomySettings::getSingletonPtr();
        for (int i = 0; i < settings->squadSettings.size(); ++i)
        {
            auto squadSettings = settings->squadSettings[i];
            if (!squadSettings->isEnabled()) continue;
            ResetAI(squadSettings->getSquad());
            squadSettings->unassignSquadHome();
        }
        
        int result = saveGame_orig(thisptr, location, name);

        if (result == 0)
        {
            shouldSave = true;
        }
        return result;
    }

    int (*loadGame_orig)(SaveManager* thisptr, const std::string& location, const std::string& name);
    int loadGame_hook(SaveManager* thisptr, const std::string& location, const std::string& name)
    {
        int result = loadGame_orig(thisptr, location, name);
        if (result != 0) return result;
        shouldLoad = true;
        SquadAutonomySettings::getSingletonPtr()->initialized = false;
        settingsSavePath = location + name + '/' + saveName;
        return result;
    }

    void (*execute_orig)(SaveManager* thisptr);
    void execute_hook(SaveManager* thisptr)
    {
        execute_orig(thisptr);
        if (shouldSave)
        {
            auto settings = SquadAutonomySettings::getSingletonPtr();
            if (settings->saveSettings(settingsSavePath))
            {
                shouldSave = false;
                SquadAutonomySettings::getSingletonPtr()->loadSettings(settingsSavePath);
            }
            SquadAutonomyModOptions::getSingletonPtr()->saveOptionsSettings();
        }
        else if (shouldLoad)
        {
            if (!ou->isLoadingFromASaveGame())
            {
                if (SquadAutonomySettings::getSingletonPtr()->loadSettings(settingsSavePath))
                {
                    shouldLoad = false;
                    SquadAutonomySettings::getSingletonPtr()->initialized = true;
                }
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
            //DebugLog("set town to: " + currentTown->getKnownName());
            squad->getOwnerships()->setHomeTown(currentTown, squad->getSquadType());
            return;
        }
        else
        {
            //DebugLog("no town");
            squad->getOwnerships()->setHomeBuilding(nullptr, squad->getSquadType());
            squad->getOwnerships()->setHomeTown(nullptr, squad->getSquadType());
        }
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
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(ai->getPlatoon());
            character = ai->getCharacter();
        }

        if (character && settings && settings->isEnabled() && thisptr)
        {
            TaskType type = key.key();
            Log("RuntargetFind TaskType: " + Ogre::StringConverter::toString(static_cast<int>(type)));
            if (type == GO_HOME_AND_GO_TO_BED || type == FIND_BED_AND_PUT_IN)
            {
                out = FindOptimalBed(character, settings->getUsePaidBeds());
                if (out) return 1.0;
                else return 0.0;
            }
            /*if (key == MAN_THE_GATE)
            {
                DebugLog("Man the gate targetfind");
            }*/
            Log("EndRuntargetFind");
            //DebugLog("runtargetFind");
            /*UseableStuff* bedCandidate = nullptr;
            Building* building = nullptr;
            if (out) building = out.getBuilding();
            if (building) bedCandidate = building->getUseableStuff();
            if (bedCandidate)
            {
                int costToUse = bedCandidate->getCostToUse(character);
                if (costToUse < 0)
                {
                    out = nullptr;
                    return 0.0f;
                }
                if (costToUse > 0 && !settings->getUsePaidBeds())
                {
                    out = nullptr;
                    return 0.0f;
                }
            }*/
        }
        return runTargetFinder_orig(thisptr, func, key, out);
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
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);
        }
        if (settings && settings->isEnabled())
        {
            Log("PermaJob");
            if (urgentOnes && _jobsEnabled)
            {
                if (settings->isRestTime())
                {
                    _jobsEnabled = false;
                }
                if (faction && settings->getPlayerInterface())
                {
                    origAutoSit = settings->getPlayerInterface()->aiOptions.autoSit;
                    faction->isPlayer = settings->getPlayerInterface();
                    faction->isPlayer->aiOptions.autoSit = false;
                }
            }
        }
        /*if (settings && settings->isEnabled())
        {
            if (orderedGoals.size() > 0 && character == gui->selectedObject.getCharacter())
            {
                for (auto it = orderedGoals.begin(); it != orderedGoals.end(); ++it)
                {
                    if (it->second)
                    {
                        Log("Choose PermaJob: " + it->second->getDescription() + " score: " + Ogre::StringConverter::toString(it->first));
                    }
                }
            }
        }*/

        choosePermaJob_orig(thisptr, orderedGoals, alreadyHasGoal, urgentOnes, _jobsEnabled);
        
        if (settings && settings->isEnabled())
        {
            if (faction && faction->isPlayer && settings->getPlayerInterface())
            {
                faction->isPlayer->aiOptions.autoSit = origAutoSit;
                faction->isPlayer = nullptr;
            }
            Log("End PermaJob");
        }
        /*if (settings && settings->isEnabled())
        {
            if (orderedGoals.size() > 0 && character == gui->selectedObject.getCharacter())
            {
                for (auto it = orderedGoals.begin(); it != orderedGoals.end(); ++it)
                {
                    if (it->second)
                    {
                        Log("After Choose PermaJob: " + it->second->getDescription() + " score: " + Ogre::StringConverter::toString(it->first));
                    }
                }
            }
        }*/
    }




    void (*chooseGoal_orig)(AITaskSytem* thisptr, bool timedLockOnCurrentGoal);
    void chooseGoal_hook(AITaskSytem* thisptr, bool timedLockOnCurrentGoal)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        if (thisptr)  character = thisptr->character;
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);
        if (settings && settings->isEnabled())
        {
            Log("chooseGoal");
            if (!settings->hasHome())
            {
                SetNearestFriendlyTownAsHome(platoon);
            }
            else
            {
                if (settings->isRestTime())
                {
                    settings->assignSquadHome(true);
                }
                else
                {
                    if (!settings->assignSquadHome(false))
                    {
                        settings->assignSquadHome(true);
                    }
                }
            }
        }
        chooseGoal_orig(thisptr, timedLockOnCurrentGoal);
        if (settings && settings->isEnabled())
        {
            Log("chooseGoal End");
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
        if (platoon) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);
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

        clearCurrentGoal_orig(thisptr, force);
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
        if (platoon) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);

        if (settings && settings->isEnabled())
        {
            Log("PeriodicUpdate");
            
            TaskData* data = taskTypetaskData->find(RELAX_IN_TOWN_PACKAGE)->second;
            data->setDurationBased(1.0, 4.0, false);
            data = taskTypetaskData->find(GO_HOME_AND_GO_TO_BED)->second;
            data->setDurationBased(4.0, 4.0, false);

            //if (character == gui->selectedObject.getCharacter()) DebugLog("PeriodUpdate: " + character->displayName);
            faction = character->getFaction();
            if (faction)
            {
                faction->isPlayer = nullptr;
            }
            if (!settings->hasHome())
            {
                //SetNearestFriendlyTownAsHome(platoon);
            }
            else
            {
                if (settings->isRestTime())
                {
                    settings->assignSquadHome(true);
                }
                else
                {
                    if (!settings->assignSquadHome(false))
                    {
                        settings->assignSquadHome(true);
                    }
                }
            }
            //DebugLog("End PeriodicUpdate");
        }

        periodicUpdate_orig(thisptr, time);

        if (settings && settings->isEnabled())
        {
            RevertTaskDuration(RELAX_IN_TOWN_PACKAGE);
            RevertTaskDuration(GO_HOME_AND_GO_TO_BED);
            if (faction) faction->isPlayer = settings->getPlayerInterface();
            Log("End periodUpdate");
            /*if (thisptr && character == gui->selectedObject.getCharacter())
            {
                auto currentGoal = thisptr->tryToGetCurrentGoal();
                if (currentGoal)
                {
                    auto taskData = currentGoal->taskData;
                    if (taskData)
                    {
                        std::string description = currentGoal->getDescription();
                        float durationMin = taskData->durationMin;
                        float durationFuzz = taskData->durationFuzz;
                        bool isDurationBased = taskData->isDurationBased;
                        bool infrequentGoalChecks = taskData->infrequentGoalChecks;
                        bool endsAfterTime = taskData->endsAfterTime;
                        bool isUnstoppable = taskData->isUnstoppableTask;
                        bool forDirectPlayerOrdersOnly = taskData->forDirectPlayerOrdersOnly;
                        bool forFulfillPlayerOrdersOrNPCOnly = taskData->forFulfillPlayerOrdersOrNPCOnly;
                        bool cantEndPrematurely = taskData->getRequirementsCantEndActionPrematurely();
                        bool isPermaJob = taskData->isPermaJob();
                        DebugLog(description + " Duration Min: " + Ogre::StringConverter::toString(durationMin) + " Duration Fuzz: " + Ogre::StringConverter::toString(durationFuzz) +
                            " Is Duration Based: " + Ogre::StringConverter::toString(isDurationBased) +
                            " Infrequent Goal Checks: " + Ogre::StringConverter::toString(infrequentGoalChecks) +
                            " Ends After Time: " + Ogre::StringConverter::toString(endsAfterTime) +
                            " Is Unstoppable Task: " + Ogre::StringConverter::toString(isUnstoppable) +
                            " For Direct Player Orders: " + Ogre::StringConverter::toString(forDirectPlayerOrdersOnly) +
                            " For Fulfil Player Orders or NPC: " + Ogre::StringConverter::toString(forFulfillPlayerOrdersOrNPCOnly) +
                            " Can't End Prematurely: " + Ogre::StringConverter::toString(cantEndPrematurely) +
                            " Is Permajob: " + Ogre::StringConverter::toString(isPermaJob));
                    }
                    DebugLog("Time Remaining: " + Ogre::StringConverter::toString(thisptr->getGOalExpiryTimeRemaining()));
                    DebugLog("Completed: " + Ogre::StringConverter::toString(thisptr->_taskCompletedFlag));
                    DebugLog("Impossible: " + Ogre::StringConverter::toString(thisptr->_taskImpossibleFlag));
                }
            }*/
        }
    }



    

    void (*_NV_setCurrentGoal_orig)(AITaskSytem* thisptr, Tasker* t, float score, taskPriority pri);
    void _NV_setCurrentGoal_hook(AITaskSytem* thisptr, Tasker* t, float score, taskPriority pri)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        TaskData* data = nullptr;
        float min = 0;
        float fuzz = 0;
        bool isDurationBased = false;
        bool resetTaskData = false;
        if (thisptr) character = thisptr->character;
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon)
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);
        }
        if (t)
        {
            data = t->taskData;
        }
        if (data)
        {
            TaskType type = t->key();
            if (settings && settings->isEnabled())
            {
                Log("SetCurrentGoal TaskType: " + Ogre::StringConverter::toString(static_cast<int>(type)));
                if (type == MAN_A_TURRET || type == MAN_A_TURRET_ON_BUILDING || type == MAN_THE_GATE || type == AUTO_LABOURING_MINES ||
                    type == STAND_AT_GUARD_NODE_HOMEBUILDING_INDOORS_ONLY || type == STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT ||
                    type == STAND_AT_GUARD_NODE_HOMETOWN_OUTSIDE)
                {
                    t->startTime = static_cast<int>(settings->getStartWorkTime());
                    t->endTime = static_cast<int>(settings->getEndWorkTime());
                }
                if (type == GO_HOME_AND_GO_TO_BED)
                {
                    if (settings->getDoSleep())
                    {
                        if (settings->isRestTime())
                        {
                            t->startTime = static_cast<int>(settings->getEndWorkTime());
                            t->endTime = static_cast<int>(settings->getStartWorkTime());
                        }
                        if (settings->getRestUntilHealed())
                        {
                            MedicalSystem* medical = character->getMedical();
                            RaceData* race = character->getRace();
                            if (race && !race->robot && medical && medical->restedState <= 0.9 && medical->scoreFirstAidNeed(false) < 0.1)
                            {
                                t->startTime = 0;
                                t->endTime = 24;
                            }
                        }
                    }
                }
            }
        }
        /*========Actual Function=======*/
        _NV_setCurrentGoal_orig(thisptr, t, score, pri);
        /*========Actual Function=======*/

    }

    // Hook the method (call interception) — the primary mod mechanism
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
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);
        }
        if (settings && settings->isEnabled())
        {
            Log("update4Frame");
            TaskData* data = taskTypetaskData->find(RELAX_IN_TOWN_PACKAGE)->second;
            data->setDurationBased(1.0, 4.0, false);
            data = taskTypetaskData->find(GO_HOME_AND_GO_TO_BED)->second;
            data->setDurationBased(4.0, 4.0, false);
        }
        /*========Actual Function=======*/
        update4Frame_orig(thisptr, position, time);
        /*========Actual Function=======*/
        if (settings && settings->isEnabled())
        {
            RevertTaskDuration(RELAX_IN_TOWN_PACKAGE);
            RevertTaskDuration(GO_HOME_AND_GO_TO_BED);
        }
    }

    bool (*signalStart_orig)(AIPackage* thisptr);
    bool signalStart_hook(AIPackage* thisptr)
    {

        Platoon* squad = thisptr->squad;
        SquadSettingsInfo* settings = nullptr;
        ActivePlatoon* activePlatoon = nullptr;
        if (squad)
        {
            activePlatoon = squad->activePlatoon;
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(squad);
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
                if (medical && (medical->restedState < 0.7))
                {
                    return false;
                }
            }

            //DebugLog("wasASuccessEnd");
        }

        return signalStart_orig(thisptr);
    }
   
    // Hook the method (call interception) — the primary mod mechanism
    bool (*_NV_signalStart_orig)(AIPackage* thisptr);
    bool _NV_signalStart_hook(AIPackage* thisptr)
    {

        bool result = _NV_signalStart_orig(thisptr);

        return result;
    }
    // Hook the method (call interception) — the primary mod mechanism
    bool (*_NV_wasASuccess_orig)(AIPackage* thisptr);
    bool _NV_wasASuccess_hook(AIPackage* thisptr)
    {

        bool result = _NV_wasASuccess_orig(thisptr);
        return result;
    }
    // install (in startPlugin):
    //KenshiLib::AddHook(KenshiLib::GetRealAddress(&AIPackage::_NV_wasASuccess), &_NV_wasASuccess_hook, &_NV_wasASuccess_orig);

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
        if (squad) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(squad);
        if (settings && settings->isEnabled() && thisptr)
        {
            TaskType type = thisptr->key();
            Log("score TaskType: " +Ogre::StringConverter::toString(static_cast<int>(type)) + " - "  +Ogre::StringConverter::toString(score));
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
            else if (type == MAN_THE_GATE || type == AUTO_LABOURING_MINES || type == AUTO_LABOURING_MINES_PRETEND ||
                type == STAND_AT_GUARD_NODE_HOMEBUILDING_INDOORS_ONLY || type == STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT || type == STAND_AT_GUARD_NODE_HOMETOWN_OUTSIDE ||
                type == TERRITORIAL_AGGRESSION_BUT_DONT_LEAVE_HOME || type == ATTACK_ENEMIES)
            {
                if (settings->isRestTime())
                {
                    return 0.0;
                }
            }
            else if (type == PROTECT_ALLIES || type == PROTECT_ALLIES_STAY_IN_TOWN || type == PROTECT_OWN_SQUAD)
            {
                if (stateBroadcast && stateBroadcast->isSleeping && !character->isLiterallyUnderMeleeAttackRightNowForSure())
                {
                    return 0.0;
                }
            }
            else if (type == PATROL_TOWN)
            {
                Tasker* currentTask = taskSystem->tryToGetCurrentGoal();
                if (currentTask)
                {
                    if (currentTask->key() == PATROL_TOWN)
                    {
                        float score = taskSystem->currentGoalScore;
                        DebugLog("Patrol score: " + Ogre::StringConverter::toString(score));
                        score *= 0.98;
                        DebugLog("Patrol score: " + Ogre::StringConverter::toString(score));
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
            else if (type == TAKE_INTRUDER_OUTSIDE)
            {
                Tasker* currentTask = taskSystem->tryToGetCurrentGoal();
                if (currentTask)
                {
                    if (currentTask->key() == TAKE_INTRUDER_OUTSIDE)
                    {
                        float score = taskSystem->currentGoalScore;
                        DebugLog("Patrol score: " + Ogre::StringConverter::toString(score));
                        score *= 0.8;
                        DebugLog("Patrol score: " + Ogre::StringConverter::toString(score));
                        taskSystem->currentGoalScore = std::max(score, 0.0001f);
                        return taskSystem->currentGoalScore;
                    }
                }

                if (settings->isRestTime())
                {
                    return 0.0;
                }
            }
            else if (type == GET_OUT_OF_BED_IF_ITS_EMERGENCY || type == GET_OUT_OF_BED || type == GET_OUT_OF_BED_ONCE_HEALED)
            {
                MedicalSystem* medical = nullptr;
                RaceData* race = nullptr;
                Faction* faction = nullptr;
                if (character)
                {
                    race = character->getRace();
                    medical = character->getMedical();
                    faction = character->getFaction();
                }
                if (settings->getDoSleep() && settings->getRestUntilHealed())
                {
                    if (race && !race->robot && medical && medical->restedState <= 0.9 && medical->scoreFirstAidNeed(false) < 0.1 && !character->isLiterallyUnderMeleeAttackRightNowForSure())
                    {
                        return 0.0;
                    }
                }
            }
            
            Log("End score");
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
        if (thisptr)
        {
            squad = thisptr->getPlatoon();
        }
        if (squad) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(squad);
        if (settings && settings->isEnabled() && thisptr)
        {
            Log("score-gotobed");
            character = thisptr->getCharacter();
            stateBroadcast = thisptr->getStateBroadcast();
            MedicalSystem* medical = nullptr;
            RaceData* race = nullptr;
            Faction* faction = nullptr;
            if (character)
            {
                race = character->getRace();
                medical = character->getMedical();
                faction = character->getFaction();
            }
            //for auto sleep task but idk if it even works
            if (faction && faction->isPlayer)
            {
                //DebugLog("AutoSleep Task: " + Ogre::StringConverter::toString(score));
                return _NV_scoreGoToBed_orig(thisptr, subject, _a2);
            }
            if (settings->getDoSleep())
            {
                //if need to continue resting
                if (race && !race->robot && medical && medical->restedState <= 0.9 && medical->scoreFirstAidNeed(false) < 0.1)
                {
                    if (settings->getRestUntilHealed() && stateBroadcast && stateBroadcast->isSleeping)
                    {
                        //DebugLog(character->displayName + " medical restedstate: " + Ogre::StringConverter::toString(medical->restedState));
                        return 5.0;
                    }
                    if (settings->isRestTime())
                    {
                        return 5.0;
                    }
                    else
                    {
                        return (1.0 - medical->restedState);
                    }
                }
                if (settings->isRestTime())
                {
                    if (stateBroadcast && !stateBroadcast->isSleeping)
                    {
                        float minuteSinceLastSlept = thisptr->getStateBroadcast()->lastSlept.getHoursPassed() * 60.0;
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
                        else return 1.0;
                    }
                }
            }
            return 0.0;
            Log("end gotobed");
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
        if (thisptr)
        {
            squad = thisptr->getPlatoon();
        }

        if (squad) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(squad);
        if (settings && settings->isEnabled() && thisptr)
        {
            Log("score-getoutofbed");
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
            if (faction && faction->isPlayer)
            {
                //DebugLog("AutoSleep Task: " + Ogre::StringConverter::toString(score));
                return _NV_scoreGetOutOfBed_orig(thisptr, subject, _a2);
            }
            //if still injured and has get rest until healed
            if (settings->getDoSleep())
            {

                if (settings->getRestUntilHealed() && race && !race->robot && medical && medical->restedState <= 0.9 && medical->scoreFirstAidNeed(false) < 0.1 && !character->isLiterallyUnderMeleeAttackRightNowForSure())
                {
                    //DebugLog(character->displayName + " medical restedstate: " + Ogre::StringConverter::toString(medical->restedState));
                    return -1.0;
                }
                if (!settings->isRestTime() && stateBroadcast->isSleeping)
                {
                    return 1.0;
                }
            }
            Log("End score-getoutofbed");
        }
        return _NV_scoreGetOutOfBed_orig(thisptr, subject, _a2);
    }

    // Hook the method (call interception) — the primary mod mechanism
    bool (*initialisation_orig)(GameWorld* thisptr);
    bool initialisation_hook(GameWorld* thisptr)
    {
        bool result = initialisation_orig(thisptr);
        if (result)
        {
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
    // install (in startPlugin):

    void Init()
    {
        modPath = GetCurrentDLLDirectory();
        logPath = modPath + logFileName;
        logBakPath = modPath + logBakFileName;
        SquadAutonomySettings::getSingletonPtr();
        Log("=====================New Session=====================");
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
            *(uintptr_t*)&SquadAutonomy::taskTypetaskData = baseAddr + 0x1ce80f0;
            *(uintptr_t*)&SquadAutonomy::getTaskDataConst = baseAddr + 0x283F40;
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
            *(uintptr_t*)&SquadAutonomy::taskTypetaskData = baseAddr + 0x1ce60F0;
            *(uintptr_t*)&SquadAutonomy::getTaskDataConst = baseAddr + 0x283AD0;
            //*(uintptr_t*)&SquadAutonomy::getTaskData = baseAddr + 0x519F20;
            //*(uintptr_t*)&load = baseAddr + 0x47AD00;
            //*(uintptr_t*)&TaskPathfinder_Node_solve = baseAddr + 0x50F080;
            //*(uintptr_t*)&TaskRepertoire_hasTask = baseAddr + 0x32D740;

        }
    }
    
    SquadAutonomy::Init();
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&ForgottenGUI::changeFontSize), &SquadAutonomy::ForgottenGUI_changeFontSize_hook, &SquadAutonomy::ForgottenGUI_changeFontSize_orig))
        ErrorLog("Could not add ForgottenGUI::changeFontSize hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&MainBarGUI::_CONSTRUCTOR), &SquadAutonomy::MainbarGUICONSTRUCTOR_hook, &SquadAutonomy::MainbarGUICONSTRUCTOR_orig))
        ErrorLog("Could not add &MainBarGUI::_CONSTRUCTOR constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&MainBarGUI::tabPlatoonChange), &SquadAutonomy::tabPlatoonChange_hook, &SquadAutonomy::tabPlatoonChange_orig))
        ErrorLog("Could not add MainBarGUI::tabPlatoonChange constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&MainBarGUI::_NV_update), &SquadAutonomy::_NV_update_hook, &SquadAutonomy::_NV_update_orig))
        ErrorLog("Could not add MainBarGUI::_NV_update constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OptionsWindow::create), &SquadAutonomy::OptionsWindow_create_hook, &SquadAutonomy::OptionsWindow_create_orig))
        ErrorLog("Could not add OptionsWindow::create constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OptionsWindow::saveOptions), &SquadAutonomy::saveOptions_hook, &SquadAutonomy::saveOptions_orig))
        ErrorLog("Could not add OptionsWindow::saveOptions constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OptionsWindow::closeButton), &SquadAutonomy::closeButton_hook, &SquadAutonomy::closeButton_orig))
        ErrorLog("Could not add OptionsWindow::closeButton constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&GameWorld::initialisation), &SquadAutonomy::initialisation_hook, &SquadAutonomy::initialisation_orig))
        ErrorLog("Could not add OptionsWindow::closeButton constructor hook!");


    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OptionsWindow::_NV_update), &SquadAutonomy::OptionsWindow_NV_update_hook, &SquadAutonomy::OptionsWindow_NV_update_orig))
        ErrorLog("Could not add OptionsWindow::saveOptions constructor hook!");*/
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
    if (SquadAutonomy::Package_WanderingTrader_signalStart)
    {
        if (KenshiLib::SUCCESS != KenshiLib::QueueHook(SquadAutonomy::Package_WanderingTrader_signalStart, &SquadAutonomy::signalStart_hook, &SquadAutonomy::signalStart_orig))
            ErrorLog("Could not add Wandering::signal_start hook!");
    }
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&Tasker::score), &SquadAutonomy::score_hook, &SquadAutonomy::score_orig))
        ErrorLog("Could not add Tasker::score hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&TaskData::runTargetFind), &SquadAutonomy::runTargetFind_hook, &SquadAutonomy::runTargetFind_orig))
        ErrorLog("Could not add runTargetFind hook!");*/
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::_NV_scoreGetOutOfBed), &SquadAutonomy::_NV_scoreGetOutOfBed_hook, &SquadAutonomy::_NV_scoreGetOutOfBed_orig))
        ErrorLog("Could not add AI::_NV_scoreGetOutOfBed hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::_NV_scoreGoToBed), &SquadAutonomy::_NV_scoreGoToBed_hook, &SquadAutonomy::_NV_scoreGoToBed_orig))
        ErrorLog("Could not add AI::_NV_scoreGoToBed hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::AIResultsCacher::runTargetFinder), &SquadAutonomy::runTargetFinder_hook, &SquadAutonomy::runTargetFinder_orig))
        ErrorLog("Could not add AI::AIResultsCacher::runTargetFinder hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OrdersReceiver::clearCurrentGoal), &SquadAutonomy::clearCurrentGoal_hook, &SquadAutonomy::clearCurrentGoal_orig))
        ErrorLog("Could not add OrdersReceiver::clearCurrentGoal hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::runGoals), &SquadAutonomy::runGoals_hook, &SquadAutonomy::runGoals_orig))
        ErrorLog("Could not add runGoals hook!");*/
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::choosePermaJob), &SquadAutonomy::choosePermaJob_hook, &SquadAutonomy::choosePermaJob_orig))
        ErrorLog("Could not add AITaskSytem::choosePermaJob hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::chooseGoalFrom), &SquadAutonomy::chooseGoalFrom_hook, &SquadAutonomy::chooseGoalFrom_orig))
        ErrorLog("Could not add chooseGoalFrom hook!");*/
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::chooseGoal), &SquadAutonomy::chooseGoal_hook, &SquadAutonomy::chooseGoal_orig))
        ErrorLog("Could not add AITaskSytem::chooseGoal hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::runGOAP) , &SquadAutonomy::runGOAP_hook, &SquadAutonomy::runGOAP_orig))
        ErrorLog("Could not add AITaskSytem::runGOAP hook!");*/
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::periodicUpdate), &SquadAutonomy::periodicUpdate_hook, &SquadAutonomy::periodicUpdate_orig))
        ErrorLog("Could not add AITaskSytem::periodicUpdate hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::_NV_setCurrentGoal), &SquadAutonomy::_NV_setCurrentGoal_hook, &SquadAutonomy::_NV_setCurrentGoal_orig))
        ErrorLog("Could not add AITaskSytem::_NV_setCurrentGoal hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::setTaskExpiryTimer), &SquadAutonomy::setTaskExpiryTimer_hook, &SquadAutonomy::setTaskExpiryTimer_orig))
        ErrorLog("Could not add AITaskSytem::setTaskExpiryTimer hook!");*/
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::update4Frame), &SquadAutonomy::update4Frame_hook, &SquadAutonomy::update4Frame_orig))
        ErrorLog("Could not add AITaskSytem::update4Frame hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(SquadAutonomy::EscMenu_openedOtherWindows, &SquadAutonomy::EscMenu_openedOtherWindows_hook, &SquadAutonomy::EscMenu_openedOtherWindows_orig))
        ErrorLog("Could not add EscMenu hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(TaskPathfinder_Node_solve, &SquadAutonomy::TaskPathfinder_Node_solve_hook, &SquadAutonomy::TaskPathfinder_Node_solve_orig))
        ErrorLog("Could not add TaskPathfinder_Node_solve hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(TaskRepertoire_hasTask, &SquadAutonomy::TaskRepertoire_hasTask_hook, &SquadAutonomy::TaskRepertoire_hasTask_orig))
        ErrorLog("Could not add TaskRepertoire_hasTask hook!");*/
    KenshiLib::ApplyQueuedHooks();
    DebugLog("Mod started");
}

