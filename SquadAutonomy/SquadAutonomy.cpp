#include <Debug.h>

#include <ogre/OgreStringConverter.h>
#include <boost/scoped_ptr.hpp>
#include <ogre/OgrePrerequisites.h>
#include <kenshi/util/OgreUnordered.h>

#include <kenshi/Kenshi.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/MainBarGUI.h>
#include <kenshi/gui/OrdersPanel.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <kenshi/gui/SquadManagementScreen.h>

#include <kenshi/util/hand.h>
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



bool (*EscMenu_openedOtherWindows)(class EscMenu*) = nullptr;
std::map<hand, float>* rentedBeds = nullptr;
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
    static int bc = 0;
    const int buffer = 50;

    void Log(std::string line)
    {
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
        logFile << line << '\n';
        lc++;
        bc++;
        if (bc >= buffer)
        {
            logFile.flush();
            bc = 0;
        }
    }

    

    bool SetAI(Platoon*, std::map<int, lektor<GameData*>>);
    bool ResetAI(Platoon*);
    bool shouldSave = false;
    bool shouldLoad = false;
    bool loadNextCall = false;
    class SquadSettingsInfo
    {
    public:
        SquadSettingsInfo(Platoon* squad) : _enabled(false), _squad(squad), _pi(nullptr), _homeBuilding(nullptr), _workBuilding(nullptr), 
            _manTurrets(false), _doSleep(false), _startSleepTime(0.0), _endSleepTime(0.0), _usePaidBeds(false), _restUntilHealed(true)
        {
            Faction* faction = squad->getFaction();
            if (faction)
            {
                _pi = faction->isPlayer;
            }
        }
        Platoon* getSquad()
        {
            return _squad;
        }
        PlayerInterface* getPlayerInterface()
        {
            return _pi;
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
                    _enabled = enable;
                    if (_enabled)
                    {
                        if (!assignSquadHome(false)) //try set home to work
                        {
                            assignSquadHome(true); //if fail set home to home
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
            bool success = false;
            if (_squadPackages.size() > 0)
            {
                success = SetAI(_squad, _squadPackages);
            }
            else
            {
                success = ResetAI(_squad);
            }
            if (success)
            {
                if (_enabled)
                {
                    if (!assignSquadHome(false)) //try set home to work
                    {
                        assignSquadHome(true); //if fail set home to home
                    }
                }
                else
                {
                    unassignSquadHome();
                }
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
        bool hasHome()
        {
            return _homeBuilding != nullptr || _workBuilding != nullptr;
        }
        bool getManTurrets()
        {
            return _manTurrets;
        }
        void setManTurrets(bool val)
        {
            _manTurrets = val;
        }
        bool getDoSleep()
        {
            return _doSleep;
        }
        bool getRestUntilHealed()
        {
            return _restUntilHealed;
        }
        bool getUsePaidBeds()
        {
            return _usePaidBeds;
        }
        float getStartSleepTime()
        {
            return _startSleepTime;
        }
        float getEndSleepTime()
        {
            return _endSleepTime;
        }
        void setStartSleepTime(float time)
        {
            _startSleepTime = time;
        }
        void setEndSleepTime(float time)
        {
            _endSleepTime = time;
        }
        void setDoSleep(bool val)
        {
            _doSleep = val;
        }
        void setRestUntilHealed(bool val)
        {
            _restUntilHealed = val;
        }
        void setUsePaidBeds(bool val)
        {
            _usePaidBeds = val;
        }
        bool isSleepTime()
        {
            if (!_doSleep) return false;
            int currentHour = static_cast<int>(ou->getTimeStamp_inGameHours().getTotalHours()) % 24;
            int startTime = static_cast<int>(_startSleepTime) % 24;
            int endTime = static_cast<int>(_endSleepTime) % 24;
            bool sleepTime = false;
            //DebugLog("Current time: " + Ogre::StringConverter::toString(currentHour));
            //DebugLog("start time: " + Ogre::StringConverter::toString(startTime) + " end time : " + Ogre::StringConverter::toString(endTime));
            if (startTime == endTime) return true;
            if (startTime > endTime)
            {
                if (currentHour >= startTime || currentHour < endTime)
                {
                    sleepTime = true;
                }
            }
            else
            {
                if (currentHour >= startTime && currentHour < endTime)
                {
                    sleepTime = true;
                }
            }
            return sleepTime;
        }

    private:
        Platoon* _squad;
        PlayerInterface* _pi;
        bool _enabled;
        std::map<int, lektor<GameData*>> _squadPackages;
        //TownBase* _homeTown;
        Building* _homeBuilding;
        //TownBase* _workTown;
        Building* _workBuilding;
        bool _manTurrets;
        bool _doSleep;
        float _startSleepTime;
        float _endSleepTime;
        bool _restUntilHealed;
        bool _usePaidBeds;
    };

    class SquadAutonomySettings
    {
    public:
        static SquadAutonomySettings* getSingletonPtr();
        static bool initialized;
        lektor<SquadSettingsInfo*> squadSettings;

        SquadAutonomySettings() : _cfgFileName("SquadAutonomy.cfg")
        {
            if (modPath != "") _cfgPath = modPath + _cfgFileName;
            else _cfgPath = GetCurrentDLLDirectory() + _cfgFileName;
            _loadConfig();
            _initGameData();
            initialized = true;
        }

        bool saveSettings(std::string savePath)
        {
            std::ofstream saveFile(savePath, std::fstream::out | std::fstream::trunc);
            if (!saveFile.is_open())
            {
                DebugLog("Save: Cannot open file");
                return false;
            }
            else
            {
                DebugLog("Saving settings to " + savePath);
                auto settings = SquadAutonomySettings::getSingletonPtr();
                if (!settings) return false;
                //DebugLog("Saving " + Ogre::StringConverter::toString(squadSettings.size()) + " squads");
                for (int i = 0; i < settings->squadSettings.size(); ++i)
                {
                    auto squadSettings = settings->squadSettings[i];
                    Platoon* platoon = squadSettings->getSquad();
                    if (!platoon) continue;
                    if (!platoon->activePlatoon) continue;
                    //DebugLog("Saving squad " + Ogre::StringConverter::toString(i));
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
                    saveFile << "\tOptions:" << '\n';
                    saveFile << "\t\tManTurrets: " << Ogre::StringConverter::toString(squadSettings->getManTurrets()) << '\n';
                    saveFile << "\t\tDoSleep: " << Ogre::StringConverter::toString(squadSettings->getDoSleep()) << '\n';
                    saveFile << "\t\t\tStartSleepTime: " << squadSettings->getStartSleepTime() << '\n';
                    saveFile << "\t\t\tEndSleepTime: " << squadSettings->getEndSleepTime() << '\n';
                    saveFile << "\t\t\tRestUntilHealed: " << Ogre::StringConverter::toString(squadSettings->getRestUntilHealed()) << '\n';
                    saveFile << "\t\t\tUsePaidBeds: " << Ogre::StringConverter::toString(squadSettings->getUsePaidBeds()) << '\n';
                    saveFile << "\tEndOptions:" << '\n';
                    saveFile << "EndSquad:" << '\n';
                }
                return true;
            }
        }

        bool loadSettings(std::string savePath)
        {
            //initialized = false;
            std::ifstream saveFile(savePath);
            if (!saveFile.is_open())
            {
                DebugLog("Load: Cannot open save file");
                return false;
            }

            DebugLog("Loading settings file at " + savePath);
            squadSettings.clear();
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
                //DebugLog(type);
                //get squad
                if (type == "Squad")
                {
                    /*========================*/
                    std::string dataLine;
                    bool enable = false;
                    Platoon* squad = nullptr;
                    Building* home = nullptr;
                    Building* work = nullptr;
                    std::map<int, lektor<GameData*>> packages;
                    bool manTurrets = false;
                    bool doSleep = false;
                    float startTime = 0.0f;
                    float endTime = 0.0f;
                    bool restUntilHealed = true;
                    bool usePaidBeds = false;
                    /*========================*/

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
                        //DebugLog(type);
                        if (type == "EndSquad") break;
                        if (type == "Enable")
                        {
                            dataLine = line.substr(colon + 1);
                            dataLine.erase(0, dataLine.find_first_not_of(" \t"));
                            //DebugLog(dataLine);
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
                                //DebugLog("Home hand created!");
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
                                //DebugLog("Work hand created!");
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
                                //DebugLog(type);
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
                        if (type == "Options")
                        {
                            while (std::getline(saveFile, line))
                            {
                                line.erase(0, line.find_first_not_of(" \t"));
                                colon = line.find(':');
                                if (colon == std::string::npos)
                                {
                                    continue;
                                }
                                type = line.substr(0, colon);
                                dataLine = line.substr(colon + 1);
                                dataLine.erase(0, dataLine.find_first_not_of(" \t"));
                                //DebugLog(type);
                                //DebugLog(dataLine);
                                if (type == "ManTurrets")
                                {
                                    if (dataLine == "true")
                                    {
                                        manTurrets = true;
                                    }
                                    continue;
                                }
                                if (type == "DoSleep")
                                {
                                    if (dataLine == "true")
                                    {
                                        doSleep = true;
                                    }
                                    continue;
                                }
                                if (type == "StartSleepTime")
                                {
                                    if (dataLine == "") continue;
                                    std::stringstream ss(dataLine);
                                    ss >> startTime;
                                    continue;
                                }
                                if (type == "EndSleepTime")
                                {
                                    if (dataLine == "") continue;
                                    std::stringstream ss(dataLine);
                                    ss >> endTime;
                                    continue;
                                }
                                if (type == "RestUntilHealed")
                                {
                                    if (dataLine == "false")
                                    {
                                        restUntilHealed = false;
                                    }
                                    continue;
                                }
                                if (type == "UsePaidBeds")
                                {
                                    if (dataLine == "true")
                                    {
                                        usePaidBeds = true;
                                    }
                                    continue;
                                }
                                if (type == "EndOptions")
                                {
                                    break;
                                }
                            }
                        }
                    }
                    //save current squad data
                    if (squad)
                    {
                        DebugLog("Load Squad " + squad->activePlatoon->getName());
                        SquadSettingsInfo* settingsInfo = new SquadSettingsInfo(squad);
                        ResetAI(squad);
                        settingsInfo->unassignSquadHome();
                        if (home)
                        {
                            settingsInfo->setBuilding(home, true);
                            //DebugLog("Load home: " + home->displayName);
                        }
                        if (work)
                        {
                            settingsInfo->setBuilding(work, false);
                            //DebugLog("Load home: " + work->displayName);
                        }
                        if (packages.size() > 0)
                        {
                            settingsInfo->setPackages(packages);
                        }
                        settingsInfo->setManTurrets(manTurrets);
                        settingsInfo->setDoSleep(doSleep);
                        settingsInfo->setStartSleepTime(startTime);
                        settingsInfo->setEndSleepTime(endTime);
                        settingsInfo->setRestUntilHealed(restUntilHealed);
                        settingsInfo->setUsePaidBeds(usePaidBeds);
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
                //DebugLog(numbers);
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
        lektor<GameData*>* getSquadTemplate()
        {
            return &_squadTemplateList;
        }
        void removeSquadSettings(Platoon* squad)
        {
            int index = -1;
            for (int i = 0; i < squadSettings.size(); ++i)
            {
                if (squadSettings[i]->getSquad() == squad)
                {
                    index = i;
                    break;
                }
            }
            if (index > -1)
            {
                lektorEx::removeAt(squadSettings, index);
            }
        }

    private:
        lektor<GameData*> _AIPackageList;
        lektor<GameData*> _squadTemplateList;
        lektor<std::string> _cfgPackageList;
        std::string _cfgFileName;
        std::string _cfgPath;
        void _loadConfig();
        void _initGameData();
    };
    SquadAutonomySettings* SquadAutonomySettings::getSingletonPtr()
    {
        static boost::scoped_ptr<SquadAutonomySettings> singleton(new SquadAutonomySettings());
        return singleton.get();
    }
    bool SquadAutonomySettings::initialized = false;
    void SquadAutonomySettings::_loadConfig()
    {
        DebugLog("Finding Config file at: " + _cfgPath);
        std::fstream cfgFile(_cfgPath, std::fstream::in | std::fstream::out | std::fstream::app);
        if (!cfgFile.is_open())
        {
            DebugLog("Load: Cannot open config file");
            return;
        }
        DebugLog("Reading config file...");
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

    void SquadAutonomySettings::_initGameData()
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
        std::sort(_AIPackageList.begin(), _AIPackageList.end(), [](GameData* a, GameData* b)
        {
            return a->name < b->name;
        });
        datas.clear();
        ou->gamedata.getDataOfType(datas, SQUAD_TEMPLATE);
        {
            for (uint32_t i = 0; i < datas.size(); ++i)
            {
                std::string packageName = datas[i]->name;
                if (packageName.size() >= identifier.size() && packageName.compare(packageName.size() - identifier.size(), identifier.size(), identifier) == 0)
                {
                    DebugLog("Load package: " + packageName);
                    lektorEx::push_back(_squadTemplateList, datas[i]);
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
        ~SquadAutonomyPanel();

        void create();
        void refresh();
        void show();
        void hide();
        bool isVisible();
        void selectSquad(Platoon*);
        class AutonomyOptions;
        AutonomyOptions* getOptionsTab()
        {
            return _options;
        }
    private:
        int _category;
        Platoon* _selectedSquad;
        DatapanelGUI* _panel;
        int _selectedAIPackageIndex;
        float _priority;

        AutonomyOptions* _options;

        void _toggleAI(DataPanelLine* line); //button for enabling/setting packages to a squad
        void _addAI(DataPanelLine* line); //set the packages in the setting
        void _clearAI(DataPanelLine* line); //clear all packages in the setting
        void _setHome(DataPanelLine* line);
        void _setWork(DataPanelLine* line);
        void _setBar(DataPanelLine* line);
        void _clearBuildings(DataPanelLine* line);
        void _setBuilding(bool home);

        void _changeAIPackageSearchText(DataPanelLine* line);
        void _updateAIPackageList(const std::string& keyword);
    };

    class SquadAutonomyPanel::AutonomyOptions
    {
    public:
        void refresh();
        void updateOptions(DataPanelLine*);
        void updateOptionsAndRefresh(DataPanelLine*);
        SquadAutonomyPanel::AutonomyOptions() : _category(1), _selectedSquad(nullptr), _panel(nullptr), _doSleep(false), _startSleepTime(0.0f), _endSleepTime(24.0f), _usePaidBeds(false) 
        {
        }
        void setPanel(DatapanelGUI* panel)
        {
            _panel = panel;
        }
        void setSelectedSquad(Platoon* squad)
        {
            _selectedSquad = squad;
        }
        int getCategory()
        {
            return _category;
        }

    private:
        int _category;
        Platoon* _selectedSquad;
        DatapanelGUI* _panel;
        bool _manTurrets;
        bool _doSleep;
        float _startSleepTime;
        float _endSleepTime;
        bool _restUntilHealed;
        bool _usePaidBeds;
        //bool _buySupplies;
        //item type?
        //bool _sellLoots;
        //item type?
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
        _options = new AutonomyOptions();
        initLineKey();
        create();
        initialized = true;

    }
    SquadAutonomyPanel::~SquadAutonomyPanel()
    {
        if (_options != nullptr)
        {
            delete _options;
        }
    }

    void SquadAutonomyPanel::create()
    {
        if (this->_panel != nullptr)
        {
            this->_panel->show(false);
            gui->destroy(this->_panel);
        }

        this->_panel = gui->createDatapanel(0.25f, 0.375f, 0.25f, 0.5f, true, "Window", true);
        this->_panel->setCaption("Squad Autonomy");
        this->_panel->setPanelName("SquadAutonomy");

        if (this->_options != nullptr)
        {
            this->_options->setPanel(this->_panel);
            this->_options->setSelectedSquad(this->_selectedSquad);
            this->_options->refresh();
        }

        this->_panel->showTabs(true);
        this->_panel->addTab(this->_category, "Main", "");
        this->_panel->addTab(_options->getCategory(), "Options", "");
        this->_panel->changeCategory(this->_category);

        refresh();

        this->_panel->show(false);
    }

    void SquadAutonomyPanel::refresh()
    {
        if (this->_panel == nullptr)
            return;
        if (!_selectedSquad || !_selectedSquad->activePlatoon) return;
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
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), 
            this, &SquadAutonomyPanel::_toggleAI);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) /button->button->getParent()->getHeight());
        if (squadSettings)
        {
            button->button->setStateSelected(squadSettings->isEnabled());
        }
        this->_panel->addSpace(this->_category, 0.25f);

        button = this->_panel->setLineTextButton("", "Set Squad Home Building", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), 
            this, &SquadAutonomyPanel::_setHome);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

        button = this->_panel->setLineTextButton("", "Set Squad Work Building", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), 
            this, &SquadAutonomyPanel::_setWork);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

        /*button = this->_panel->setLineTextButton("", "Make Building a Bar", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), 
            this, &SquadAutonomyPanel::_setBar);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());*/

        button = this->_panel->setLineTextButton("", "Clear Buildings", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), 
            this, &SquadAutonomyPanel::_clearBuildings);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
        this->_panel->addSpace(this->_category, 0.25f);

        auto editbox = this->_panel->setLineTextEditable("Search", "", this->_category, true, false, MyGUI::Align::Left, 0.95f);
        editbox->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), 
            this, &SquadAutonomyPanel::_changeAIPackageSearchText);

        auto dropbox = this->_panel->setLineDropBox(lineBoxAIPackage, this->_category, &this->_selectedAIPackageIndex, false, 1.0f);

        _updateAIPackageList("");
        editbox->getEditBox()->setSize(dropbox->listBox->getSize());
        this->_panel->addSpace(this->_category, 0.25f);

        auto slider = this->_panel->setLineSliderEditable("Priority", this->_category, true, 0.0f, 5.0f, &this->_priority);
        slider->nameText->setEnabled(false);
        slider->setPrecision(0);
        this->_panel->addSpace(this->_category, 0.25f);

        button = this->_panel->setLineTextButton("", "Add AI Package", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), 
            this, &SquadAutonomyPanel::_addAI);
        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

        button = this->_panel->setLineTextButton("", "Clear AI Packages", this->_category, 1.0f, "Kenshi_Button2");
        button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), 
            this, &SquadAutonomyPanel::_clearAI);
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
                //DebugLog("Squad Settings exist: " + Ogre::StringConverter::toString(squadPackages.size()));
                for (auto it = squadPackages.begin(); it != squadPackages.end(); ++it)
                {
                    auto packages = it->second;
                    //DebugLog(Ogre::StringConverter::toString(it->first) + " Packages: " + Ogre::StringConverter::toString(packages.size()));
                    for (int i = 0; i < packages.size(); ++i)
                    {
                        //DebugLog(Ogre::StringConverter::toString(it->first) + " " + packages[i]->name);
                        textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left);
                        textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
                        textbox->editBox->setCaption(Ogre::StringConverter::toString(it->first) + " " + packages[i]->name);
                    }
                }
            }
        }
        //textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left); //buffer text for scrolling
        //this->_panel->addSpace(this->_category, 6.0f);

    }

    void SquadAutonomyPanel::AutonomyOptions::refresh()
    {
        if (this->_panel == nullptr)
            return;
        if (this->_selectedSquad == nullptr) return;
        if (!this->_selectedSquad->isFullyLoaded() || !this->_selectedSquad->activePlatoon) return;
        if (!SquadAutonomySettings::initialized) return;
        this->_panel->setLineSpacing(24.0f);
        this->_panel->clearPage(this->_category);
        auto squadSettings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
        //get value from settings
        _manTurrets = squadSettings->getManTurrets();
        _doSleep = squadSettings->getDoSleep();
        _startSleepTime = squadSettings->getStartSleepTime();
        _endSleepTime = squadSettings->getEndSleepTime();
        _usePaidBeds = squadSettings->getUsePaidBeds();
        _restUntilHealed = squadSettings->getRestUntilHealed();

        this->_panel->setCaption("Squad Autonomy: " + _selectedSquad->activePlatoon->getName());
        //DebugLog("refresh options!");
        auto checkbox = this->_panel->setLineCheckbox("Man Turrets", &_manTurrets, this->_category);
        checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
            this, &SquadAutonomyPanel::AutonomyOptions::updateOptions);
        checkbox = this->_panel->setLineCheckbox("Allow Going to Bed", &_doSleep, this->_category);
        checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this), 
            this, &SquadAutonomyPanel::AutonomyOptions::updateOptionsAndRefresh);



        if (squadSettings->getDoSleep())
        {
            auto startSlider = this->_panel->setLineSliderEditable("Start Time", this->_category, true, 0.0f, 24.0f, &this->_startSleepTime);
            startSlider->nameText->setEnabled(false);
            startSlider->setPrecision(0);
            startSlider->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
                this, &SquadAutonomyPanel::AutonomyOptions::updateOptions);
            auto endSlider = this->_panel->setLineSliderEditable("End Time", this->_category, true, 0.0f, 24.0f, &this->_endSleepTime);
            endSlider->nameText->setEnabled(false);
            endSlider->setPrecision(0);
            endSlider->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
                this, &SquadAutonomyPanel::AutonomyOptions::updateOptions);
            checkbox = this->_panel->setLineCheckbox("Rest until healed", &_restUntilHealed, this->_category);
            checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
                this, &SquadAutonomyPanel::AutonomyOptions::updateOptions);
            checkbox = this->_panel->setLineCheckbox("Allow Using Paid Beds", &_usePaidBeds, this->_category);
            checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
                this, &SquadAutonomyPanel::AutonomyOptions::updateOptions);
        }
    }
    void SquadAutonomyPanel::AutonomyOptions::updateOptions(DataPanelLine* line)
    {
        if (!SquadAutonomySettings::initialized) return;
        if (this->_selectedSquad == nullptr) return;
        auto squadSettings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
        squadSettings->setManTurrets(_manTurrets);
        squadSettings->setDoSleep(_doSleep);
        squadSettings->setRestUntilHealed(_restUntilHealed);
        squadSettings->setUsePaidBeds(_usePaidBeds);
        squadSettings->setStartSleepTime(_startSleepTime);
        squadSettings->setEndSleepTime(_endSleepTime);
    }
    void SquadAutonomyPanel::AutonomyOptions::updateOptionsAndRefresh(DataPanelLine* line)
    {
        if (!SquadAutonomySettings::initialized) return;
        if (!this->_selectedSquad->isFullyLoaded()) return;
        updateOptions(line);
        refresh();
    }
    void SquadAutonomyPanel::show()
    {
        if (!SquadAutonomySettings::initialized) return;
        if (!this->_selectedSquad->isFullyLoaded()) return;
        this->_panel->changeCategory(this->_category);
        this->_panel->show(true);
        MyGUI::LayerManager::getInstancePtr()->upLayerItem(this->_panel->getWidget());
        refresh();
        _options->refresh();
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
        if (!SquadAutonomySettings::initialized) return;
        if (!squad->isFullyLoaded()) return;
        _selectedSquad = squad;
        _options->setSelectedSquad(squad);
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
        if (_selectedSquad && _selectedSquad->activePlatoon) 
        {
            auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
            if (line != nullptr && line->classType == DataPanelLine::DPL_BUTTON)
            {
                auto button = reinterpret_cast<DataPanelLine_Button*>(line)->button;
                if (settings && settings->enableAutonomy(!button->getStateSelected()))
                {
                    refresh();
                    if (settings->isEnabled())
                    {
                        ou->showPlayerAMessage(_selectedSquad->activePlatoon->getName() + ": Autonomy enable", true);
                    }
                    else
                    {
                        ou->showPlayerAMessage(_selectedSquad->activePlatoon->getName() + ": Autonomy disable", true);
                    }
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
                //DebugLog("Add " + data->name + " package");

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

    void SquadAutonomyPanel::_setBar(DataPanelLine* line)
    {
        //use platoon->sethomebuildingdesignation instead
        if (!SquadAutonomySettings::initialized) return;
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
                //DebugLog("Is a door of " + building->doorParentBuilding()->displayName);
                building = building->doorParentBuilding();
            }
            else if (building->isFurniture())
            {
                //DebugLog("Is a furniture of " + building->furnitureParentBuilding()->displayName);
                building = building->furnitureParentBuilding();
            }
        }
        GameData* squadTemplate = (*SquadAutonomySettings::getSingletonPtr()->getSquadTemplate())[0];
        building->residentSquadTemplate = squadTemplate;
        //DebugLog("Designation: " + Ogre::StringConverter::toString(squadTemplate->idata["building designation"]));
        building->setDesignation(static_cast<BuildingDesignation>(squadTemplate->idata["building designation"]));
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
        if (settings->isEnabled())
        {
            settings->unassignSquadHome();
        }
        ou->showPlayerAMessage("Clear buildings", true);

        refresh();

    }

    void SquadAutonomyPanel::_setBuilding(bool home)
    {
        if (!SquadAutonomySettings::initialized) return;
        if (!_selectedSquad || !_selectedSquad->activePlatoon)
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
                //DebugLog("Is a door of " + building->doorParentBuilding()->displayName);
                building = building->doorParentBuilding();
            }
            else if (building->isFurniture())
            {
                //DebugLog("Is a furniture of " + building->furnitureParentBuilding()->displayName);
                building = building->furnitureParentBuilding();
            }
        }

        //_selectedSquad->me->getOwnerships()->setHomeBuilding(building, _selectedSquad->me->getSquadType());
        settings->setBuilding(building, home);
        std::string report = "Set " + _selectedSquad->activePlatoon->getName();
        if (home) report += " home building: " + building->displayName;
        else report += " work building: " + building->displayName;
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
        //DebugLog("Set AI");
        if (!platoon || !platoon->getFaction()->isPlayer || !platoon->activePlatoon)
        {
            ou->showPlayerAMessage("No squad selected/invalid squad", true);
            return false;
        }
        for (auto iter = platoon->activePlatoon->things.begin(); iter != platoon->activePlatoon->things.end(); ++iter)
        {
            auto obj = reinterpret_cast<Character*>(*iter);
            //obj->getMovement()->halt();
            obj->clearAllAIGoals();
            //obj->getBody()->_endAction();
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
            //obj->getBody()->_endAction();
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

    /*bool (*TaskPathfinder_Node_solve_orig)(TaskPathfinder::Node* thisptr, std::deque<TaskPathfinder::Node*>& output, TaskType desiredTask, const hand& target,
        const Ogre::Vector3 loc, const hand& _debugHandle, bool playerOrder);
    bool TaskPathfinder_Node_solve_hook(TaskPathfinder::Node* thisptr, std::deque<TaskPathfinder::Node*>& output, TaskType desiredTask, const hand& target,
        const Ogre::Vector3 loc, const hand& _debugHandle, bool playerOrder)
    {
        bool result = TaskPathfinder_Node_solve_orig(thisptr, output, desiredTask, target,
            loc, _debugHandle, playerOrder);
        if (desiredTask == MAN_THE_GATE)
        {
            DebugLog("Solve node man the gate: " + Ogre::StringConverter::toString(result));
            
        }
        return result;
    }

    bool (*TaskRepertoire_hasTask_orig)(TaskRepertoire* thisptr, TaskType key);
    bool TaskRepertoire_hasTask_hook(TaskRepertoire* thisptr, TaskType key)
    {
        if (key == MAN_THE_GATE)
        {
            DebugLog("TaskRepertoire_hasTask man the gate");
        }
        bool result = TaskRepertoire_hasTask_orig(thisptr, key);
        if (key == MAN_THE_GATE)
        {
            DebugLog("TaskRepertoire_hasTask man the gate: " + Ogre::StringConverter::toString(result));
        }
        return result;
    }*/

    /*void (*setBedMode_orig)(Character* thisptr, bool on, UseableStuff* h);
    void setBedMode_hook(Character* thisptr, bool on, UseableStuff* h)
    {
        if (
        setBedMode_orig(thisptr, on, h);
    }*/
    

    hand FindOptimalBed(Character* character, bool usePaidBeds)
    {
        if (!character) return nullptr;
        AI* ai = character->ai;
        if (!ai) return nullptr;
        UseableStuff* optimalBed = nullptr;
        lektor<UseableStuff *> beds;
        float maxScore = std::numeric_limits<float>::lowest();
        float maxEfficiency = std::numeric_limits<float>::lowest();
        float minCost = std::numeric_limits<float>::max();

        TownBase* currentTown = character->getCurrentTownLocation();
        if (currentTown)
        {
            lektor<Building*>* bedBuildings = currentTown->findAllBuildingsWithFunction(BF_BED, character);
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
                            //DebugLog("Bed already rented!");
                            cost = 0;
                            break;
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
    std::unordered_set<SquadManagementScreen::SquadCellView*> autButtons;
    void (*SquadCellView_update_orig)(SquadManagementScreen::SquadCellView* thisptr, const MyGUI::IBDrawItemInfo& _info, SquadManagementScreen::SquadData* _data);
    void SquadCellView_update_hook(SquadManagementScreen::SquadCellView* thisptr, const MyGUI::IBDrawItemInfo& _info, SquadManagementScreen::SquadData* _data)
    {
        SquadCellView_update_orig(thisptr, _info, _data);

        if (autButtons.size() > 0 && autButtons.find(thisptr) != autButtons.end()) return;
        //DebugLog("Create AUT button");
        MyGUI::Widget* parent = thisptr->txtName->getParent();
        int left = thisptr->txtName->getRight() ;
        int width = thisptr->txtSquadSize->getLeft() - left;
        int top = thisptr->txtName->getTop();
        int height = thisptr->txtName->getHeight();
        MyGUI::Button* autonomyButton = parent->createWidgetReal<MyGUI::Button>("Kenshi_Button1", static_cast<float>(left) / parent->getWidth() + 0.05, 
            static_cast<float>(top) / parent->getHeight(), static_cast<float>(width) / parent->getWidth() - 0.1, static_cast<float>(height) / parent->getHeight(), MyGUI::Align::Center, "AutonomyButton");
        autonomyButton->setCaption("AUT");
        AutonomyButton* autButton = new AutonomyButton(autonomyButton, thisptr);
        autButtons.insert(thisptr);
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
    MyGUI::Button* autonomyButton = nullptr;
    MainBarGUI* (*MainbarGUICONSTRUCTOR_orig)(MainBarGUI* thisptr);
    MainBarGUI* MainbarGUICONSTRUCTOR_hook(MainBarGUI* thisptr)
    {
        MainBarGUI* orig = MainbarGUICONSTRUCTOR_orig(thisptr);
        autonomyButton = orig->getWidget()->createWidgetReal<MyGUI::Button>("Kenshi_Button1", 0.690, 0.788, 0.015, 0.0358, MyGUI::Align::Center, "AutonomyButton");
        autonomyButton->setCaption("AUT");
        autonomyButton->setFontHeight(12);
        autonomyButton->eventMouseButtonClick += MyGUI::newDelegate(OpenSquadAutonomyPanelMainBar);
        autonomyButton->setDepth(0);
        return orig;
    }

    void (*_NV_update_orig)(MainBarGUI* thisptr);
    void _NV_update_hook(MainBarGUI* thisptr)
    {
        _NV_update_orig(thisptr);
        if (autonomyButton)
        {
            autonomyButton->setRealPosition(0.69, 0.788);
            autonomyButton->setFontHeight(12);
            autonomyButton->setDepth(0);
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
    


    int (*saveGame_orig)(SaveManager* thisptr, const std::string& location, const std::string& name);
    int saveGame_hook(SaveManager* thisptr, const std::string& location, const std::string& name)
    {
        settingsSavePath = location + name + '\\' + saveName;
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

    /*void (*load_orig)(SaveManager* thisptr, const std::string& name);
    void load_hook(SaveManager* thisptr, const std::string& name)
    {
        DebugLog("Load hook");
        load_orig(thisptr, name);
        shouldLoad = true;
        SquadAutonomySettings::getSingletonPtr()->initialized = false;
        settingsSavePath = name + '\\' + saveName;
    }*/

    int (*loadGame_orig)(SaveManager* thisptr, const std::string& location, const std::string& name);
    int loadGame_hook(SaveManager* thisptr, const std::string& location, const std::string& name)
    {
        int result = loadGame_orig(thisptr, location, name);
        if (result != 0) return result;
        shouldLoad = true;
        SquadAutonomySettings::getSingletonPtr()->initialized = false;
        settingsSavePath = location + name + '\\' + saveName;
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
        float score = std::numeric_limits<float>::max();
        auto factions = *ou->factionMgr->getAllFactions();
        Character* leader = squad->getSquadLeader();
        if (!leader) return;
        auto pos = leader->getPosition();
        Town* nearest = nullptr;
        Faction* myFaction = squad->getFaction();
        if (!myFaction) return;
        int i = 0;
        for (i = 0; i < factions.size(); ++i)
        {
            Faction* townFaction = factions[i];
            if (townFaction->isNotARealFaction() || factions[i]->relations->isEnemy(myFaction)) continue;

            Town* town = shou->townList->getNearestTown(pos, factions[i], nullptr, squad->getFaction(), TOWN_TOWN);
            Town* village = shou->townList->getNearestTown(pos, factions[i], nullptr, squad->getFaction(), TOWN_VILLAGE);
            Town* slaveCamp = shou->townList->getNearestTown(pos, factions[i], nullptr, squad->getFaction(), TOWN_SLAVE_CAMP);
            Town* military = shou->townList->getNearestTown(pos, factions[i], nullptr, squad->getFaction(), TOWN_MILITARY);
            if (town && town->squaredDistanceTo(pos) < score)
            {
                score = town->squaredDistanceTo(pos);
                nearest = town;
            }
            if (village && village->squaredDistanceTo(pos) < score)
            {
                score = village->squaredDistanceTo(pos);
                nearest = village;
            }
            if (slaveCamp && slaveCamp->squaredDistanceTo(pos) < score)
            {
                score = slaveCamp->squaredDistanceTo(pos);
                nearest = slaveCamp;
            }
            if (military && military->squaredDistanceTo(pos) < score)
            {
                score = military->squaredDistanceTo(pos);
                nearest = military;
            }
        }
        Town* playerTown = shou->townList->getNearestPlayerTown(pos, true, std::numeric_limits<float>::max());
        if (playerTown && playerTown->squaredDistanceTo(pos) < score)
        {
            score = playerTown->squaredDistanceTo(pos);
            nearest = playerTown;
        }
        if (nearest)
        {
            squad->getOwnerships()->setHomeTown(nearest, squad->getSquadType());
            /*DebugLog("Set " + squad->displayName + " hometown as " + nearest->getKnownName());
            DebugLog("Out of " + Ogre::StringConverter::toString(i) + " towns");*/
        }
    }

    float (*runTargetFind_orig)(TaskData* thisptr, AI* ai, const hand& _target, hand& out, bool justAsking);
    float runTargetFind_hook(TaskData* thisptr, AI* ai, const hand& _target, hand& out, bool justAsking)
    {
        float score = runTargetFind_orig(thisptr, ai, _target, out, justAsking);
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;

        if (ai && ai->getPlatoon())
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(ai->getPlatoon());
            character = ai->getCharacter();
        }
            
        if (character && settings && settings->isEnabled() && thisptr)
        {
            Log("RuntargetFind");
            TaskType key = thisptr->key;
            if (key == GO_HOME_AND_GO_TO_BED)
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
        return score;

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
        if (urgentOnes && _jobsEnabled && settings && settings->isEnabled())
        {
            Log("PermaJob");
            if (settings->isSleepTime())
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

    /*void (*chooseGoalFrom_orig)(AITaskSytem* thisptr, std::map<float, Tasker*, std::less<float>, Ogre::STLAllocator<std::pair<float const, Tasker*>, Ogre::GeneralAllocPolicy > >& orderedGoals, TaskMatch& alreadyHasGoal, lektor<Tasker*>& goalListIn, bool theseAreBlackboard);
    void chooseGoalFrom_hook(AITaskSytem* thisptr, std::map<float, Tasker*, std::less<float>, Ogre::STLAllocator<std::pair<float const, Tasker*>, Ogre::GeneralAllocPolicy > >& orderedGoals, TaskMatch& alreadyHasGoal, lektor<Tasker*>& goalListIn, bool theseAreBlackboard)
    {
        Character* character = nullptr;
        if (thisptr) character = thisptr->character;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        //Faction* faction = nullptr;
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon)
        {
            //faction = platoon->getFaction();
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);
        }
        if (settings && settings->isEnabled())
        {
            Log("ChooseGoalFrom");

            if (orderedGoals.size() > 0 && character == gui->selectedObject.getCharacter())
            {
                for (auto it = orderedGoals.begin(); it != orderedGoals.end(); ++it)
                {
                    if (it->second)
                    {
                        Log("Choose Goal: " + it->second->getDescription() + " score: " + Ogre::StringConverter::toString(it->first));

                    }
                }
            }
        }
        
       chooseGoalFrom_orig(thisptr, orderedGoals, alreadyHasGoal, goalListIn, theseAreBlackboard);
       if (settings && settings->isEnabled())
       {
           Log("End chooseGoalFrom");
           if (orderedGoals.size() > 0 && character == gui->selectedObject.getCharacter())
           {
               for (auto it = orderedGoals.begin(); it != orderedGoals.end(); ++it)
               {
                   if (it->second)
                   {
                       Log("After Choose Goal: " + it->second->getDescription() + " score: " + Ogre::StringConverter::toString(it->first));
                   }
               }
           }
        }
    }*/

    float (*runGOAP_orig)(AITaskSytem* thisptr, Tasker* task, bool _a2, bool playerOrder);
    float runGOAP_hook(AITaskSytem* thisptr, Tasker* task, bool _a2, bool playerOrder)
    {
        //bool setHome = false;
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        if (thisptr)  character = thisptr->character;
        if (character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);
        if (settings && settings->isEnabled())
        {
            Log("runGOAP");
            /*if (task && task->key() == MAN_THE_GATE)
            {
                DebugLog("Running Man The Gate Goap");
            }*/
            if (!playerOrder && task && character)
            {
                if (!settings->hasHome())
                {
                    SetNearestFriendlyTownAsHome(platoon);
                }
                else
                {
                    if (settings->isSleepTime())
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
        }
        if (settings && settings->isEnabled() && task)
        {
            if (settings->isSleepTime())
            {
                TaskType type = task->key();
                if (task->priority == TP_URGENT && (type == ATTACK_ENEMIES || type == TERRITORIAL_AGGRESSION_BUT_DONT_LEAVE_HOME
                    || type == PROTECT_ALLIES || type == PROTECT_ALLIES_STAY_IN_TOWN || type == INVESTIGATE_ALARMS))
                {
                    Log("Task: " + task->getDescription() + " become non-urgent");
                    task->priority = TP_NON_URGENT;
                }
            }
        }

        float result = runGOAP_orig(thisptr, task, _a2, playerOrder);
        if (settings && settings->isEnabled())
        {
            if (task) Log("GOAP task: " + task->getDescription() + " score: " + Ogre::StringConverter::toString(result));
            Log("end runGOAP");
        }
        /*if (settings && settings->isEnabled() && task && character == gui->selectedObject.getCharacter())
        {
            Log("GOAP task: " + task->getDescription() + " score: "  + Ogre::StringConverter::toString(result));
            Log("end runGOAP");
        }*/

        return result;// runGOAP_orig(thisptr, task, _a2, playerOrder);
    }



    /*bool (*runGoals_orig)(AITaskSytem* thisptr, std::map<float, Tasker*, std::less<float>, Ogre::STLAllocator<std::pair<float const, Tasker*>, Ogre::GeneralAllocPolicy > >& orderedGoals, bool playerOrder);
    bool runGoals_hook(AITaskSytem* thisptr, std::map<float, Tasker*, std::less<float>, Ogre::STLAllocator<std::pair<float const, Tasker*>, Ogre::GeneralAllocPolicy > >& orderedGoals, bool playerOrder)
    {
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        if (thisptr) character = thisptr->character;
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);
        if (settings && settings->isEnabled())
        {
            if (orderedGoals.size() > 0 && character == gui->selectedObject.getCharacter())
            {
                for (auto it = orderedGoals.begin(); it != orderedGoals.end(); ++it)
                {
                    if (it->second)
                    {
                        DebugLog("Run Goal: " + it->second->getDescription() + " score: " + Ogre::StringConverter::toString(it->first));
                    }
                }
            }
        }
        bool result = runGoals_orig(thisptr, orderedGoals, playerOrder);
        if (settings && settings->isEnabled())
        {
            if (orderedGoals.size() > 0 && character == gui->selectedObject.getCharacter())
            {
                DebugLog("Run goal result: " + Ogre::StringConverter::toString(result));
                for (auto it = orderedGoals.begin(); it != orderedGoals.end(); ++it)
                {
                    if (it->second)
                    {
                        DebugLog("After Run Goal: " + it->second->getDescription() + " score: " + Ogre::StringConverter::toString(it->first));
                    }
                }
            }
        }
        return result;
    }*/

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
            Log("Clear current goal");
            auto currentGoal = thisptr->tryToGetCurrentGoal();
            if (currentGoal && (currentGoal->key() == WANDERING_TRADER ||
                currentGoal->key() == TRAVEL_TO_TARGET_TOWN || currentGoal->key() == TRAVEL_TO_TARGET_TOWN_FAST || currentGoal->key() == TRAVEL_TO_TARGET_PACKAGE))
            {
                return;
            }
            Log("End Clear current goal");
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
                SetNearestFriendlyTownAsHome(platoon);
            }
            else
            {
                if (settings->isSleepTime())
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
        _NV_setCurrentGoal_orig(thisptr, t, score, pri);
        Character* character = nullptr;
        SquadSettingsInfo* settings = nullptr;
        Platoon* platoon = nullptr;
        if (thisptr) character = thisptr->character;
        if (character && character->getPlatoon()) platoon = character->getPlatoon()->me;
        if (platoon)
        {
            settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(platoon);
        }
        if (t && settings && settings->isEnabled())
        {
            Log("SetCurrentGoal");
            TaskData* taskData = t->taskData;
            TaskType type = t->key();
            if (type == MAN_A_TURRET || type == MAN_A_TURRET_ON_BUILDING || type == MAN_THE_GATE || type == AUTO_LABOURING_MINES ||
                type == STAND_AT_GUARD_NODE_HOMEBUILDING_INDOORS_ONLY || type == STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT ||
                type == STAND_AT_GUARD_NODE_HOMETOWN_OUTSIDE || type == PATROL_TOWN || type == ATTACK_ENEMIES || type == TERRITORIAL_AGGRESSION_BUT_DONT_LEAVE_HOME)
            {
                        
                if (settings->getDoSleep())
                {
                    t->startTime = settings->getEndSleepTime();
                    t->endTime = settings->getStartSleepTime();
                }
                //DebugLog("Man something end time: " + Ogre::StringConverter::toString(t->endTime));
                //std::string description = t->getDescription();
                //taskData->setDurationBased(0.1, 0.5, false);
                        
            }
            if (type == GO_HOME_AND_GO_TO_BED)
            {
                if (settings->getDoSleep())
                {
                    if (settings->isSleepTime())
                    {
                        t->startTime = settings->getStartSleepTime();
                        t->endTime = settings->getEndSleepTime();
                    }
                    else if (settings->getRestUntilHealed())
                    {
                        t->startTime = 0;
                        t->endTime = 24;
                    }
                    if (taskData)
                    {
                        taskData->setDurationBased(6.0, 2.0, false);
                    }
                }
            }
            if (type == RELAX_IN_TOWN_PACKAGE)
            {
                if (taskData)
                {
                    taskData->setDurationBased(1.0, 1.0, false);
                }
            }
            Log("End SetCurrentGoal");
        }
    }

    float (*score_orig)(Tasker* thisptr, AI* ai);
    float score_hook(Tasker* thisptr, AI* ai)
    {
        Platoon* squad = nullptr;
        SquadSettingsInfo* settings = nullptr;
        StateBroadcastData* stateBroadcast = nullptr;
        Character* character = nullptr;
        if (ai)
        {
            squad = ai->getPlatoon();
            character = ai->getCharacter();
            stateBroadcast = ai->getStateBroadcast();
        }
        if (squad) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(squad);


        float score = score_orig(thisptr, ai);

        if (settings && settings->isEnabled() && thisptr)
        {
            Log("score");
            TaskType type = thisptr->key();
            if (type == STAY_IN_HOME || type == SIT_AROUND)
            {
                if (settings->getDoSleep() && !settings->isSleepTime())
                {
                    return score *= 0.01;
                }
            }
            if (type == MAN_A_TURRET || type == MAN_A_TURRET_ON_BUILDING || type == MAN_THE_GATE || type == AUTO_LABOURING_MINES ||
                type == STAND_AT_GUARD_NODE_HOMEBUILDING_INDOORS_ONLY || type == STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT ||
                type == STAND_AT_GUARD_NODE_HOMETOWN_OUTSIDE || type == PATROL_TOWN || type == ATTACK_ENEMIES || type == TERRITORIAL_AGGRESSION_BUT_DONT_LEAVE_HOME)
            {
                if (settings->isSleepTime())
                {
                    return 0.0;
                }
            }
            if (type == MAN_A_TURRET || type == MAN_A_TURRET_ON_BUILDING) 
            {
                if (!settings->getManTurrets())
                {
                    return 0.0;
                }
            }
            Log("End score");
        }
        return score;
    }

    float (*_NV_scoreGoToBed_orig)(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2);
    float _NV_scoreGoToBed_hook(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2)
    {
        float score = _NV_scoreGoToBed_orig(thisptr, subject, _a2);
        Platoon* squad = nullptr;
        SquadSettingsInfo* settings = nullptr;
        StateBroadcastData* stateBroadcast = nullptr;
        Character* character = nullptr;
        if (thisptr)
        {
            squad = thisptr->getPlatoon();
            character = thisptr->getCharacter();
            stateBroadcast = thisptr->getStateBroadcast();
        }
        if (squad) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(squad);
        if (settings && settings->isEnabled() && thisptr)
        {
            Log("score-gotobed");
            MedicalSystem* medical = nullptr;
            RaceData* race = nullptr;
            if (character)
            {
                race = character->getRace();
                medical = character->getMedical();
            }
            if (settings->getDoSleep())
            {
                //if need to rest
                if (stateBroadcast && stateBroadcast->isSleeping)
                {
                    if (race && !race->robot && medical && !medical->isFullyRested())
                    {
                        //DebugLog(character->displayName + " medical restedstate: " + Ogre::StringConverter::toString(medical->restedState));
                        return score = 10.0;
                    }
                }
                else
                {
                    if (settings->isSleepTime() && stateBroadcast && !stateBroadcast->isSleeping)
                    {
                        if (race && !race->robot && medical && !medical->isFullyRested())
                        {
                            //DebugLog(character->displayName + " medical restedstate: " + Ogre::StringConverter::toString(medical->restedState));
                            return score *= 10.0;
                        }
                        float minuteSinceLastSlept = thisptr->getStateBroadcast()->lastSlept.getHoursPassed() * 60.0;
                        if (minuteSinceLastSlept < 120.0)
                        {
                            return score *= 0.001;
                        }
                        if (minuteSinceLastSlept < 240.0)
                        {
                            return score *= 0.005;
                        }
                        if (minuteSinceLastSlept < 480.0)
                        {
                            return score *= 0.01;
                        }
                        if (minuteSinceLastSlept < 960.0)
                        {
                            return score *= 0.5;
                        }
                    }
                }
            }
            else
            {
                Faction* faction = squad->getFaction();
                //for auto sleep task but idk if it even works
                if (faction && faction->isPlayer)
                {
                    //DebugLog("AutoSleep Task: " + Ogre::StringConverter::toString(score));
                    return score;
                }
                return 0.0;
            }
            Log("end gotobed");
        }
        return score;
    }

    float (*_NV_scoreGetOutOfBed_orig)(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2);
    float _NV_scoreGetOutOfBed_hook(AI* thisptr, const hand& subject, const Ogre::Vector3& _a2)
    {
        float score = _NV_scoreGetOutOfBed_orig(thisptr, subject, _a2);
        Platoon* squad = nullptr;
        SquadSettingsInfo* settings = nullptr;
        StateBroadcastData* stateBroadcast = nullptr;
        Character* character = nullptr;
        if (thisptr)
        {
            squad = thisptr->getPlatoon();
            character = thisptr->getCharacter();
        }

        if (squad) settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(squad);
        if (settings && settings->isEnabled() && thisptr)
        {
            Log("score-getoutofbed");
            MedicalSystem* medical = nullptr;
            RaceData* race = nullptr;
            if (character)
            {
                race = character->getRace();
                medical = character->getMedical();
            }
            if (settings->getDoSleep())
            {
                //if still injured and has get rest until healed
                if (settings->getRestUntilHealed())
                {
                    if (medical && !medical->isFullyRested() && race && !race->robot)
                    {
                        //DebugLog(character->displayName + " medical restedstate: " + Ogre::StringConverter::toString(medical->restedState));
                        return 0.0;
                    }
                }
                if (settings->isSleepTime())
                {
                    return 0.0;
                }
                else
                {
                    return 1.0;
                }
            }
            Log("End score-getoutofbed");
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

    void Init()
    {
        modPath = GetCurrentDLLDirectory();
        logPath = modPath + logFileName;
        logBakPath = modPath + logBakFileName;
        SquadAutonomySettings::getSingletonPtr();
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
            *(uintptr_t*)&EscMenu_openedOtherWindows = baseAddr + 0x916250;
            *(uintptr_t*)&rentedBeds = baseAddr+0x212db18;
            //*(uintptr_t*)&TaskPathfinder_Node_solve = baseAddr + 0x50ED70;
            //*(uintptr_t*)&TaskRepertoire_hasTask = baseAddr + 0x32dbb0;
            //*(uintptr_t*)&load = baseAddr + 0x47AC10;
        }
        else if (platform == 0)
        {
            *(uintptr_t*)&EscMenu_openedOtherWindows = baseAddr + 0x915970;
            //*(uintptr_t*)&load = baseAddr + 0x47AD00;
           //*(uintptr_t*)&TaskPathfinder_Node_solve = baseAddr + 0x50F080;
            //*(uintptr_t*)&TaskRepertoire_hasTask = baseAddr + 0x32D740;

        }
    }
    
    SquadAutonomy::Init();
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&ForgottenGUI::changeFontSize), &SquadAutonomy::ForgottenGUI_changeFontSize_hook, &SquadAutonomy::ForgottenGUI_changeFontSize_orig))
        ErrorLog("Could not add changeFontSize hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&MainBarGUI::_CONSTRUCTOR), &SquadAutonomy::MainbarGUICONSTRUCTOR_hook, &SquadAutonomy::MainbarGUICONSTRUCTOR_orig))
        ErrorLog("Could not add MainBarGUI constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&MainBarGUI::tabPlatoonChange), &SquadAutonomy::tabPlatoonChange_hook, &SquadAutonomy::tabPlatoonChange_orig))
        ErrorLog("Could not add tabPlatoonChange constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&MainBarGUI::_NV_update), &SquadAutonomy::_NV_update_hook, &SquadAutonomy::_NV_update_orig))
        ErrorLog("Could not add tabPlatoonChange constructor hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::saveGame), &SquadAutonomy::saveGame_hook, &SquadAutonomy::saveGame_orig))
		ErrorLog("Could not add saveGame hook!");
	if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::loadGame), &SquadAutonomy::loadGame_hook, &SquadAutonomy::loadGame_orig))
		ErrorLog("Could not add loadGame hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SaveManager::execute), &SquadAutonomy::execute_hook, &SquadAutonomy::execute_orig))
        ErrorLog("Could not add execute hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SquadManagementScreen::SquadCellView::update), &SquadAutonomy::SquadCellView_update_hook, &SquadAutonomy::SquadCellView_update_orig))
        ErrorLog("Could not add SquadCellView::update hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&SquadManagementScreen::removeSquad), &SquadAutonomy::removeSquad_hook, &SquadAutonomy::removeSquad_orig))
        ErrorLog("Could not add removeSquad hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(UseABed, &SquadAutonomy::UseABed_hook, &SquadAutonomy::UseABed_orig))
        ErrorLog("Could not add scoreGetOutOfBed hook!");*/// Hook the method (call interception) — the primary mod mechanism
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::_NV_scoreGetOutOfBed), &SquadAutonomy::_NV_scoreGetOutOfBed_hook, &SquadAutonomy::_NV_scoreGetOutOfBed_orig))
        ErrorLog("Could not add scoreGetOutOfBed hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AI::_NV_scoreGoToBed), &SquadAutonomy::_NV_scoreGoToBed_hook, &SquadAutonomy::_NV_scoreGoToBed_orig))
        ErrorLog("Could not add scoreGoToBed hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&OrdersReceiver::clearCurrentGoal), &SquadAutonomy::clearCurrentGoal_hook, &SquadAutonomy::clearCurrentGoal_orig))
        ErrorLog("Could not add clearCurrentGoal hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::runGoals), &SquadAutonomy::runGoals_hook, &SquadAutonomy::runGoals_orig))
        ErrorLog("Could not add runGoals hook!");*/
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::choosePermaJob), &SquadAutonomy::choosePermaJob_hook, &SquadAutonomy::choosePermaJob_orig))
        ErrorLog("Could not add choosePermaJob hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::chooseGoalFrom), &SquadAutonomy::chooseGoalFrom_hook, &SquadAutonomy::chooseGoalFrom_orig))
        ErrorLog("Could not add chooseGoalFrom hook!");*/
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::runGOAP) , &SquadAutonomy::runGOAP_hook, &SquadAutonomy::runGOAP_orig))
        ErrorLog("Could not add runGOAP hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::periodicUpdate), &SquadAutonomy::periodicUpdate_hook, &SquadAutonomy::periodicUpdate_orig))
        ErrorLog("Could not add periodicUpdate hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&AITaskSytem::_NV_setCurrentGoal), &SquadAutonomy::_NV_setCurrentGoal_hook, &SquadAutonomy::_NV_setCurrentGoal_orig))
        ErrorLog("Could not add _NV_setCurrentGoal hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&Tasker::score), &SquadAutonomy::score_hook, &SquadAutonomy::score_orig))
        ErrorLog("Could not add Tasker::score hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(KenshiLib::GetRealAddress(&TaskData::runTargetFind), &SquadAutonomy::runTargetFind_hook, &SquadAutonomy::runTargetFind_orig))
        ErrorLog("Could not add runTargetFind hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(EscMenu_openedOtherWindows, &SquadAutonomy::EscMenu_openedOtherWindows_hook, &SquadAutonomy::EscMenu_openedOtherWindows_orig))
        ErrorLog("Could not add EscMenu hook!");
    /*if (KenshiLib::SUCCESS != KenshiLib::QueueHook(TaskPathfinder_Node_solve, &SquadAutonomy::TaskPathfinder_Node_solve_hook, &SquadAutonomy::TaskPathfinder_Node_solve_orig))
        ErrorLog("Could not add TaskPathfinder_Node_solve hook!");
    if (KenshiLib::SUCCESS != KenshiLib::QueueHook(TaskRepertoire_hasTask, &SquadAutonomy::TaskRepertoire_hasTask_hook, &SquadAutonomy::TaskRepertoire_hasTask_orig))
        ErrorLog("Could not add TaskRepertoire_hasTask hook!");*/
    KenshiLib::ApplyQueuedHooks();
    DebugLog("Mod started");
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

