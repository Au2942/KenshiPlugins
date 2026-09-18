#include "SquadAutonomy.h"
#include "SquadAutonomyPanel.h"
#include "SquadAutonomySettings.h"

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
    bool (*EscMenu_openedOtherWindows)(class EscMenu*) = nullptr;
    std::map<hand, float>* rentedBeds = nullptr;
    bool (*Package_WanderingTrader_signalStart)(AIPackage*) = nullptr;
    std::string* _MainColorCode = nullptr;
    bool showOnMain = true;
    bool useFloatingPanel = false;
    bool showInSquad = true;
    bool enableLogging = false;
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
    bool mDragging = false;

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
    
    bool SetAI(Platoon* platoon, std::map<int, lektor<GameData*>> aiPackages)
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
            obj->ai->resultsCache.resetAllCaches();
            obj->getBody()->_endAction();
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
    bool ResetAI(Platoon* platoon)
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
            obj->ai->resultsCache.resetAllCaches();
            obj->getBody()->_endAction();
        }
        return true;
    }

    hand FindOptimalBed(Character* character, bool usePaidBeds)
    {
        if (!character) return nullptr;
        AI* ai = character->ai;
        if (!ai) return nullptr;
        UseableStuff* optimalBed = nullptr;

        RaceData* race = character->getRace();
        lektor<UseableStuff*> beds;
        float maxScore = std::numeric_limits<float>::lowest();
        float maxEfficiency = std::numeric_limits<float>::lowest();
        float minCost = std::numeric_limits<float>::max();
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
                if (!bedBuildings) continue;
                UseableStuff* bed = bedBuildings->at(i)->getUseableStuff();
                if (bed) lektorEx::push_back_unique(beds, bed);
            }
        }
        bool ownFactionOnly = false;
        Faction* ownFaction = character->getFaction();

        for (uint32_t i = 0; i < beds.size(); ++i)
        {
            UseableStuff* b = beds[i];
            if (b->getOccupant())
            {
                if (b->getOccupant() == character->getHandle())
                {
                    return b->handle;
                }
                continue;
            }
            Faction* faction = b->getFaction();

            if (ownFaction && faction)
            {
                if (faction == ownFaction)
                {
                    if (!ownFactionOnly)
                    {
                        ownFactionOnly = true;
                        maxScore = character->ai->scoreDistanceTo(b, false);
                        optimalBed = b;
                        continue;
                    }
                }
                else
                {
                    if (ownFactionOnly) continue;
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
                else if (cost == minCost && distanceScore > maxScore) {
                    better = true;
                }
            }

            if (better) {
                optimalBed = b;
                maxEfficiency = efficiency;
                minCost = cost;
                maxScore = distanceScore;
            }
        }
        if (optimalBed)
        {
            if (optimalBed->getOccupant()) return nullptr;
            else return optimalBed->handle;
        }
        return nullptr;
    }


    void OpenSquadAutonomyPanel(Platoon* platoon)
    {
        if (platoon) SquadAutonomyPanel::getSingletonPtr()->selectSquad(platoon);
        if (SquadAutonomyPanel::getSingletonPtr()->isVisible())
        {
            SquadAutonomyPanel::getSingletonPtr()->hide();
        }
        else
        {
            SquadAutonomyPanel::getSingletonPtr()->show();
        }
    }
    void OpenSquadAutonomyPanelMainBar(MyGUI::Widget* sender)
    {
        Platoon* platoon = ou->player->getCurrentPlatoon();
        if (platoon) SquadAutonomyPanel::getSingletonPtr()->selectSquad(platoon);
        if (SquadAutonomyPanel::getSingletonPtr()->isVisible())
        {
            SquadAutonomyPanel::getSingletonPtr()->hide();
        }
        else
        {
            SquadAutonomyPanel::getSingletonPtr()->show();
        }
    }

    void (*ForgottenGUI_changeFontSize_orig)();
    void ForgottenGUI_changeFontSize_hook()
    {
        ForgottenGUI_changeFontSize_orig();
        if (SquadAutonomyPanel::initialized)
            SquadAutonomyPanel::getSingletonPtr()->create();

    }

    void (*OptionsWindow_create_orig)(OptionsWindow* thisptr);
    void OptionsWindow_create_hook(OptionsWindow* thisptr)
    {
        OptionsWindow_create_orig(thisptr);
        auto tabCount = thisptr->tabs->getItemCount();
        std::vector<int> catList(tabCount);
        int maxCat = 0;
        DatapanelGUI* generalPanel = nullptr;
        for (size_t i = 0; i < tabCount; i++)
        {
            auto panel = *(thisptr->tabs->getItemDataAt<DatapanelGUI*>(i, false));
            //From KEP: general category = 0x1
            if (panel && panel->getCurrentCategory() == 0x1)
            {
                generalPanel = panel;
            }
        }
        if (generalPanel)
        {
            auto tooltip = thisptr->tooltip;
            auto textbox = generalPanel->setLineText("", *_MainColorCode + "[Squad Autonomy]", 0x1, true, MyGUI::Align::Left);
            auto checkbox = generalPanel->setLineCheckbox("Show button on Mainbar", &showOnMain, 0x1);
            tooltip->setup(checkbox->getTextBox(), "Show AUT button on the Mainbar");
            checkbox = generalPanel->setLineCheckbox("Use floating panel", &useFloatingPanel, 0x1);
            tooltip->setup(checkbox->getTextBox(), "Use floating panel for AUT button");
            checkbox = generalPanel->setLineCheckbox("Show button in Squad screen", &showInSquad, 0x1);
            tooltip->setup(checkbox->getTextBox(), "Show AUT button in the Squad Management screen");
            checkbox = generalPanel->setLineCheckbox("Enable logging", &enableLogging, 0x1);
            tooltip->setup(checkbox->getTextBox(), "Write to log. Turn on if experiencing frequent crashes and include the last few lines with your bug report.");
        }
    }

    void (*saveOptions_orig)(OptionsWindow* thisptr);
    void saveOptions_hook(OptionsWindow* thisptr)
    {
        saveOptions_orig(thisptr);
        auto settings = SquadAutonomySettings::getSingletonPtr();
        std::ifstream cfgFile(settings->getConfigPath(), std::fstream::in);
        std::ofstream tempFile("temp.txt");
        if (!cfgFile.is_open() || !tempFile.is_open())
        {
            DebugLog("Config Save: Cannot open file");
        }
        else
        {
            bool packagesFound = false;
            std::string line = "";
            tempFile << "<Options>" << '\n';
            tempFile << "ShowOnMain: " << Ogre::StringConverter::toString(showOnMain) << '\n';
            tempFile << "UseFloatingPanel: " << Ogre::StringConverter::toString(useFloatingPanel) << '\n';
            tempFile << "ShowInSquad: " << Ogre::StringConverter::toString(showInSquad) << '\n';
            tempFile << "EnableLogging: " << Ogre::StringConverter::toString(enableLogging) << '\n';
            tempFile << "</Options>" << '\n';
            while (std::getline(cfgFile, line))
            {
                if (line == "<Packages>")
                {
                    tempFile << line << '\n';
                    while (std::getline(cfgFile, line))
                    {
                        tempFile << line << '\n';
                        if (line == "</Packages>")
                        {
                            packagesFound = true;
                            break;
                        }
                    }
                    break;
                }
            }
            if (!packagesFound)
            {
                tempFile << "<Packages>" << '\n';
                tempFile << "</Packages>";
            }
            cfgFile.close();
            tempFile.close();

            std::remove(settings->getConfigPath().c_str());
            std::rename("temp.txt", settings->getConfigPath().c_str());
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

    void onDragPressed(
        MyGUI::Widget* sender,
        int left,
        int top,
        MyGUI::MouseButton id)
    {
        if (id != MyGUI::MouseButton::Left)
            return;

        mDragging = true;

        mDragStart = MyGUI::IntPoint(left, top);
        mWindowStart = autBtnWindow->getPosition();
    }

    void onDrag(
        MyGUI::Widget* sender,
        int left,
        int top,
        MyGUI::MouseButton id)
    {
        if (!mDragging || id != MyGUI::MouseButton::Left)
            return;

        const int dx = left - mDragStart.left;
        const int dy = top - mDragStart.top;

        autBtnWindow->setPosition(
            mWindowStart.left + dx,
            mWindowStart.top + dy
        );
    }

    MainBarGUI* (*MainbarGUICONSTRUCTOR_orig)(MainBarGUI* thisptr);
    MainBarGUI* MainbarGUICONSTRUCTOR_hook(MainBarGUI* thisptr)
    {
        MainBarGUI* orig = MainbarGUICONSTRUCTOR_orig(thisptr);
        autBtn = orig->getWidget()->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.690, 0.788, 0.015, 0.0358, MyGUI::Align::Center, "AUTBtn");
        autBtn->setCaption("AUT");
        autBtn->setFontHeight(12);
        autBtn->eventMouseButtonClick += MyGUI::newDelegate(OpenSquadAutonomyPanelMainBar);
        autBtn->setDepth(0);

        MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();

        autBtnWindow = gui->createWidgetReal<MyGUI::Window>(
            "",
            0.655, 0.727,
            0.039, 0.065,
            MyGUI::Align::Center,
            "Modal",
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
        btn->eventMouseButtonClick += MyGUI::newDelegate(OpenSquadAutonomyPanelMainBar);
        if (!showOnMain)
        {
            autBtn->setVisible(false);
            autBtnWindow->setVisible(false);
        }
        else if (useFloatingPanel)
        {
            autBtn->setVisible(false);
        }
        return orig;
    }

    void (*_NV_update_orig)(MainBarGUI* thisptr);
    void _NV_update_hook(MainBarGUI* thisptr)
    {
        _NV_update_orig(thisptr);
        if (autBtn && autBtnWindow)
        {
            if (showOnMain)
            {
                autBtn->setVisible(!useFloatingPanel);
                autBtnWindow->setVisible(useFloatingPanel);
                if (!useFloatingPanel)
                {
                    autBtn->setRealPosition(0.69, 0.788);
                    autBtn->setFontHeight(12);
                }
            }
            else
            {
                autBtn->setVisible(false);
                autBtnWindow->setVisible(false);
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
            if (type == GO_HOME_AND_GO_TO_BED || type == PUT_DOWN_CHARACTER_IN_BED)
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



    void (*periodicUpdate_orig)(AITaskSytem* thisptr, float time);
    void periodicUpdate_hook(AITaskSytem* thisptr, float time)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        PlayerInterface* pi = nullptr;
        Faction* faction = nullptr;
        Platoon* platoon = nullptr;
        if (thisptr) character = thisptr->character;
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);

        if (settings && settings->isEnabled())
        {
            Log("PeriodicUpdate");
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
        if (t && settings && settings->isEnabled())
        {
            TaskType type = t->key();
            data = t->taskData;
            Log("SetCurrentGoal TaskType: " + Ogre::StringConverter::toString(static_cast<int>(type)));
            if (type == MAN_A_TURRET || type == MAN_A_TURRET_ON_BUILDING || type == MAN_THE_GATE || type == AUTO_LABOURING_MINES ||
                type == STAND_AT_GUARD_NODE_HOMEBUILDING_INDOORS_ONLY || type == STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT ||
                type == STAND_AT_GUARD_NODE_HOMETOWN_OUTSIDE)
            {
                t->startTime = static_cast<int>(settings->getStartWorkTime());
                t->endTime = static_cast<int>(settings->getEndWorkTime());
                //DebugLog("Man something end time: " + Ogre::StringConverter::toString(t->endTime));
                //std::string description = t->getDescription();
                //taskData->setDurationBased(0.1, 0.5, false);

            }
            if (type == GO_HOME_AND_GO_TO_BED)
            {
                if (settings->getDoSleep())
                {
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
                    else if (settings->isRestTime())
                    {
                        t->startTime = static_cast<int>(settings->getEndWorkTime());
                        t->endTime = static_cast<int>(settings->getStartWorkTime());
                    }
                    if (data)
                    {
                        min = data->durationMin;
                        fuzz = data->durationFuzz;
                        isDurationBased = data->isDurationBased;
                        data->setDurationBased(4.0, 4.0, false);
                        resetTaskData = true;
                    }
                }
            }
            if (type == RELAX_IN_TOWN_PACKAGE)
            {
                if (data)
                {
                    min = data->durationMin;
                    fuzz = data->durationFuzz;
                    isDurationBased = data->isDurationBased;
                    data->setDurationBased(1.0, 1.0, false);
                    resetTaskData = true;
                }
            }
        }
        /*========Actual Function=======*/
        _NV_setCurrentGoal_orig(thisptr, t, score, pri);
        /*========Actual Function=======*/
        if (t && settings && settings->isEnabled())
        {

            if (data && resetTaskData)
            {
                data->setDurationBased(min, fuzz, isDurationBased);
            }
            Log("End SetCurrentGoal");
        }
    }

    void (*setTaskExpiryTimer_orig)(AITaskSytem* thisptr);
    void setTaskExpiryTimer_hook(AITaskSytem* thisptr)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        TaskData* data = nullptr;
        Tasker* tasker = nullptr;
        float min = 0;
        float fuzz = 0;
        bool isDurationBased = false;
        bool resetTaskData = false;
        if (thisptr)
        {
            character = thisptr->character;
            tasker = thisptr->getCurrentDurationTimedTask();
        }
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon)
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);
        }
        if (tasker && settings && settings->isEnabled())
        {
            TaskType type = tasker->key();
            data = tasker->taskData;
            Log("TaskExpiryTimer: " + Ogre::StringConverter::toString(static_cast<int>(type)));
            if (data)
            {
                min = data->durationMin;
                fuzz = data->durationFuzz;
                isDurationBased = data->isDurationBased;
                if (type == GO_HOME_AND_GO_TO_BED)
                {
                    if (settings->getDoSleep())
                    {
                    
                        data->setDurationBased(4.0, 4.0, false);
                        resetTaskData = true;
                    }
                }
                if (type == RELAX_IN_TOWN_PACKAGE)
                {
                    if (data)
                    {
                        data->setDurationBased(1.0, 1.0, false);
                        resetTaskData = true;
                    }
                }
            }
        }
        /*========Actual Function=======*/
        setTaskExpiryTimer_orig(thisptr);
        /*========Actual Function=======*/
        if (tasker && settings && settings->isEnabled())
        {
            if (resetTaskData && data)
            {
                data->setDurationBased(min, fuzz, isDurationBased);
            }
            Log("End TaskExpiryTimer");
        }
    }

    // Hook the method (call interception) — the primary mod mechanism
    void (*update4Frame_orig)(AITaskSytem* thisptr, Ogre::Vector3 position, float time);
    void update4Frame_hook(AITaskSytem* thisptr, Ogre::Vector3 position, float time)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        TaskData* data = nullptr;
        Tasker* tasker = nullptr;
        float min = 0;
        float fuzz = 0;
        bool isDurationBased = false;
        bool resetTaskData = false;
        if (thisptr)
        {
            character = thisptr->character;
            tasker = thisptr->getCurrentDurationTimedTask();
        }
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon)
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);
        }
        if (tasker && settings && settings->isEnabled())
        {
            TaskType type = tasker->key();
            data = tasker->taskData;
            Log("TaskExpiryTimer: " + Ogre::StringConverter::toString(static_cast<int>(type)));
            if (data)
            {
                min = data->durationMin;
                fuzz = data->durationFuzz;
                isDurationBased = data->isDurationBased;
                if (type == GO_HOME_AND_GO_TO_BED)
                {
                    if (settings->getDoSleep())
                    {

                        data->setDurationBased(4.0, 4.0, false);
                        resetTaskData = true;
                    }
                }
                if (type == RELAX_IN_TOWN_PACKAGE)
                {
                    if (data)
                    {
                        data->setDurationBased(1.0, 1.0, false);
                        resetTaskData = true;
                    }
                }
            }
        }
        /*========Actual Function=======*/
        update4Frame_orig(thisptr, position, time);
        /*========Actual Function=======*/
        if (tasker && settings && settings->isEnabled())
        {
            if (resetTaskData && data)
            {
                data->setDurationBased(min, fuzz, isDurationBased);
            }
            Log("End TaskExpiryTimer");
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
                if (obj->stateBroadcast && (obj->stateBroadcast->isSleeping || obj->stateBroadcast->unconcious))
                {
                    //DebugLog("Member is sleeping");
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
                    return score *= 0.001;
                }
            }
            else if (type == MAN_A_TURRET || type == MAN_A_TURRET_ON_BUILDING)
            {
                if (!settings->getManTurrets() || settings->isRestTime())
                {
                    return 0.0;
                }
            }
            else if (type == MAN_THE_GATE || type == AUTO_LABOURING_MINES || type == AUTO_LABOURING_MINES_PRETEND ||
                type == STAND_AT_GUARD_NODE_HOMEBUILDING_INDOORS_ONLY || type == STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT || type == STAND_AT_GUARD_NODE_HOMETOWN_OUTSIDE)
            {
                if (settings->isRestTime())
                {
                    return 0.0;
                }
            }
            else if (type == PATROL_TOWN)
            {
                if (settings->isRestTime())
                {
                    return score * 0.001;
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
                //if need to rest
                if (settings->getRestUntilHealed() && stateBroadcast && stateBroadcast->isSleeping)
                {
                    if (race && !race->robot && medical && medical->restedState <= 0.9 && medical->scoreFirstAidNeed(false) < 0.1)
                    {
                        //DebugLog(character->displayName + " medical restedstate: " + Ogre::StringConverter::toString(medical->restedState));
                        return 2.0;
                    }
                }
                if (settings->isRestTime())
                {
                    if (race && !race->robot && medical && medical->restedState <= 0.9 && medical->scoreFirstAidNeed(false) < 0.1)
                    {
                        //DebugLog(character->displayName + " medical restedstate: " + Ogre::StringConverter::toString(medical->restedState));
                        return 2.0;
                    }
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
                else if (race && !race->robot && medical && medical->restedState <= 0.9 && medical->scoreFirstAidNeed(false) < 0.1)
                {
                    return (1.0 - medical->restedState);
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

                if (race && !race->robot && medical && medical->restedState <= 0.9 && medical->scoreFirstAidNeed(false) < 0.1 && !character->isLiterallyUnderMeleeAttackRightNowForSure())
                {
                    //DebugLog(character->displayName + " medical restedstate: " + Ogre::StringConverter::toString(medical->restedState));
                    return 0.0;
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
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::setTaskExpiryTimer), &SquadAutonomy::setTaskExpiryTimer_hook, &SquadAutonomy::setTaskExpiryTimer_orig))
        ErrorLog("Could not add AITaskSytem::setTaskExpiryTimer hook!");
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

