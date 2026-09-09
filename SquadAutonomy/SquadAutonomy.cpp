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
    std::string saveName = "SquadAutonomy.save";
    std::string savePath;
    bool SetAI(Platoon*, std::map<int, lektor<GameData*>>);
    bool ResetAI(Platoon*);
    bool shouldSave = false;
    bool shouldLoad = false;
    class SquadSettingsInfo
    {
    public:
        SquadSettingsInfo(Platoon* squad) : _enabled(false), _squad(squad), _homeBuilding(nullptr), _workBuilding(nullptr)
        {

        }
        Platoon* getSquad()
        {
            return _squad;
        }
        bool isEnabled()
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
        void setPackages(const std::map<int, lektor<GameData*> >& packages)
        {
            _squadPackages.clear();
            for (auto it = packages.begin(); it != packages.end(); ++it)
            {
                auto gameDatas = it->second;
                auto find = _squadPackages.find(it->first);
                if (_squadPackages.size() > 0 && find != _squadPackages.end())
                {
                    for (int i = 0; i < gameDatas.size(); ++i)
                    {
                        lektorEx::push_back_unique(find->second, gameDatas[i]);
                    }
                }
                else
                {
                    lektor<GameData*> dataList;
                    for (int i = 0; i < gameDatas.size(); ++i)
                    {
                        lektorEx::push_back_unique(dataList, gameDatas[i]);
                    }
                    _squadPackages[it->first] = dataList;
                }
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

                if (success)
                {
                    DebugLog("Autonomy enable!");
                    _enabled = enable;
                    if (_enabled)
                    {
                        if (!assignSquadHome(false)) //if fail to set home to work
                        {
                            assignSquadHome(true); //set home to home
                        }
                    }
                    else
                    {
                        unassignSquadHome();
                    }
                }
            }
            return success;
        }
        void updateSquadPackages()
        {
            if (!_enabled) return;
            
            if (_squadPackages.size() > 0)
            {
                SetAI(_squad, _squadPackages);
                if (!assignSquadHome(false)) //if fail to set home to work
                {
                    assignSquadHome(true); //set home to home
                }
            }
            else
            {
                ResetAI(_squad);
                unassignSquadHome();
            }

        }

        Building* getBuilding(bool home)
        {
            if (home) return _homeBuilding;
            return _workBuilding;
        }

        void setBuilding(Building* building, bool home)
        {
            if (home) _homeBuilding = building;
            else _workBuilding = building;
        }

        bool assignSquadHome(bool home)
        {
            if (home)
            {
                if (_homeBuilding)
                {
                    _squad->getOwnerships()->setHomeBuilding(_homeBuilding, _squad->getSquadType());
                    TownBase* town = _homeBuilding->getTown();
                    if (town)
                    {
                        _squad->getOwnerships()->setHomeTown(town, _squad->getSquadType());
                    }
                    return true;
                }
            }
            else
            {
                if (_workBuilding)
                {
                    _squad->getOwnerships()->setHomeBuilding(_workBuilding, _squad->getSquadType());
                    TownBase* town = _workBuilding->getTown();
                    if (town)
                    {
                        _squad->getOwnerships()->setHomeTown(town, _squad->getSquadType());
                    }
                    return true;
                }
            }
            return false;
        }
        void unassignSquadHome()
        {
            _squad->getOwnerships()->setHomeBuilding(nullptr, _squad->getSquadType());
            _squad->getOwnerships()->setHomeTown(nullptr, _squad->getSquadType());
        }

    private:
        Platoon* _squad;
        bool _enabled;
        std::map<int, lektor<GameData*>> _squadPackages;
        //TownBase* _homeTown;
        Building* _homeBuilding;
        //TownBase* _workTown;
        Building* _workBuilding;
    };

    class SquadAutonomySettings
    {
    public:
        static SquadAutonomySettings* getSingletonPtr();
        static bool initialized;
        lektor<SquadSettingsInfo*> squadSettings;

        SquadAutonomySettings() : _cfgPath("SquadAutonomy.cfg")
        {
            _loadConfig();
            _initAIPackageList();
            initialized = true;
        }

        bool loadSettings(std::string savePath)
        {
            //initialized = false;
            DebugLog("Finding settings file at " + savePath);
            std::ifstream saveFile(savePath);
            if (!saveFile.is_open())
            {
                DebugLog("Cannot open save file");
                return false;
            }

            std::string line;
            std::string type;

            while (std::getline(saveFile, line))
            {
                line.erase(0, line.find_first_not_of(" \t"));
                size_t colon = line.find(':');

                if (colon == std::string::npos)
                {
                    continue;
                }
                type = line.substr(0, colon);
                DebugLog(type);
                //get squad
                if (type == "Squad")
                {
                    std::string dataLine;
                    bool enable = false;
                    Platoon* squad = nullptr;
                    Building* home = nullptr;
                    Building* work = nullptr;
                    std::map<int, lektor<GameData*>> packages;
                    dataLine = line.substr(colon + 1);
                    hand* hand = createHandfromLine(dataLine);
                    if (hand) squad = hand->getPlatoon();
                    else continue;

                    //get home buildings and packages
                    while (std::getline(saveFile, line))
                    {
                        line.erase(0, line.find_first_not_of(" \t"));
                        colon = line.find(':');
                        if (colon == std::string::npos)
                        {
                            continue;
                        }

                        type = line.substr(0, colon);
                        DebugLog(type);
                        if (type == "EndSquad") break;
                        if (type == "Enable")
                        {
                            dataLine = line.substr(colon + 1);
                            dataLine.erase(0, dataLine.find_first_not_of(" \t"));
                            DebugLog(dataLine);
                            if (dataLine == "true")
                            {
                                enable = true;
                            }
                            continue;
                        }
                        if (type == "HomeBuilding")
                        {
                            dataLine = line.substr(colon + 1);
                            hand = createHandfromLine(dataLine);
                            if (hand)
                            {
                                DebugLog("Home hand created!");
                                home = hand->getBuilding();
                            }
                            continue;
                        }
                        if (type == "WorkBuilding")
                        {
                            dataLine = line.substr(colon + 1);
                            hand = createHandfromLine(dataLine);
                            if (hand)
                            {
                                DebugLog("Work hand created!");
                                work = hand->getBuilding();
                            }
                           continue;
                        }
                        if (type == "Packages")
                        {
                            while (std::getline(saveFile, line))
                            {
                                line.erase(0, line.find_first_not_of(" \t"));
                                int priority;
                                colon = line.find(':');
                                if (colon == std::string::npos)
                                {
                                    continue;
                                }
                                type = line.substr(0, colon);
                                DebugLog(type);
                                if (type == "EndPackages") break;
                                std::stringstream ss(type);
                                if (!(ss >> priority))
                                {
                                    continue;
                                }
                                size_t start = line.find('<', colon);
                                size_t end = line.rfind('>');

                                if (start != std::string::npos && end > start)
                                {
                                    std::string packageName = line.substr(start + 1, end - start - 1);
                                    for (int i = 0; i < _AIPackageList.size(); ++i)
                                    {
                                        if (packageName == _AIPackageList[i]->name)
                                        {
                                            auto find = packages.find(priority);
                                            if (packages.size() > 0 && find != packages.end())
                                            {
                                                lektorEx::push_back_unique(find->second, _AIPackageList[i]);
                                            }
                                            else
                                            {
                                                lektor<GameData*> dataList;
                                                lektorEx::push_back_unique(dataList, _AIPackageList[i]);
                                                packages[priority] = dataList;
                                            }
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    //save current squad data
                    if (squad)
                    {
                        DebugLog("Load Squad " + squad->activePlatoon->getName());
                        SquadSettingsInfo* settingsInfo = new SquadSettingsInfo(squad);
                        if (home)
                        {
                            settingsInfo->setBuilding(home, true);
                            DebugLog("Load home: " + home->displayName);
                        }
                        if (work)
                        {
                            settingsInfo->setBuilding(work, false);
                            DebugLog("Load home: " + work->displayName);
                        }
                        if (packages.size() > 0)
                        {
                            settingsInfo->setPackages(packages);
                        }
                        settingsInfo->enableAutonomy(enable);
                        lektorEx::push_back(squadSettings, settingsInfo);
                    }
                }
            }
            saveFile.close();
            //initialized = true;
            return true;
        }

        hand* createHandfromLine(std::string line)
        {

            size_t start = line.rfind('>');

            if (start != std::string::npos)
            {
                std::string numbers = line.substr(start + 1);
                DebugLog(numbers);
                std::istringstream iss(numbers);
                
                int type;
                unsigned int container;
                unsigned int containerSerial;
                unsigned int index;
                unsigned int serial;

                iss >> index
                    >> serial
                    >> type
                    >> container
                    >> containerSerial;
                return new hand(index, serial, static_cast<itemType>(type), container, containerSerial);
            }
            return nullptr;

        }

        SquadSettingsInfo* getSquadSettings(Platoon* squad, bool createNew = false)
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
                SquadSettingsInfo* newSettings = new SquadSettingsInfo(squad);
                lektorEx::push_back(squadSettings, newSettings);
                return newSettings;
            }
            return nullptr;
        }
        lektor<GameData*>* getAIPackageList()
        {
            return &_AIPackageList;
        }

    private:
        lektor<GameData*> _AIPackageList;
        lektor<std::string> _cfgPackageList;
        std::string _cfgPath;
        void _loadConfig();
        void _initAIPackageList();
    };
    SquadAutonomySettings* SquadAutonomySettings::getSingletonPtr()
    {
        static boost::scoped_ptr<SquadAutonomySettings> singleton(new SquadAutonomySettings());
        return singleton.get();
    }
    bool SquadAutonomySettings::initialized = false;
    void SquadAutonomySettings::_loadConfig()
    {
        _cfgPath = GetCurrentDLLDirectory() + "SquadAutonomy.cfg";
        DebugLog("Config file is at: " + _cfgPath);
        DebugLog("Reading config file");
        std::fstream cfgFile(_cfgPath, std::fstream::in | std::fstream::out | std::fstream::app);
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

    void SquadAutonomySettings::_initAIPackageList()
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
            else if (_cfgPackageList.size() > 0)
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

    //lektor<ActivePlatoon*> autonomousPlatoons;
    //std::unordered_map<Character*, PlayerInterface*> characterPIs;

    //based on KEP dev tools panel
    class SquadAutonomyPanel
    {
    public:
        static SquadAutonomyPanel* getSingletonPtr();
        static bool initialized;
        SquadAutonomyPanel();
        //~SquadAutonomyPanel();

        void create();
        void refresh();
        void show();
        void hide();
        bool isVisible();
        void selectSquad(Platoon*);

    private:
        int _category;
        Platoon* _selectedSquad;
        void _toggleAI(DataPanelLine* line); //button for enabling/setting packages to a squad
        void _addAI(DataPanelLine* line); //set the packages in the setting
        void _clearAI(DataPanelLine* line); //clear all packages in the setting
        void _setHome(DataPanelLine* line);
        void _setWork(DataPanelLine* line);
        void _clearBuildings(DataPanelLine* line);
        void _setBuilding(bool home);

        void _changeAIPackageSearchText(DataPanelLine* line);
        void _updateAIPackageList(const std::string& keyword);

        DatapanelGUI* _panel;
        
        int _selectedAIPackageIndex;
        float _priority;

        void SetButton(const std::string& caption, int cat, float width, void (SquadAutonomyPanel::* callback)(DataPanelLine*));
        void SetDropBox(const std::string& caption, int cat, float width, int* valPtr, void (SquadAutonomyPanel::* callback)(DataPanelLine*));
        void SetEditBox(const std::string& caption, int cat, float width, const std::string& text, void (SquadAutonomyPanel::* callback)(DataPanelLine*));
    };

    bool SquadAutonomyPanel::initialized = false;
    std::string lineBoxAIPackage;

    void initLineKey()
    {
        lineBoxAIPackage = "AI package";
    }

    SquadAutonomyPanel* SquadAutonomyPanel::getSingletonPtr()
    {
        static boost::scoped_ptr<SquadAutonomyPanel> singleton(new SquadAutonomyPanel());
        return singleton.get();
    }

    SquadAutonomyPanel::SquadAutonomyPanel() : _selectedSquad(nullptr), _panel(nullptr), _selectedAIPackageIndex(0), _category(0), _priority(0.0f)
    {
        initLineKey();
        create();
        initialized = true;
    }

    void SquadAutonomyPanel::create()
    {
        if (this->_panel != nullptr)
        {
            this->_panel->show(false);
            gui->destroy(this->_panel);
        }

        this->_panel = gui->createDatapanel(0.2f, 0.35f, 0.3f, 0.6f, true, "Window", true);
        this->_panel->setCaption("Squad Autonomy");
        this->_panel->setPanelName("SquadAutonomy");

        refresh();

        this->_panel->show(false);
    }

    void SquadAutonomyPanel::refresh()
    {
        if (this->_panel == nullptr)
            return;
        if (!_selectedSquad) return;
        if (!SquadAutonomySettings::initialized) return;
        /*if (shouldLoad)
        {
            SquadAutonomySettings::getSingletonPtr()->loadSettings(savePath);
            shouldLoad = false;
        }*/

        this->_panel->setLineSpacing(32.0f);
        this->_panel->clearPage(this->_category);
        auto squadSettings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
        this->_panel->setCaption("Squad Autonomy: " + _selectedSquad->activePlatoon->getName());
        
        DataPanelLine_Text* textbox;
        
        auto button = this->_panel->setLineToggleButton("", "Enable Autonomy", this->_category);
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, &SquadAutonomyPanel::_toggleAI);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) /button->button->getParent()->getHeight());
        if (squadSettings)
        {
            button->button->setStateSelected(squadSettings->isEnabled());
        }
        this->_panel->addSpace(this->_category, 0.25f);

        button = this->_panel->setLineTextButton("", "Set Squad Home Building", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, &SquadAutonomyPanel::_setHome);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

        button = this->_panel->setLineTextButton("", "Set Squad Work Building", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, &SquadAutonomyPanel::_setWork);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
        button = this->_panel->setLineTextButton("", "Clear Buildings", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, &SquadAutonomyPanel::_clearBuildings);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
        this->_panel->addSpace(this->_category, 0.25f);

        auto editbox = this->_panel->setLineTextEditable("Search", "", this->_category, true, false, MyGUI::Align::Left, 0.95f);
        editbox->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, &SquadAutonomyPanel::_changeAIPackageSearchText);

        auto dropbox = this->_panel->setLineDropBox(lineBoxAIPackage, this->_category, &this->_selectedAIPackageIndex, false, 1.0f);

        _updateAIPackageList("");
        editbox->getEditBox()->setSize(dropbox->listBox->getSize());
        this->_panel->addSpace(this->_category, 0.25f);

        auto slider = this->_panel->setLineSliderEditable("Priority", this->_category, true, 0.0f, 5.0f, &this->_priority);
        slider->nameText->setEnabled(false);
        slider->setPrecision(0);
        this->_panel->addSpace(this->_category, 0.25f);

        button = this->_panel->setLineTextButton("", "Add AI Package", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, &SquadAutonomyPanel::_addAI);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

        button = this->_panel->setLineTextButton("", "Clear AI Packages", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), this, &SquadAutonomyPanel::_clearAI);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
        this->_panel->addSpace(this->_category, 0.25f);
        if (squadSettings)
        {
            Building* building = squadSettings->getBuilding(true);
            TownBase* town;
            if (building)
            {
                std::string townText;
                textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left);
                textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
                townText = "Home: " + building->displayName;
                town = building->getTown();
                if (town)
                {
                    townText += ", " + town->getKnownName();
                }
                textbox->editBox->setCaption(townText);
            }
            building = squadSettings->getBuilding(false);
            if (building)
            {
                std::string townText;
                textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left);
                textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
                townText = "Work: " + squadSettings->getBuilding(false)->displayName;
                town = building->getTown();
                if (town)
                {
                    townText += ", " + town->getKnownName();
                }
                textbox->editBox->setCaption(townText);
            }
            
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
                        textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left);
                        textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
                        textbox->editBox->setCaption(Ogre::StringConverter::toString(it->first) + " " + packages[i]->name);
                    }
                }
            }
        }
        textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left); //buffer text for scrolling (dont know why it doesn't fit)
        this->_panel->addSpace(this->_category, 1.0f);

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

    void SquadAutonomyPanel::selectSquad(Platoon* squad)
    {
        _selectedSquad = squad;
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
        if (!SquadAutonomySettings::initialized) return;
        SquadAutonomySettings* settings = SquadAutonomySettings::getSingletonPtr();
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
            for (uint32_t i = 0; i < settings->getAIPackageList()->size(); ++i)
            {
                dropBox->addAValue(settings->getAIPackageList()->at(i)->name, i);
            }
        }
        else
        {
            std::string s1 = keyword;
            std::transform(s1.begin(), s1.end(), s1.begin(), [](char c) { return std::toupper(c); });
            for (uint32_t i = 0; i < settings->getAIPackageList()->size(); ++i)
            {
                std::string& name = settings->getAIPackageList()->at(i)->name;
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
        if (!SquadAutonomySettings::initialized) return;
        if (_selectedSquad) 
        {
            auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
            if (line != nullptr && line->classType == DataPanelLine::DPL_BUTTON)
            {
                auto button = reinterpret_cast<DataPanelLine_Button*>(line)->button;
                if (settings && settings->enableAutonomy(!button->getStateSelected()))
                {
                    refresh();
                }
            }
        }
        else
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
        }
    }

    void SquadAutonomyPanel::_addAI(DataPanelLine* line)
    {
        if (!SquadAutonomySettings::initialized) return;
        if (_selectedSquad)
        {
            GameData* data;
            SquadAutonomySettings* settings = SquadAutonomySettings::getSingletonPtr();
            SquadSettingsInfo* settingsInfo = settings->getSquadSettings(_selectedSquad, true);
            if (_selectedAIPackageIndex < settings->getAIPackageList()->size()) data = settings->getAIPackageList()->at(_selectedAIPackageIndex);
            if (data && settingsInfo)
            {
                settingsInfo->addPackage(static_cast<int>(_priority), data);
                DebugLog("Add " + data->name + " package");

                settingsInfo->updateSquadPackages();

                refresh();
            }
        }
        else
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
        }
    }
    
    void SquadAutonomyPanel::_clearAI(DataPanelLine* line)
    {
        if (!SquadAutonomySettings::initialized) return;
        if (_selectedSquad)
        {
            auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
            if (settings)
            {
                settings->clearPackages();

                settings->updateSquadPackages();

                refresh();
            }
        }
        else
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
        }
    }

    void SquadAutonomyPanel::_setHome(DataPanelLine* line)
    {
        _setBuilding(true);
    }

    void SquadAutonomyPanel::_setWork(DataPanelLine* line)
    {
        _setBuilding(false);
    }

    void SquadAutonomyPanel::_clearBuildings(DataPanelLine* line)
    {
        if (!SquadAutonomySettings::initialized) return;
        if (!_selectedSquad)
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
            return;
        }
        auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
        settings->setBuilding(nullptr, true);
        settings->setBuilding(nullptr, false);
        ou->showPlayerAMessage("Clear buildings", true);
        refresh();

    }

    void SquadAutonomyPanel::_setBuilding(bool home)
    {
        if (!SquadAutonomySettings::initialized) return;
        if (!_selectedSquad)
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
            return;
        }
        if (!gui->selectedObject)
        {
            ou->showPlayerAMessage("No object selected!", true);
            return;
        }
        auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);

        Building* building = gui->selectedObject.getBuilding();
        if (!building)
        {
            ou->showPlayerAMessage("Select a building!", true);
            return;
        }
        if (building->isFurnitureOrDoor())
        {
            if (building->isDoor())
            {
                DebugLog("Is a door of " + building->doorParentBuilding()->displayName);
                building = building->doorParentBuilding();
            }
            else if (building->isFurniture())
            {
                DebugLog("Is a furniture of " + building->furnitureParentBuilding()->displayName);
                building = building->furnitureParentBuilding();
            }
        }

        //_selectedSquad->me->getOwnerships()->setHomeBuilding(building, _selectedSquad->me->getSquadType());
        settings->setBuilding(building, home);
        std::string report = "Set " + _selectedSquad->displayName + " home building: " + building->displayName;
        TownBase* town = building->getCurrentTownLocation();
        if (town)
        {
            //_selectedSquad->me->getOwnerships()->setHomeTown(town, _selectedSquad->me->squadType);
            report += ", " + town->getKnownName();
        }
        ou->showPlayerAMessage(report, true);

        settings->updateSquadPackages();
        refresh();
    }

    bool SetAI(Platoon* platoon, std::map<int, lektor<GameData*>> aiPackages)
    {
        DebugLog("Set AI");
        if (!platoon || !platoon->getFaction()->isPlayer)
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
            return false;
        }
        if (aiPackages.size() < 1)
        {
            ou->showPlayerAMessage("No AI packages to enable", true);
            return false;
        }
        for (auto iter = platoon->activePlatoon->things.begin(); iter != platoon->activePlatoon->things.end(); ++iter)
        {
            auto obj = reinterpret_cast<Character*>(*iter);
            obj->getMovement()->halt();
            obj->ai->taskSystemAI->clearOrders();
            obj->getBody()->_endAction();
        }
        Blackboard* bb = platoon->getBlackboard();
        bb->clearAllPackages();
        for (auto pack = aiPackages.begin(); pack != aiPackages.end(); ++pack)
        {
            auto data = pack->second;
            for (int i = 0; i < data.count; ++i)
            {
                bb->_addPackage(data[i], pack->first);
                DebugLog("Adding " + data[i]->name + " to " + platoon->activePlatoon->getName());
            }
        }
        return true;
    }
    bool ResetAI(Platoon* platoon)
    {
        DebugLog("Reset AI");
        Blackboard* blackboard;
        if (!platoon)
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
            return false;
        }

        blackboard = platoon->getBlackboard();
        blackboard->clearAllPackages();
        blackboard->addFallbackPackages(platoon->squadTemplate);
        blackboard->replaceAIPackage(platoon->squadTemplate);

        for (auto iter = platoon->activePlatoon->things.begin(); iter != platoon->activePlatoon->things.end(); ++iter)
        {
            auto obj = reinterpret_cast<Character*>(*iter);
            obj->getMovement()->halt();
            obj->ai->taskSystemAI->clearOrders();
            obj->getBody()->_endAction();
        }
        return true;
    }

    void (*ForgottenGUI_changeFontSize_orig)();
    void ForgottenGUI_changeFontSize_hook()
    {
        ForgottenGUI_changeFontSize_orig();
        if (SquadAutonomyPanel::initialized)
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



    void OpenSquadAutonomyPanel(Platoon * platoon)
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



    class AutonomyButton
    {
    public:
        MyGUI::Button* button;
        Platoon* squad;
        AutonomyButton(MyGUI::Button* b, Platoon* s)
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
        MyGUI::Button* autonomyButton = parent->createWidgetReal<MyGUI::Button>("Kenshi_Button1", static_cast<float>(left) / parent->getWidth() + 0.05, 
            static_cast<float>(top) / parent->getHeight(), static_cast<float>(width) / parent->getWidth() - 0.1, static_cast<float>(height) / parent->getHeight(), MyGUI::Align::Center, "AutonomyButton");
        autonomyButton->setCaption("AUT");
        AutonomyButton* autButton = new AutonomyButton(autonomyButton, _data->platoon->me);
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


    void saveSettings()
    {
        std::ofstream saveFile(savePath, std::fstream::out | std::fstream::trunc);
        DebugLog("Saving settings to " + savePath);
        if (!saveFile.is_open())
        {
            DebugLog("Cannot open save file");
        }
        else
        {
            auto settings = SquadAutonomySettings::getSingletonPtr();
            for (int i = 0; i < settings->squadSettings.size(); ++i)
            {
                auto squadSettings = settings->squadSettings[i];
                Platoon* platoon = squadSettings->getSquad();
                if (!platoon) continue;
                hand platoonHand = platoon->getHandle();
                if (platoonHand)
                {
                    saveFile << "Squad: <" << platoon->activePlatoon->getName() << "> ";
                    saveFile << platoonHand.index << ' ';
                    saveFile << platoonHand.serial << ' ';
                    saveFile << platoonHand.type << ' ';
                    saveFile << platoonHand.container << ' ';
                    saveFile << platoonHand.containerSerial << '\n';
                    saveFile << "\tEnable: " << Ogre::StringConverter::toString(squadSettings->isEnabled()) << '\n';
                }
                Building* home = squadSettings->getBuilding(true);
                hand homeHand = home->getHandle();
                saveFile << "\tHomeBuilding:";
                if (home)
                {
                    saveFile << " <" << home->displayName << "> ";
                    saveFile << homeHand.index << ' ';
                    saveFile << homeHand.serial << ' ';
                    saveFile << homeHand.type << ' ';
                    saveFile << homeHand.container << ' ';
                    saveFile << homeHand.containerSerial;
                }
                saveFile << '\n';
                Building* work = squadSettings->getBuilding(false);
                saveFile << "\tWorkBuilding:";
                hand workHand = work->getHandle();
                if (work)
                {
                    saveFile << " <" << work->displayName << "> ";
                    saveFile << workHand.index << ' ';
                    saveFile << workHand.serial << ' ';
                    saveFile << workHand.type << ' ';
                    saveFile << workHand.container << ' ';
                    saveFile << workHand.containerSerial;
                }
                saveFile << '\n';
                auto packages = squadSettings->getSquadPackages();
                saveFile << "\tPackages:\n";
                if (packages.size() > 0)
                {
                    for (auto it = packages.begin(); it != packages.end(); ++it)
                    {
                        auto data = it->second;

                        for (int j = 0; j < data.size(); ++j)
                        {
                            saveFile << "\t\t" << it->first << ':';
                            saveFile << " <" << data[j]->name << '>';
                            saveFile << '\n';
                        }
                    }
                }
                saveFile << "\tEndPackages:" << '\n';
                saveFile << "EndSquad:" << '\n';
            }
        }
        
    }
    void (*save_orig)(SaveManager* thisptr, const std::string& s, bool autosave);
    void save_hook(SaveManager* thisptr, const std::string& s, bool autosave)
    {
        DebugLog("Autosave: " + s);
        savePath = s + '\\' + saveName;
        auto settings = SquadAutonomySettings::getSingletonPtr();

        for (int i = 0; i < settings->squadSettings.size(); ++i)
        {
            auto squadSettings = settings->squadSettings[i];
            if (!squadSettings->isEnabled()) continue;
            ResetAI(squadSettings->getSquad());
            squadSettings->unassignSquadHome();
        }

        save_orig(thisptr, s, autosave);

        saveSettings();
        for (int i = 0; i < settings->squadSettings.size(); ++i)
        {
            auto squadSettings = settings->squadSettings[i];
            if (!squadSettings->isEnabled()) continue;
            SetAI(squadSettings->getSquad(), squadSettings->getSquadPackages());
            if (!squadSettings->assignSquadHome(false))
            {
                squadSettings->assignSquadHome(true);
            }
        }
    }

    int (*saveGame_orig)(SaveManager* thisptr, const std::string& location, const std::string& name);
    int saveGame_hook(SaveManager* thisptr, const std::string& location, const std::string& name)
    {
        auto settings = SquadAutonomySettings::getSingletonPtr();
        for (int i = 0; i < settings->squadSettings.size(); ++i)
        {
            auto squadSettings = settings->squadSettings[i];
            if (!squadSettings->isEnabled()) continue;
            ResetAI(squadSettings->getSquad());
            squadSettings->unassignSquadHome();
        }
        
        int result = saveGame_orig(thisptr, location, name);

        savePath = location + name + '\\' + saveName;
        if (result == 0)
        {
            shouldSave = true;
        }
        return result;
    }

    void (*execute_orig)(SaveManager* thisptr);
    void execute_hook(SaveManager* thisptr)
    {
        execute_orig(thisptr);
        if (shouldSave)
        {
            shouldSave = false;
            saveSettings();
            auto settings = SquadAutonomySettings::getSingletonPtr();
            for (int i = 0; i < settings->squadSettings.size(); ++i)
            {
                auto squadSettings = settings->squadSettings[i];
                if (!squadSettings->isEnabled()) continue;
                SetAI(squadSettings->getSquad(), squadSettings->getSquadPackages());
                if (!squadSettings->assignSquadHome(false))
                {
                    squadSettings->assignSquadHome(true);
                }
            }
        }
        else if (shouldLoad)
        {
            SquadAutonomySettings::getSingletonPtr()->loadSettings(savePath);
            shouldLoad = false;
        }
    }

    int (*loadGame_orig)(SaveManager* thisptr, const std::string& location, const std::string& name);
    int loadGame_hook(SaveManager* thisptr, const std::string& location, const std::string& name)
    {
        int result = loadGame_orig(thisptr, location, name);
        if (result != 0) return result;
        shouldLoad = true;
        savePath = location + name + '\\' + saveName;
        return result;
    }

    void (*choosePermaJob_orig)(AITaskSytem* thisptr, std::map<float, Tasker*, std::less<float>, Ogre::STLAllocator<std::pair<float const, Tasker*>, Ogre::GeneralAllocPolicy > >& orderedGoals, TaskMatch& alreadyHasGoal, bool urgentOnes, bool _jobsEnabled);
    void choosePermaJob_hook(AITaskSytem* thisptr, std::map<float, Tasker*, std::less<float>, Ogre::STLAllocator<std::pair<float const, Tasker*>, Ogre::GeneralAllocPolicy > >& orderedGoals, TaskMatch& alreadyHasGoal, bool urgentOnes, bool _jobsEnabled)
    {
        Character* character = thisptr->character;
        SquadSettingsInfo* settings;
        if (urgentOnes && _jobsEnabled && character && character->getPlatoon())
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(character->getPlatoon()->me);
            if (settings && settings->isEnabled())
            {
                //_jobsEnabled = false;
                for (auto it = orderedGoals.begin(); it != orderedGoals.end(); ++it)
                {
                    //if (it->second && (it->second->key() == AUTO_LABOURING_MINES || it->second->key() == AUTO_LABOURING_MINES_PRETEND))
                    if (it->second && (it->second->key() == GO_HOME_AND_GO_TO_BED))
                    {
                        DebugLog("Disable job");
                        _jobsEnabled = false;
                        break;
                    }
                }
            }
        }
        choosePermaJob_orig(thisptr, orderedGoals, alreadyHasGoal, urgentOnes, _jobsEnabled);
    }

    float (*runGOAP_orig)(AITaskSytem* thisptr, Tasker* task, bool _a2, bool playerOrder);
    float runGOAP_hook(AITaskSytem* thisptr, Tasker* task, bool _a2, bool playerOrder)
    {
        Character* character = thisptr->character;
        SquadSettingsInfo* settings;
        bool setHome = false;
        if (character && character->getPlatoon())
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(character->getPlatoon()->me);
            if (settings && settings->isEnabled())
            {
                if (task && (task->key() == STAY_IN_HOME || task->key() == GO_HOME_AND_GO_TO_BED))
                {
                    setHome = settings->assignSquadHome(true);
                }
            }
        }
        float result = runGOAP_orig(thisptr, task, _a2, playerOrder);

        if (setHome)
        {
            settings->assignSquadHome(false);
        }

        return result;
    }
    

    void (*periodicUpdate_orig)(AITaskSytem* thisptr, float time);
    void periodicUpdate_hook(AITaskSytem* thisptr, float time)
    {
        Character* character = thisptr->character;
        SquadSettingsInfo* settings;
        PlayerInterface* pi;
        if (character && character->getPlatoon())
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(character->getPlatoon()->me);
            if (settings && settings->isEnabled())
            {
                pi = character->getFaction()->isPlayer;
                character->getFaction()->isPlayer = nullptr;
            }
        }

        periodicUpdate_orig(thisptr, time);

        if (settings && settings->isEnabled())
        {
            thisptr->character->getFaction()->isPlayer = pi;
            for (auto it = thisptr->orderedGoals.begin(); it != thisptr->orderedGoals.end(); ++it)
            {
                auto tasker = it->second;
                if (tasker)
                {
                    //DebugLog(Ogre::StringConverter::toString(it->first) + " - " + tasker->getDescription());
                }
            }
        }
    }

    void (*_NV_setCurrentGoal_orig)(AITaskSytem* thisptr, Tasker* t, float score, taskPriority pri);
    void _NV_setCurrentGoal_hook(AITaskSytem* thisptr, Tasker* t, float score, taskPriority pri)
    {
        _NV_setCurrentGoal_orig(thisptr, t, score, pri);
        Character* character = thisptr->character;
        SquadSettingsInfo* settings;
        if (t && character && character->getPlatoon())
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(character->getPlatoon()->me);
            if (settings && settings->isEnabled())
            {
                TaskData* taskData = t->taskData;
                if (taskData)
                {
                    if (t->key() == RELAX_IN_TOWN_PACKAGE)
                    {
                        {
                            //DebugLog("Change goal duration!");
                            //std::string description = t->getDescription();
                            taskData->setDurationBased(0.5, 8.0, false);
                        }
                    }
                    if (t->key() == MAN_A_TURRET || t->key() == MAN_THE_GATE)
                    {
                        {
                            //DebugLog("Change goal duration!");
                            //std::string description = t->getDescription();
                            taskData->setDurationBased(1.0, 1.0, false);
                        }
                    }
                }
            }

        }
    }

    float (*_NV_scoreGoToBed_orig)(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2);
    float _NV_scoreGoToBed_hook(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2)
    {
        float score = _NV_scoreGoToBed_orig(thisptr, subject, _a2);
        auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(thisptr->getPlatoon());
        if (settings && settings->isEnabled() && score > 0.0 && !thisptr->getStateBroadcast()->isSleeping)
        {
            float minuteSinceLastSlept = thisptr->getStateBroadcast()->lastSlept.getHoursPassed() * 60.0;
            if (minuteSinceLastSlept < 120.0)
            {
                return score *= 0.1;
            }
            if (minuteSinceLastSlept < 240.0)
            {
                return score *= 0.3;
            }
            if (minuteSinceLastSlept < 360.0)
            {
                return score *= 0.6;
            }
        }
        return score;
    }

    /*float (*findSomethingToDoInHome_orig)(AI* thisptr, const hand& in, hand& out, bool justAsking);
    float findSomethingToDoInHome_hook(AI* thisptr, const hand& in, hand& out, bool justAsking)
    {
        Character* character = thisptr->getCharacter();
        SquadSettingsInfo* settings;
        if (character)
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(character->getPlatoon());
            if (settings && settings->getEnabled())
            {
                DebugLog("Debug: Find something to do in home!");
            }
        }
        return findSomethingToDoInHome_orig(thisptr, in, out, justAsking);
    }*/

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
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::saveGame), &SquadAutonomy::saveGame_hook, &SquadAutonomy::saveGame_orig))
		ErrorLog("Autonomy: Could not add hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::save), &SquadAutonomy::save_hook, &SquadAutonomy::save_orig))
        ErrorLog("Autonomy: Could not add hook!");
	if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::loadGame), &SquadAutonomy::loadGame_hook, &SquadAutonomy::loadGame_orig))
		ErrorLog("Autonomy: Could not add hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::execute), &SquadAutonomy::execute_hook, &SquadAutonomy::execute_orig))
        ErrorLog("Autonomy: Could not add hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SquadManagementScreen::SquadCellView::update), &SquadAutonomy::SquadCellView_update_hook, &SquadAutonomy::SquadCellView_update_orig))
        ErrorLog("Autonomy: Could not add hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::choosePermaJob), &SquadAutonomy::choosePermaJob_hook, &SquadAutonomy::choosePermaJob_orig))
        ErrorLog("Autonomy: Could not add hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::runGOAP) , &SquadAutonomy::runGOAP_hook, &SquadAutonomy::runGOAP_orig))
        ErrorLog("Autonomy: Could not add hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::periodicUpdate), &SquadAutonomy::periodicUpdate_hook, &SquadAutonomy::periodicUpdate_orig))
        ErrorLog("Autonomy: Could not add  hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::_NV_setCurrentGoal), &SquadAutonomy::_NV_setCurrentGoal_hook, &SquadAutonomy::_NV_setCurrentGoal_orig))
        ErrorLog("Autonomy: Could not add  hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::_NV_scoreGoToBed), &SquadAutonomy::_NV_scoreGoToBed_hook, &SquadAutonomy::_NV_scoreGoToBed_orig))
        ErrorLog("Autonomy: Could not add  hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(EscMenu_openedOtherWindows, &SquadAutonomy::EscMenu_openedOtherWindows_hook, &SquadAutonomy::EscMenu_openedOtherWindows_orig))
        ErrorLog("Autonomy: Could not add EscMenu hook!");

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

