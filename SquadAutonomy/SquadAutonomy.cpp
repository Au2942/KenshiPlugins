#include <Debug.h>

#include <ogre/OgreStringConverter.h>

#include <boost/scoped_ptr.hpp>

#include <kenshi/Kenshi.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/MainBarGUI.h>
#include <kenshi/gui/OrdersPanel.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <kenshi/gui/SquadManagementScreen.h>

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/GameData.h>
#include <kenshi/RootObject.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Character.h>
#include <kenshi/CharMovement.h>
#include <kenshi/CharBody.h>
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
#include <kenshi/Town.h>
#include <kenshi/SharedKing.h>
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



namespace SquadAutonomy
{
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

    bool SetAI(ActivePlatoon*, std::map<int, lektor<GameData*>>, bool = true);
    bool ResetAI(ActivePlatoon*, bool = true);

    class SquadSettingsInfo
    {
    public:
        SquadSettingsInfo(bool enabled, ActivePlatoon* squad)
        {
            _enabled = enabled;
            _squad = squad;
        }
        ActivePlatoon* getSquad()
        {
            return _squad;
        }
        bool getEnabled()
        {
            return _enabled;
        }
        void addPackage(int priority, GameData* data)
        {
            auto find = _squadPackages.find(priority);
            if (_squadPackages.size() > 0 && find != _squadPackages.end())
            {
                lektorEx::push_back_unique(find->second, data);
            }
            else
            {
                lektor<GameData*> dataList;
                _squadPackages[priority] = dataList;
                lektorEx::push_back_unique(_squadPackages[priority], data);
            }
        }
        void clearPackages()
        {
            _squadPackages.clear();
        }
        std::map<int, lektor<GameData*>> getSquadPackages()
        {
            return _squadPackages;
        }
        bool enableAutonomy(bool enable)
        {
            bool success = false;
            if (_enabled != enable)
            {
                if (enable)
                {
                    success = SetAI(_squad, _squadPackages);
                }
                else
                {
                    success = ResetAI(_squad);
                }
                if (success) _enabled = enable;
            }
            return success;
        }
    private:
        ActivePlatoon* _squad;
        bool _enabled;
        std::map<int, lektor<GameData*>> _squadPackages;
    };
    class SquadAutonomySettings
    {
    public:
        static SquadAutonomySettings* getSingletonPtr();
        lektor<SquadSettingsInfo*> squadSettings;
        SquadSettingsInfo* getSquadSettings(ActivePlatoon* squad, bool createNew = true)
        {
            for (int i = 0; i < squadSettings.size(); ++i)
            {
                if (squad == squadSettings[i]->getSquad())
                {
                    return squadSettings[i];
                }
            }
            if (createNew)
            {
                SquadSettingsInfo* newSettings = new SquadSettingsInfo(false, squad);
                lektorEx::push_back(squadSettings, newSettings);
                return newSettings;
            }
            return nullptr;
        }
        std::map<int, lektor<GameData*>>* getSquadPackages(ActivePlatoon* squad, bool createNew = true)
        {
            for (int i = 0; i < squadSettings.size(); ++i)
            {
                if (squad == squadSettings[i]->getSquad())
                {
                    return &squadSettings[i]->getSquadPackages();
                }
            }
            if (createNew)
            {
                SquadSettingsInfo* newSettings = new SquadSettingsInfo(false, squad);
                lektorEx::push_back(squadSettings, newSettings);
                return &newSettings->getSquadPackages();
            }
            return nullptr;
        }

    private:

    };
    SquadAutonomySettings* SquadAutonomySettings::getSingletonPtr()
    {
        static boost::scoped_ptr<SquadAutonomySettings> singleton(new SquadAutonomySettings());
        return singleton.get();
    }

    //lektor<ActivePlatoon*> autonomousPlatoons;
    //std::unordered_map<Character*, PlayerInterface*> characterPIs;

    //based on KEP dev tools panel
    class SquadAutonomyPanel
    {
    public:
        static SquadAutonomyPanel* getSingletonPtr();
        static bool initialized();
        SquadAutonomyPanel();
        //~SquadAutonomyPanel();

        void create();
        void refresh();
        void show();
        void hide();
        bool isVisible();
        void selectSquad(ActivePlatoon*);

    private:
        int _category;
        ActivePlatoon* _selectedSquad;
        void _toggleAI(DataPanelLine* line); //button for enabling/setting packages to a squad
        void _addAI(DataPanelLine* line); //set the packages in the setting
        void _clearAI(DataPanelLine* line); //clear all packages in the setting

        void _changeAIPackageSearchText(DataPanelLine* line);
        void _loadConfig();
        void _initAIPackageList();
        void _updateAIPackageList(const std::string& keyword);

        DatapanelGUI* _panel;
        lektor<GameData*> _AIPackageList;
        lektor<std::string> _cfgPackageList;
        std::string _cfgPath;
        int _selectedAIPackageIndex;
        float _priority;

        void SetButton(const std::string& caption, int cat, float width, void (SquadAutonomyPanel::* callback)(DataPanelLine*));
        void SetDropBox(const std::string& caption, int cat, float width, int* valPtr, void (SquadAutonomyPanel::* callback)(DataPanelLine*));
        void SetEditBox(const std::string& caption, int cat, float width, const std::string& text, void (SquadAutonomyPanel::* callback)(DataPanelLine*));
    };

    bool _initialized = false;
    std::string lineBoxAIPackage;

    void initLineKey()
    {
        //for future localization
        lineBoxAIPackage = "AI package";
    }

    SquadAutonomyPanel* SquadAutonomyPanel::getSingletonPtr()
    {
        static boost::scoped_ptr<SquadAutonomyPanel> singleton(new SquadAutonomyPanel());
        return singleton.get();
    }

    bool SquadAutonomyPanel::initialized()
    {
        return _initialized;
    }

    SquadAutonomyPanel::SquadAutonomyPanel() : _panel(nullptr), _selectedAIPackageIndex(0), _cfgPath("SquadAutonomy.cfg"), _category(0), _priority(0.0f)
    {
        _loadConfig();
        initLineKey();
        _initAIPackageList();
        create();
        _initialized = true;
    }

    void SquadAutonomyPanel::create()
    {
        if (this->_panel != nullptr)
        {
            this->_panel->show(false);
            gui->destroy(this->_panel);
        }

        //this->_panel = gui->createDatapanel(0.828f, 0.40f, 0.28f, 0.172f, true, "Window", true);
        this->_panel = gui->createDatapanel(0.25f, 0.25f, 0.5f, 0.5f, true, "Window", true);
        this->_panel->setCaption("Squad Autonomy");
        this->_panel->setPanelName("SquadAutonomy");

        refresh();

        this->_panel->show(false);
    }

    void SquadAutonomyPanel::refresh()
    {
        if (this->_panel == nullptr)
            return;
        this->_panel->setLineSpacing(32.0f);
        this->_panel->clearPage(this->_category);

        auto squadSettings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad);
        auto button = this->_panel->setLineToggleButton("", "Enable Autonomy", this->_category);
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, &SquadAutonomyPanel::_toggleAI);
        if (squadSettings)
        {
            button->button->setStateSelected(squadSettings->getEnabled());
        }
        auto textbox = this->_panel->setLineTextEditable("Search", "", this->_category, true, false, MyGUI::Align::Left, 0.95f);
        textbox->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, &SquadAutonomyPanel::_changeAIPackageSearchText);
        DebugLog(textbox->editBox->_getTextureName());
        auto dropbox = this->_panel->setLineDropBox(lineBoxAIPackage, this->_category, &this->_selectedAIPackageIndex, false, 1.0f);
        _updateAIPackageList("");
        textbox->getEditBox()->setSize(dropbox->listBox->getSize());

        auto slider = this->_panel->setLineSliderEditable("Priority", this->_category, true, 0.0f, 5.0f, &this->_priority);
        slider->nameText->setEnabled(false);
        slider->setPrecision(0);

        button = this->_panel->setLineTextButton("", "Add AI Package", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, &SquadAutonomyPanel::_addAI);

        button = this->_panel->setLineTextButton("", "Clear AI Packages", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, &SquadAutonomyPanel::_clearAI);
        
        if (squadSettings)
        {
            auto squadPackages = squadSettings->getSquadPackages();
            if (squadPackages.size() > 0)
            {
                DebugLog("Squad Settings exist: " + Ogre::StringConverter::toString(squadPackages.size()));
                for (auto it = squadPackages.begin(); it != squadPackages.end(); ++it)
                {
                    auto packages = it->second;
                    DebugLog(Ogre::StringConverter::toString(it->first) + " Packages: " + Ogre::StringConverter::toString(packages.size()));
                    for (int i = 0; i < packages.size(); ++i)
                    {
                        DebugLog(Ogre::StringConverter::toString(it->first) + " " + packages[i]->name);
                        auto textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left);
                        textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
                        textbox->editBox->setCaption(Ogre::StringConverter::toString(it->first) + " " + packages[i]->name);
                    }
                }
            }
        }
    
    }

    void SquadAutonomyPanel::SetButton(const std::string& caption, int cat, float width, void (SquadAutonomyPanel::* callback)(DataPanelLine*))
    {
        auto button = _panel->setLineTextButton("", caption, cat, width, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, callback);
    }

    void SquadAutonomyPanel::SetDropBox(const std::string& caption, int cat, float width, int* valPtr, void (SquadAutonomyPanel::* callback)(DataPanelLine*))
    {
        auto dropbox = _panel->setLineDropBox(caption, cat, valPtr, true, width);
        dropbox->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, callback);
    }

    void SquadAutonomyPanel::SetEditBox(const std::string& caption, int cat, float width, const std::string& text, void (SquadAutonomyPanel::* callback)(DataPanelLine*))
    {
        auto textbox = _panel->setLineTextEditable(caption, text, cat, true, false, MyGUI::Align::Left, width);
        textbox->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, callback);
    }

    void SquadAutonomyPanel::show()
    {
        this->_panel->show(true);
        MyGUI::LayerManager::getInstancePtr()->upLayerItem(this->_panel->getWidget());
        refresh();
    }

    void SquadAutonomyPanel::hide()
    {
        this->_panel->show(false);
    }

    bool SquadAutonomyPanel::isVisible()
    {
        return this->_panel->isVisible();
    }

    void SquadAutonomyPanel::selectSquad(ActivePlatoon* squad)
    {
        _selectedSquad = squad;
    }

    void SquadAutonomyPanel::_loadConfig()
    {
        _cfgPath = GetCurrentDLLDirectory() + "SquadAutonomy.cfg";
        DebugLog("Config file is at: " + _cfgPath);
        DebugLog("Reading config file");
        std::fstream cfgFile(_cfgPath, std::ios::in | std::ios::out | std::ios::app);
        if (!cfgFile.is_open())
        {
            DebugLog("Cannot open config file");
            return;
        }
        std::string line;
        std::string token;
        while (std::getline(cfgFile, line))
        {
            //DebugLog(line);
            if (line == "<Packages>")
            {
                while (std::getline(cfgFile, line))
                {
                    if (line == "</Packages>")
                    {
                        DebugLog("Done reading packages in config");
                        break;
                    }
                    bool insideQuote = false;
                    std::stringstream ssline(line);
                    while (std::getline(ssline, token, '"'))
                    {
                        if (!token.empty())
                        {
                            if (insideQuote)
                            {
                                lektorEx::push_back(_cfgPackageList, token);
                                DebugLog("Detected package in config: " + token);
                            }
                        }
                        insideQuote = !insideQuote;
                    }
                }
            }

        }
        cfgFile.close();
    }

    void SquadAutonomyPanel::_initAIPackageList()
    {
        lektor<GameData*> datas;
        ou->gamedata.getDataOfType(datas, AI_PACKAGE);
        std::string identifier = "<SquadAutonomy>";
        DebugLog("Initialized AI Packages");
        for (uint32_t i = 0; i < datas.size(); ++i)
        {
            std::string packageName = datas[i]->name;
            if (packageName.size() >= identifier.size() && packageName.compare(packageName.size() - identifier.size(), identifier.size(), identifier) == 0)
            {
                DebugLog("Load package: " + packageName);
                lektorEx::push_back(_AIPackageList, datas[i]);
            }
            else if(_cfgPackageList.size() > 0)
            {
                int matchedIndex = -1;
                for (uint32_t j = 0; j < _cfgPackageList.size(); ++j)
                {
                    if (packageName == _cfgPackageList[j])
                    {
                        DebugLog("Load package: " + packageName);
                        matchedIndex = j;
                        lektorEx::push_back(_AIPackageList, datas[i]);
                    }
                }
                if (matchedIndex > -1)
                {
                    lektorEx::removeAt(_cfgPackageList, matchedIndex);
                }
            }
        }
    }

    void SquadAutonomyPanel::_changeAIPackageSearchText(DataPanelLine* line)
    {
        std::string keyword = "";
        if (line != nullptr && line->classType == DataPanelLine::DPL_TEXT_EDIT)
            keyword = reinterpret_cast<DataPanelLine_TextEditable*>(line)->editBox->getOnlyText();

        _updateAIPackageList(keyword);
    }

    void SquadAutonomyPanel::_updateAIPackageList(const std::string& keyword)
    {
        auto dropBox = reinterpret_cast<DataPanelLine_DropBox*>(this->_panel->getLine(lineBoxAIPackage, this->_category));
        if (dropBox == nullptr)
            return;

        int currentSelected = this->_selectedAIPackageIndex;
        if (currentSelected < 0)
            currentSelected = 0;
        int selectVal = -1;
        dropBox->clearValues();
        if (keyword.empty())
        {
            selectVal = currentSelected;
            for (uint32_t i = 0; i < this->_AIPackageList.size(); ++i)
            {
                dropBox->addAValue(this->_AIPackageList[i]->name, i);
            }
        }
        else
        {
            std::string s1 = keyword;
            std::transform(s1.begin(), s1.end(), s1.begin(), [](char c) { return std::toupper(c); });
            for (uint32_t i = 0; i < this->_AIPackageList.size(); ++i)
            {
                std::string& name = this->_AIPackageList[i]->name;
                std::string s2 = name;
                std::transform(s2.begin(), s2.end(), s2.begin(), [](char c) { return std::toupper(c); });
                if (s2.find(s1) != std::string::npos)
                {
                    dropBox->addAValue(name, i);
                    if (selectVal == -1 || i == currentSelected)
                        selectVal = i;
                }
            }
        }
        dropBox->setSelectedValue(selectVal);
    }

    void SquadAutonomyPanel::_toggleAI(DataPanelLine* line)
    {
        if (!_selectedSquad) return;
        auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad);
        if (line != nullptr && line->classType == DataPanelLine::DPL_BUTTON)
        {
            auto button = reinterpret_cast<DataPanelLine_Button*>(line)->button;
            if (settings && settings->enableAutonomy(!button->getStateSelected()))
            {
                button->setStateSelected(!button->getStateSelected());
            }
        }
    }

   

    void SquadAutonomyPanel::_addAI(DataPanelLine* line)
    {
        /*ActivePlatoon* platoon;
        Ogre::FastArray<MainTabPortraitPlatoon> tabPlatoons = gui->mainbar->tabPortraits;
        for (int i = 0; i < tabPlatoons.size(); ++i)
        {
            if (tabPlatoons[i].tab->getVisible())
            {
                platoon = tabPlatoons[i].platoon;
                DebugLog("Squad: " + platoon->getName() + " is selected");
            }
        }*/
        if (_selectedSquad)
        {
            GameData* data;
            if (_selectedAIPackageIndex < _AIPackageList.size()) data = _AIPackageList[_selectedAIPackageIndex];
            SquadSettingsInfo* squadSettings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad);
            if (data && squadSettings)
            {
                squadSettings->addPackage(static_cast<int>(_priority), data);
                DebugLog("Add " + data->name + " package");
            }
            refresh();
        }
        else
        {
            DebugLog("Add AI: No squad selected.");
        }
        /*if (platoon && platoon->isPlayer)
        {
            auto squadPackage = settings.GetSquadPackages(platoon);
            if (squadPackage)
            {
                SetAI(platoon, *squadPackage);
            }
        }
        else
        {
            ou->showPlayerAMessage("Error: Please select a squad.", true);
        }*/
    }
    
    void SquadAutonomyPanel::_clearAI(DataPanelLine* line)
    {
        if (_selectedSquad)
        {
            auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad);
            if (settings)
            {
                settings->clearPackages();
                refresh();
            }
        }
        else
        {
            DebugLog("Clear AI: No squad selected");
        }
    }


    bool SetAI(ActivePlatoon* platoon, std::map<int, lektor<GameData*>> aiPackages, bool addToList)
    {
        if (!platoon || !platoon->me->getFaction()->isPlayer)
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
            return false;
        }
        if (aiPackages.size() < 1)
        {
            ou->showPlayerAMessage("No AI packages to enable", true);
            return false;
        }
        for (auto iter = platoon->things.begin(); iter != platoon->things.end(); ++iter)
        {
            auto obj = reinterpret_cast<Character*>(*iter);
            //obj->ai->taskSystemAI->clearPermajobs();
            //obj->ai->taskSystemAI->actions.clearAndDelete();
            //obj->ai->taskSystemAI->clearSlaveJobs();
            obj->getMovement()->halt();
            obj->ai->taskSystemAI->clearOrders();
            obj->getBody()->_endAction();
            /*PlayerInterface* playerInterface = obj->getFaction()->isPlayer;
            if (playerInterface)
            {
                obj->getFaction()->isPlayer = nullptr;
                //characterPIs[obj] = playerInterface;
            }*/

        }
        Blackboard* bb = platoon->me->getBlackboard();
        bb->clearAllPackages();
        for (auto pack = aiPackages.begin(); pack != aiPackages.end(); ++pack)
        {
            auto data = pack->second;
            for (int i = 0; i < data.count; ++i)
            {
                bb->_addPackage(data[i], pack->first);
            }
        }
        //if (addToList) lektorEx::push_back_unique(autonomousPlatoons, platoon);
        return true;
    }
    bool ResetAI(ActivePlatoon* platoon, bool removeFromList)
    {
        Blackboard* blackboard;
        if (!platoon)
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
            return false;
        }

        blackboard = platoon->me->getBlackboard();
        blackboard->clearAllPackages();
        blackboard->addFallbackPackages(platoon->me->squadTemplate);
        blackboard->replaceAIPackage(platoon->me->squadTemplate);

        for (auto iter = platoon->things.begin(); iter != platoon->things.end(); ++iter)
        {
            auto obj = reinterpret_cast<Character*>(*iter);
            //obj->ai->taskSystemAI->clearPermajobs();
            //obj->ai->taskSystemAI->actions.clearAndDelete();
            //obj->ai->taskSystemAI->clearSlaveJobs();
            obj->getMovement()->halt();
            obj->ai->taskSystemAI->clearOrders();
            obj->getBody()->_endAction();
            //std::unordered_map<Character*, PlayerInterface*>::iterator it;
            /*for (auto it = characterPIs.begin(); it != characterPIs.end(); it++)
            {
                if (it->first == obj)
                {
                    obj->getFaction()->isPlayer = it->second;
                    characterPIs.erase(obj);
                }
            }*/
        }
        //if (removeFromList) lektorEx::remove(autonomousPlatoons, platoon);
        return true;
    }

    void (*ForgottenGUI_changeFontSize_orig)();
    void ForgottenGUI_changeFontSize_hook()
    {
        ForgottenGUI_changeFontSize_orig();
        if (SquadAutonomyPanel::initialized())
            SquadAutonomyPanel::getSingletonPtr()->create();

    }

    //KenshiLib doesn't have EscMenu class yet
    bool (*EscMenu_openedOtherWindows_orig)(void*);
    bool EscMenu_openedOtherWindows_hook(void* self)
    {
        auto out = EscMenu_openedOtherWindows_orig(self);
        if (out)
        {
            SquadAutonomyPanel::getSingletonPtr()->hide();
            //InformationPanel::getSingletonPtr()->hide();
        }
        else
        {
            auto settingsPanel = SquadAutonomyPanel::getSingletonPtr();
            if (settingsPanel->isVisible())
            {
                settingsPanel->hide();
                out = true;
            }
            /*auto infoPanel = SquadAutonomyPanel::getSingletonPtr();
            if (infoPanel->isVisible())
            {
                infoPanel->hide();
                out = true;
            }*/
        }
        return out;
    }

    hand* FindOptimalBarBed(Character* character)
    {
        UseableStuff* optimalBed;
        if (character)
        {
            TownBase* currentTown = character->getCurrentTownLocation();
            if (currentTown)
            {
                //find homebase beds

                //find friendly faction beds?

                //find bar beds
                //lektor<UseableStuff*> beds;
                lektor<Building*> bars = *currentTown->findAllBuildingsOfType(BD_BAR, character);
                float maxScore = -1; // std::numeric_limits<float>::min();
                for (uint32_t i = 0; i < bars.size(); ++i)
                {
                    lektor<Building*> barBeds;
                    bars[i]->findAllFurnitureWithFunction(barBeds, BF_BED);
                    for (uint32_t j = 0; j < barBeds.size(); ++j)
                    {
                        UseableStuff* b = barBeds[j]->getUseableStuff();
                        if ((b->isPublic()) && !(b->getOccupant()))
                        {
                            float distanceScore = character->ai->scoreDistanceTo(b, false);
                            if (maxScore < distanceScore)
                            {
                                maxScore = distanceScore;
                                optimalBed = b;
                            }
                            //lektorEx::push_back_unique(beds, b);
                        }
                    }
                }
            }
        }
        return &optimalBed->handle;
    }

    void FindOptimalBedAndSleep(Character* character)
    {
        if (character)
        {
            character->getMovement()->halt();
            character->ai->taskSystemAI->clearOrders();
            character->getBody()->_endAction();

            TownBase* currentTown = character->getCurrentTownLocation();
            if (currentTown != nullptr)
            {
                UseableStuff* optimalBed = nullptr;
                //find homebase beds

                //find friendly faction beds?

                //find bar beds
                //lektor<UseableStuff*> beds;
                lektor<Building*> bars = *currentTown->findAllBuildingsOfType(BD_BAR, character);
                float maxScore = -1; // std::numeric_limits<float>::min();
                for (uint32_t i = 0; i < bars.size(); ++i)
                {
                    lektor<Building*> barBeds;
                    bars[i]->findAllFurnitureWithFunction(barBeds, BF_BED);
                    for (uint32_t j = 0; j < barBeds.size(); ++j)
                    {
                        UseableStuff* b = barBeds[j]->getUseableStuff();
                        if (b->isPublic() && !(b->owner->relations->isEnemy(character->getFaction())) && (b->getOccupant() == nullptr))
                        {
                            float distanceScore = character->ai->scoreDistanceTo(b, false);
                            if (maxScore < distanceScore)
                            {
                                maxScore = distanceScore;
                                optimalBed = b;
                            }
                            //lektorEx::push_back_unique(beds, b);
                        }
                    }
                }
                if (optimalBed != nullptr)
                {
                    //this teleport the character into the bed, might be useful if I can't fix the behavior (make them walk to the bed then teleporting them in)
                    //character->setBedMode(true, optimalBed);
                    
                    //characters in faction with notARealFaction = true can't use beds
					//character->getFaction()->relations->setNoLongerEnemies(optimalBed->owner);
                    //character->getOwnerships()->addMoney(optimalBed->getCostToUse(character));
                    if (character->getFaction()->notARealFaction)
                    {
                        DebugLog("not a real faction!");
                        //character->getFaction()->notARealFaction = false;
                    }
                    character->getOrdersReciever()->addOrder(USE_BED_ORDER, optimalBed, optimalBed->getPosition(), true, false);
                    DebugLog("Character: " + character->displayName + " Money: " + Ogre::StringConverter::toString(character->getOwnerships()->getMoney()));
                    DebugLog("BedCost: " + Ogre::StringConverter::toString(optimalBed->getCostToUse(character)));
                }
            }

        }
    }
	//prioritize sleeping in owned bed, then bar's bed, then any free bed in town
    //bed->isPublic();
    //float distanceScore = selectedCharacter->ai->scoreDistanceTo(bed, false);
    //bed->getCostToUse();
	//shou->townList->getNearestTownWithBuildingDesignation(BD_BAR, selectedCharacter->getPosition(), nullptr, nullptr, selectedCharacter->getFaction(), TOWN_TOWN, 
    //selectedCharacter->ai->findNearestHomeBase();

    void SetAiSleep(MyGUI::WidgetPtr sender)
    {

        Character* selectedCharacter = gui->selectedObject.getCharacter();
		if (selectedCharacter)
		{
			FindOptimalBedAndSleep(selectedCharacter);
		}
		else
		{
			ErrorLog("Autonomy: Please select a character.");
		}
    }


  

    void OpenSquadAutonomyPanel(ActivePlatoon * platoon)
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

    void ForceAction(MyGUI::WidgetPtr sender)
    {
        Character* selectedCharacter = gui->selectedObject.getCharacter();
        if (selectedCharacter)
        {
            selectedCharacter->addGoal(GO_HOME_AND_GO_TO_BED, selectedCharacter);
        }
    }

    void MarkCharacter(MyGUI::WidgetPtr sender)
    {
        Character* selectedCharacter = gui->selectedObject.getCharacter();
        if (selectedCharacter)
        {
            //lektorEx::push_back_unique(autonomousPlatoons, selectedCharacter->platoon);
        }
    }

    void ClearCharacter(MyGUI::WidgetPtr sender)
    {
        Character* selectedCharacter = gui->selectedObject.getCharacter();
        if (selectedCharacter)
        {
            //autonomousPlatoons.clear();
        }
    }

    class AutonomyButton
    {
    public:
        MyGUI::Button* button;
        ActivePlatoon* squad;
        AutonomyButton(MyGUI::Button* b, ActivePlatoon* s)
        {
            button = b;
            squad = s;
            button->eventMouseButtonClick += MyGUI::newDelegate(this, &AutonomyButton::OnClick);
        }
        void OnClick(MyGUI::WidgetPtr sender)
        {
            OpenSquadAutonomyPanel(squad);
        }
    private:
    };
    std::unordered_set<SquadManagementScreen::SquadCellView*> autButtons;
    void (*SquadCellView_update_orig)(SquadManagementScreen::SquadCellView* thisptr, const MyGUI::IBDrawItemInfo& _info, SquadManagementScreen::SquadData* _data);
    void SquadCellView_update_hook(SquadManagementScreen::SquadCellView* thisptr, const MyGUI::IBDrawItemInfo& _info, SquadManagementScreen::SquadData* _data)
    {
        SquadCellView_update_orig(thisptr, _info, _data);

        if (autButtons.size() > 0 && autButtons.find(thisptr) != autButtons.end()) return;
        DebugLog("Create AUT button");
        MyGUI::Widget* parent = thisptr->txtName->getParent();
        int left = thisptr->txtName->getRight() ;
        int width = thisptr->txtSquadSize->getLeft() - left;
        int top = thisptr->txtName->getTop();
        int height = thisptr->txtName->getHeight();
        DebugLog(Ogre::StringConverter::toString(left) + ", " + Ogre::StringConverter::toString(width) +
            ", " + Ogre::StringConverter::toString(top) + ", " + Ogre::StringConverter::toString(height));
        MyGUI::Button* autonomyButton = parent->createWidgetReal<MyGUI::Button>("Kenshi_Button1", (float)left / parent->getWidth() + 0.05, 
            (float)top / parent->getHeight(), (float)width / parent->getWidth() - 0.1, (float)height / parent->getHeight(), MyGUI::Align::Center, "AutonomyButton");
        autonomyButton->setCaption("AUT");
        AutonomyButton* autButton = new AutonomyButton(autonomyButton, _data->platoon);
        autButtons.insert(thisptr);
    }

    /*MainBarGUI* (*MainbarGUICONSTRUCTOR_orig)(MainBarGUI* thisptr);
    MainBarGUI* MainbarGUICONSTRUCTOR_hook(MainBarGUI* thisptr)
    {
        MainBarGUI* orig = MainbarGUICONSTRUCTOR_orig(thisptr);
        //put the button relative to panel position?
        //MyGUI::TabControl* tab = orig->portraitsTabPanel;
		MyGUI::Widget* root = MyGUI::Gui::getInstancePtr()->createWidgetReal<MyGUI::Widget>("PanelEmpty", 0.68, 0.828, 0.0234, 0.0231, MyGUI::Align::Stretch, "Modal", "AutonomyButtonPanel");
        MyGUI::Button* autonomyButton = root->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.0, 0.0, 1.0, 1.0, MyGUI::Align::Center, "AutonomyButton");
        autonomyButton->setCaption("AUT");
        autonomyButton->eventMouseButtonClick += MyGUI::newDelegate(OpenSquadAutonomyPanel);
        MyGUI::Window* window = MyGUI::Gui::getInstancePtr()->createWidgetReal<MyGUI::Window>("Kenshi_WindowCX", 0.25, 0.25, 0.30, 0.33, MyGUI::Align::Center, "Window", "DebugWindow");
        MyGUI::Button* checkButton = window->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.25, 0.0, 0.5, 0.1, MyGUI::Align::Center, "CheckButton");
        checkButton->setCaption("Check");
        checkButton->eventMouseButtonClick += MyGUI::newDelegate(CheckInfo);
        MyGUI::Button* forceButton = window->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.25, 0.1, 0.5, 0.1, MyGUI::Align::Center, "ForceButton");
        forceButton->setCaption("Force");
        forceButton->eventMouseButtonClick += MyGUI::newDelegate(ForceAction);
        MyGUI::Button* markButton = window->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.25, 0.2, 0.5, 0.1, MyGUI::Align::Center, "MarkButton");
        markButton->setCaption("Mark");
        markButton->eventMouseButtonClick += MyGUI::newDelegate(MarkCharacter);
        MyGUI::Button* clearButton = window->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.25, 0.3, 0.5, 0.1, MyGUI::Align::Center, "clearButton");
        clearButton->setCaption("clear");
        clearButton->eventMouseButtonClick += MyGUI::newDelegate(ClearCharacter);
        return orig;
    }*/

    /*SquadManagementScreen* smsPtr;

    void (*update_orig)(SquadManagementScreen* thisptr);
    void update_hook(SquadManagementScreen* thisptr)
    {
        update_orig(thisptr);
        smsPtr = thisptr;
    }*/


    int (*saveGame_orig)(SaveManager* thisptr, const std::string& location, const std::string& name);
    int saveGame_hook(SaveManager* thisptr, const std::string& location, const std::string& name)
    {
        //save the settings into mod folder to init when load
        // clear all packages from all player's squads so they can safely remove the mod
        // return the playerinterface to all player members
        //std::unordered_map<Character*, PlayerInterface*>::iterator it;
        /*auto settings = SquadAutonomySettings::getSingletonPtr()->squadSettings;

        for (int i = 0; i < settings.size(); ++i)
        {
            ActivePlatoon * platoon = settings[i]->getSquad();
            if (platoon)
            {
                ResetAI(settings[i]->getSquad(), false);
            }
        }*/
        //DebugLog(location + name);
        saveGame_orig(thisptr, location, name);
        //DebugLog("Save code: " + Ogre::StringConverter::toString(saveError));
        /*if (saveError == 0)
        {
            for (int i = 0; i < settings.size(); ++i)
            {
                ActivePlatoon* platoon = settings[i]->getSquad();
                if (platoon)
                {
                    SetAI(settings[i]->getSquad(), settings[i]->getSquadPackages(), false);
                }
            }

        }
        return saveError;*/
    }

    int (*loadGame_orig)(SaveManager* thisptr, const std::string& location, const std::string& name);
    int loadGame_hook(SaveManager* thisptr, const std::string& location, const std::string& name)
    {
        // load settings or create .settings file if not exists
        //DebugLog(location + name);
        return loadGame_orig(thisptr, location, name);
    }
    // Hook the method (call interception) — the primary mod mechanism
    void (*currentActionChecks_orig)(AITaskSytem* thisptr);
    void currentActionChecks_hook(AITaskSytem* thisptr)
    {
        Character* character = thisptr->character;
        SquadSettingsInfo* settings;
        PlayerInterface* pi;
        if (character)
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(thisptr->character->getPlatoon());
            if (settings && settings->getEnabled())
            {
                pi = thisptr->character->getFaction()->isPlayer;
                thisptr->character->getFaction()->isPlayer = nullptr;
            }
        }
        currentActionChecks_orig(thisptr);
        if (settings && settings->getEnabled())
        {
            thisptr->character->getFaction()->isPlayer = pi;
        }
    }

    // Hook the method (call interception) — the primary mod mechanism
    void (*chooseGoal_orig)(AITaskSytem* thisptr, bool timedLockOnCurrentGoal);
    void chooseGoal_hook(AITaskSytem* thisptr, bool timedLockOnCurrentGoal)
    {
        Character* character = thisptr->character;
        SquadSettingsInfo* settings;
        PlayerInterface* pi;
        if (character)
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(thisptr->character->getPlatoon());
            if (settings && settings->getEnabled())
            {
                pi = thisptr->character->getFaction()->isPlayer;
                thisptr->character->getFaction()->isPlayer = nullptr;
            }
        }

        chooseGoal_orig(thisptr, timedLockOnCurrentGoal);

        if (settings && settings->getEnabled())
        {
            thisptr->character->getFaction()->isPlayer = pi;
        }

    }
  
}

bool (*EscMenu_openedOtherWindows)(class EscMenu*);

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
            *(uintptr_t*)&EscMenu_openedOtherWindows = baseAddr + 0x916250;
        }
        else if (platform == 0)
        {
            *(uintptr_t*)&EscMenu_openedOtherWindows = baseAddr + 0x915970;
        }
    }
    SquadAutonomy::SquadAutonomySettings::getSingletonPtr();
    DebugLog("Autonomy: Mod started");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&ForgottenGUI::changeFontSize), &SquadAutonomy::ForgottenGUI_changeFontSize_hook, &SquadAutonomy::ForgottenGUI_changeFontSize_orig))
        ErrorLog("Autonomy: Could not add hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&MainBarGUI::_CONSTRUCTOR), &SquadAutonomy::MainbarGUICONSTRUCTOR_hook, &SquadAutonomy::MainbarGUICONSTRUCTOR_orig))
        ErrorLog("Autonomy: Could not add hook!");*/
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::saveGame), &SquadAutonomy::saveGame_hook, &SquadAutonomy::saveGame_orig))
		ErrorLog("Autonomy: Could not add hook!");
	if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::loadGame), &SquadAutonomy::loadGame_hook, &SquadAutonomy::loadGame_orig))
		ErrorLog("Autonomy: Could not add hook!");*/
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SquadManagementScreen::SquadCellView::update), &SquadAutonomy::SquadCellView_update_hook, &SquadAutonomy::SquadCellView_update_orig))
        ErrorLog("Autonomy: Could not add hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::chooseGoal) , &SquadAutonomy::chooseGoal_hook, &SquadAutonomy::chooseGoal_orig))
        ErrorLog("Autonomy: Could not add EscMenu hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::currentActionChecks), &SquadAutonomy::currentActionChecks_hook, &SquadAutonomy::currentActionChecks_orig))
        ErrorLog("Autonomy: Could not add EscMenu hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(EscMenu_openedOtherWindows, &SquadAutonomy::EscMenu_openedOtherWindows_hook, &SquadAutonomy::EscMenu_openedOtherWindows_orig))
        ErrorLog("Autonomy: Could not add hook!");
    KenshiLib::ApplyQueuedHooks();
}

/*
	std::string LogTaskDataInfo(Tasker* tasker)
	{
		if (tasker)
		{
			TaskData* taskData = tasker->taskData;
			if (taskData)
			{
				std::string description = tasker->getDescription();
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
                return (description + " Duration Min: " + Ogre::StringConverter::toString(durationMin) + " Duration Fuzz: " + Ogre::StringConverter::toString(durationFuzz) +
                    " Is Duration Based: " + Ogre::StringConverter::toString(isDurationBased) +
                    " Infrequent Goal Checks: " + Ogre::StringConverter::toString(infrequentGoalChecks) +
                    " Ends After Time: " + Ogre::StringConverter::toString(endsAfterTime) +
                    " Is Unstoppable Task: " + Ogre::StringConverter::toString(isUnstoppable) +
                    " For Direct Player Orders: " + Ogre::StringConverter::toString(forDirectPlayerOrdersOnly) +
                    " For Fulfil Player Orders or NPC: " + Ogre::StringConverter::toString(forFulfillPlayerOrdersOrNPCOnly) +
                    " Can't End Prematurely: " + Ogre::StringConverter::toString(cantEndPrematurely) +
                    " Is Permajob: " + Ogre::StringConverter::toString(isPermaJob));
			}
		}
		return "";
	}

    void CheckInfo(MyGUI::WidgetPtr sender)
    {
        //double currentHour = std::fmod(ou->getTimeStamp_inGameHours().getTotalHours(),24);

        //double totalHour = ou->getTimeStamp_inGameHours().getTotalHours();
        //DebugLog("Autonomy: Total hour: " + std::to_string((long long)currentHour));
        Character* selectedCharacter = gui->selectedObject.getCharacter();
        if (selectedCharacter)
        {
            //Tasker* tasker = selectedCharacter->getBody()->getCurrentAction();

            auto requestsList = selectedCharacter->ai->taskSystemAI->requestsList;
            auto list = selectedCharacter->ai->taskSystemAI->actions.list;

            //if (tasker1) lektorEx::push_back(taskers, tasker1);

            //std::map<float, Tasker*>::iterator it;
            //for (auto it = orderedGoals.begin(); it != orderedGoals.end(); it++)
            DebugLog("Character: " + selectedCharacter->displayName);
            Blackboard* bb = selectedCharacter->getBlackboard();
            if (bb)
            {
                DebugLog("Current Package: " + selectedCharacter->getBlackboard()->getCurrentAIPackageName());
                auto umapPackages = bb->packagesMain;
                //boost::unordered::unordered_map<int, lektor<AIPackage*>>::iterator it;
                for (auto it = umapPackages.begin(); it != umapPackages.end(); it++)
                {
                    DebugLog("Priority: " + Ogre::StringConverter::toString(it->first));
                    for (int i = 0; i < it->second.size(); i++)
                    {
                        DebugLog("PackageName: " + it->second[i]->packageData->name);
                    }
                }
            }

            for (uint32_t i = 0; i < list.size(); ++i)
            {
                Tasker* tasker = list[i];
                DebugLog("Task : " + LogTaskDataInfo(tasker));
            }
            auto tryList = selectedCharacter->ai->taskSystemAI->actionsTryList.list;

            for (uint32_t i = 0; i < tryList.size(); ++i)
            {
                Tasker* tasker = tryList[i];
                DebugLog("Try Task : " + LogTaskDataInfo(tasker));
            }
            auto taskaftercurrent = selectedCharacter->ai->taskSystemAI->actions.getTaskAfterCurrent();
            auto taskaftercurrent2 = selectedCharacter->ai->taskSystemAI->actions.get2TasksAfterCurrent();
            if (taskaftercurrent) {
                DebugLog("Task after current: " + LogTaskDataInfo(taskaftercurrent));
            }
            if (taskaftercurrent2) {
                DebugLog("Task 2 after current: " + LogTaskDataInfo(taskaftercurrent2));
            }
        }
        else
        {
            ErrorLog("Autonomy: Please select a character.");
        }
    }
*/

