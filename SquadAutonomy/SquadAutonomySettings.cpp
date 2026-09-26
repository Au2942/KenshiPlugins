#include "SquadAutonomy.h"
#include "SquadAutonomySettings.h"

#include <Debug.h>

#include <ogre/OgreStringConverter.h>
#include <boost/scoped_ptr.hpp>

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Character.h>
#include <kenshi/StateBroadcastData.h>
#include <kenshi/Platoon.h>
#include <kenshi/Faction.h>
#include <kenshi/AI/AITaskSystem.h>
#include <kenshi/Tasker.h>
#include <kenshi/Building/Building.h>
#include <kenshi/util/lektor.h>

#include "lektorExtension.h"

using namespace SquadAutonomy;

bool SquadAutonomySettings::initialized = false;

SquadAutonomySettings::SquadAutonomySettings() : _cfgFileName(L"SquadAutonomy.cfg")
{
    if (modPath != L"") _cfgPath = modPath + _cfgFileName;
    else _cfgPath = GetCurrentDLLDirectory() + _cfgFileName;
    _loadConfig();
    _initGameData();
    initialized = true;
}


SquadAutonomySettings* SquadAutonomySettings::getSingletonPtr()
{
    static boost::scoped_ptr<SquadAutonomySettings> singleton(new SquadAutonomySettings());
    return singleton.get();
}
void SquadAutonomySettings::_loadConfig()
{
    DebugLog("Finding Config file at: " + converter.to_bytes(_cfgPath));
    std::wfstream cfgFile(_cfgPath, std::wfstream::in | std::wfstream::out | std::wfstream::app);
    if (!cfgFile.is_open())
    {
        DebugLog("Load: Cannot open config file");
        return;
    }
    DebugLog("Reading config file...");
    std::wstring wline;
    std::string token;
    while (std::getline(cfgFile, wline))
    {
        std::string line = converter.to_bytes(wline);
        //DebugLog(line);
        if (line == "<Options>")
        {
            while (std::getline(cfgFile, wline))
            {
                std::string line = converter.to_bytes(wline);
                if (line == "</Options>") break;
                int colon = -1;
                std::string type = "";
                std::string dataLine = "";
                line.erase(0, line.find_first_not_of(" \t"));
                colon = line.find(':');
                if (colon == std::string::npos)
                {
                    continue;
                }

                type = line.substr(0, colon);
                dataLine = line.substr(colon + 1);
                //DebugLog(type);
                if (type == "ShowOnMain")
                {
                    dataLine.erase(0, dataLine.find_first_not_of(" \t"));
                    //DebugLog(dataLine);
                    if (dataLine == "false")
                    {
                        showOnMain = false;
                    }
                    continue;
                }
                if (type == "LockPosition")
                {
                    dataLine.erase(0, dataLine.find_first_not_of(" \t"));
                    //DebugLog(dataLine);
                    if (dataLine == "false")
                    {
                        lockPosition = false;
                    }
                    continue;
                }
                if (type == "BtnWidth")
                {
                    if (dataLine == "") continue;
                    std::stringstream ss(dataLine);
                    ss >> btnWidth;
                    continue;
                }
                if (type == "BtnHeight")
                {
                    if (dataLine == "") continue;
                    std::stringstream ss(dataLine);
                    ss >> btnHeight;
                    continue;
                }
                if (type == "BtnLeft")
                {
                    if (dataLine == "") continue;
                    std::stringstream ss(dataLine);
                    ss >> btnLeft;
                    continue;
                }
                if (type == "BtnTop")
                {
                    if (dataLine == "") continue;
                    std::stringstream ss(dataLine);
                    ss >> btnTop;
                    continue;
                }
                if (type == "BtnFontSize")
                {
                    if (dataLine == "") continue;
                    std::stringstream ss(dataLine);
                    ss >> btnFontSize;
                    continue;
                }
                if (type == "ShowInSquad")
                {
                    dataLine.erase(0, dataLine.find_first_not_of(" \t"));
                    //DebugLog(dataLine);
                    if (dataLine == "false")
                    {
                        showInSquad = false;
                    }
                    continue;
                }
                if (type == "EnableLogging")
                {
                    dataLine.erase(0, dataLine.find_first_not_of(" \t"));
                    //DebugLog(dataLine);
                    if (dataLine == "true")
                    {
                        enableLogging = true;
                    }
                    continue;
                }
            }
        }
        else if (line == "<Packages>")
        {
            while (std::getline(cfgFile, wline))
            {
                std::string line = converter.to_bytes(wline);
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

        for (uint32_t j = 0; j < _cfgPackageList.size(); ++j)
        {
            if (packageName == _cfgPackageList[j])
            {
                DebugLog("Load package: " + packageName);
                lektorEx::push_back_unique(_AIPackageList, datas[i]);
                break;
            }
        }

        if (packageName.size() >= identifier.size() && packageName.compare(packageName.size() - identifier.size(), identifier.size(), identifier) == 0)
        {
            DebugLog("Load package: " + packageName);
            lektorEx::push_back_unique(_AIPackageList, datas[i]);
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

bool SquadAutonomySettings::saveSettings(std::wstring savePath)
{
    std::wofstream saveFile(savePath, std::wfstream::out | std::wfstream::trunc);
    if (!saveFile.is_open())
    {
        DebugLog("Save: Cannot open file");
        return false;
    }
    else
    {
        DebugLog("Saving settings to " + converter.to_bytes(savePath));
        auto settings = SquadAutonomySettings::getSingletonPtr();
        if (!settings) return false;
        //DebugLog("Saving " + Ogre::StringConverter::toString(squadSettings.size()) + " squads");
        for (int i = 0; i < settings->squadSettings.size(); ++i)
        {
            auto settingsInfo = settings->squadSettings[i];
            Platoon* squad = settingsInfo->getSquad();
            if (!squad) continue;
            if (!squad->activePlatoon) continue;
            //DebugLog("Saving squad " + Ogre::StringConverter::toString(i));
            hand platoonHand = squad->getHandle();
            if (platoonHand)
            {
                saveFile << converter.from_bytes("Squad: <" + squad->activePlatoon->getName() + "> ");
                saveFile << platoonHand.index << L' ';
                saveFile << platoonHand.serial << L' ';
                saveFile << platoonHand.type << L' ';
                saveFile << platoonHand.container << L' ';
                saveFile << platoonHand.containerSerial << L'\n';
                saveFile << converter.from_bytes("\tEnable: " + Ogre::StringConverter::toString(settingsInfo->isEnabled()) + '\n');
            }
            Building* home = settingsInfo->getBuilding(true);
            saveFile << "\tHomeBuilding:";
            if (home)
            {
                hand homeHand = home->getHandle();
                saveFile << converter.from_bytes(" <" + home->displayName + "> ");
                saveFile << homeHand.index << L' ';
                saveFile << homeHand.serial << L' ';
                saveFile << homeHand.type << L' ';
                saveFile << homeHand.container << L' ';
                saveFile << homeHand.containerSerial;
            }
            saveFile << '\n';
            Building* work = settingsInfo->getBuilding(false);
            saveFile << "\tWorkBuilding:";
            if (work)
            {
                hand workHand = work->getHandle();
                saveFile << converter.from_bytes(" <" + work->displayName + "> ");
                saveFile << workHand.index << L' ';
                saveFile << workHand.serial << L' ';
                saveFile << workHand.type << L' ';
                saveFile << workHand.container << L' ';
                saveFile << workHand.containerSerial;
            }
            saveFile << L'\n';
            auto packages = settingsInfo->getSquadPackages();
            saveFile << converter.from_bytes("\tPackages:\n");
            if (packages.size() > 0)
            {
                for (auto it = packages.begin(); it != packages.end(); ++it)
                {
                    auto data = it->second;

                    for (int j = 0; j < data.size(); ++j)
                    {
                        saveFile << converter.from_bytes("\t\t") << it->first << L':';
                        saveFile << converter.from_bytes(" <" + data[j]->name + '>');
                        saveFile << L'\n';
                    }
                }
            }
            saveFile << converter.from_bytes("\tEndPackages:") << L'\n';
            saveFile << converter.from_bytes("\tOptions:") << L'\n';
            saveFile << converter.from_bytes("\t\tStartWorkTime: ") << settingsInfo->getStartWorkTime() << L'\n';
            saveFile << converter.from_bytes("\t\tEndWorkTime: ") << settingsInfo->getEndWorkTime() << L'\n';
            saveFile << converter.from_bytes("\t\tManTurrets: " + Ogre::StringConverter::toString(settingsInfo->getManTurrets())) << L'\n';
            saveFile << converter.from_bytes("\t\tCloseGate: " + Ogre::StringConverter::toString(settingsInfo->getCloseGate())) << L'\n';
            saveFile << converter.from_bytes("\t\tDoSleep: " + Ogre::StringConverter::toString(settingsInfo->getDoSleep())) << L'\n';
            saveFile << converter.from_bytes("\t\t\tRestUntilHealed: " + Ogre::StringConverter::toString(settingsInfo->getRestUntilHealed())) << L'\n';
            saveFile << converter.from_bytes("\t\t\tUsePaidBeds: " + Ogre::StringConverter::toString(settingsInfo->getUsePaidBeds())) << L'\n';
            saveFile << converter.from_bytes("\tEndOptions:") << L'\n';
            saveFile << converter.from_bytes("EndSquad:") << L'\n';
        }
        return true;
    }
}

bool SquadAutonomySettings::loadSettings(std::wstring savePath)
{
    //initialized = false;
    std::wifstream saveFile(savePath);
    if (!saveFile.is_open())
    {
        DebugLog("Load: Cannot open save file");
        return false;
    }

    DebugLog("Loading settings file at " + converter.to_bytes(savePath));
    squadSettings.clear();
    std::wstring wline;
    std::string type;

    while (std::getline(saveFile, wline))
    {
        std::string line = converter.to_bytes(wline);
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
            float startTime = 0.0f;
            float endTime = 24.0f;
            bool manTurrets = false;
            bool closeGate = false;
            bool doSleep = false;
            bool restUntilHealed = true;
            bool usePaidBeds = false;
            /*========================*/

            dataLine = line.substr(colon + 1);
            hand* hand = createHandfromLine(dataLine);

            if (hand) squad = hand->getPlatoon();
            else continue;

            //get home buildings and packages
            while (std::getline(saveFile, wline))
            {
                std::string line = converter.to_bytes(wline);
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
                    while (std::getline(saveFile, wline))
                    {
                        std::string line = converter.to_bytes(wline);
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
                    while (std::getline(saveFile, wline))
                    {
                        std::string line = converter.to_bytes(wline);
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
                        if (type == "StartWorkTime")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> startTime;
                            continue;
                        }
                        if (type == "EndWorkTime")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> endTime;
                            continue;
                        }
                        if (type == "ManTurrets")
                        {
                            if (dataLine == "true")
                            {
                                manTurrets = true;
                            }
                            continue;
                        }
                        if (type == "CloseGate")
                        {
                            if (dataLine == "true")
                            {
                                closeGate = true;
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
                settingsInfo->enableAutonomy(false);
                settingsInfo->unassignSquadHome();
                if (home)
                {
                    settingsInfo->setBuilding(home, true);
                    DebugLog("Load home: " + home->displayName);
                }
                if (work)
                {
                    settingsInfo->setBuilding(work, false);
                    DebugLog("Load work: " + work->displayName);
                }
                if (packages.size() > 0)
                {
                    settingsInfo->setPackages(packages);
                }
                settingsInfo->setStartWorkTime(startTime);
                settingsInfo->setEndWorkTime(endTime);
                settingsInfo->setManTurrets(manTurrets);
                settingsInfo->setCloseGate(closeGate);
                settingsInfo->setDoSleep(doSleep);
                settingsInfo->setRestUntilHealed(restUntilHealed);
                settingsInfo->setUsePaidBeds(usePaidBeds);
                settingsInfo->enableAutonomy(enable, false);
                lektorEx::push_back(squadSettings, settingsInfo);
            }
        }
    }
    saveFile.close();
    //initialized = true;
    return true;
}

hand* SquadAutonomySettings::createHandfromLine(std::string line)
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

SquadSettingsInfo* SquadAutonomySettings::getSquadSettings(Platoon* squad, bool createNew)
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
lektor<GameData*>* SquadAutonomySettings::getAIPackageList()
{
    return &_AIPackageList;
}
lektor<GameData*>* SquadAutonomySettings::getSquadTemplate()
{
    return &_squadTemplateList;
}
void SquadAutonomySettings::removeSquadSettings(Platoon* squad)
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
std::wstring SquadAutonomySettings::getConfigPath()
{
    return _cfgPath;
}

SquadSettingsInfo::SquadSettingsInfo(Platoon* squad) : _enabled(false), _squad(squad), _pi(nullptr), _homeBuilding(nullptr), _workBuilding(nullptr),
_startWorkTime(0.0), _endWorkTime(24.0), _manTurrets(false), _closeGate(false), _doSleep(false), _usePaidBeds(false), _restUntilHealed(true)
{
    Faction* faction = squad->getFaction();
    if (faction)
    {
        _pi = faction->isPlayer;
    }
}
Platoon* SquadSettingsInfo::getSquad()
{
    return _squad;
}
PlayerInterface* SquadSettingsInfo::getPlayerInterface()
{
    return _pi;
}
bool SquadSettingsInfo::isEnabled()
{
    return _enabled;
}
void SquadSettingsInfo::addPackage(int priority, GameData* data)
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
void SquadSettingsInfo::setPackages(const std::map<int, lektor<GameData*> >& packages)
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
void SquadSettingsInfo::clearPackages()
{
    _squadPackages.clear();
}
std::map<int, lektor<GameData*>> SquadSettingsInfo::getSquadPackages()
{
    return _squadPackages;
}

bool SquadAutonomy::SquadSettingsInfo::hasTask(TaskType)
{
    for (int i = 0; i < _squadPackages.size(); ++i)
    {
        for (int j = 0; j < _squadPackages.size(); ++j)
        {
            lektor<GameData*> goals;
            _squadPackages[i][j];
        }
    }
}

bool SquadSettingsInfo::enableAutonomy(bool enable, bool endAction)
{
    bool success = false;
    if (_enabled != enable)
    {
        if (enable)
        {
            success = SetAI(_squad, _squadPackages, endAction);
        }
        else
        {
            success = ResetAI(_squad, endAction);
        }

        if (success)
        {
            _enabled = enable;
        }
    }
    return success;
}
void SquadSettingsInfo::updateSquadPackages(bool endAction)
{
    if (!_enabled) return;
    bool success = false;
    if (_squadPackages.size() > 0)
    {
        ResetAI(_squad, endAction);
        success = SetAI(_squad, _squadPackages, endAction);
    }
    else
    {
        success = ResetAI(_squad, endAction);
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

Building* SquadSettingsInfo::getBuilding(bool home)
{
    if (home) return _homeBuilding;
    return _workBuilding;
}

void SquadSettingsInfo::setBuilding(Building* building, bool home)
{
    if (home) _homeBuilding = building;
    else _workBuilding = building;
}

bool SquadSettingsInfo::assignSquadHome(bool home)
{
    ActivePlatoon* activeSquad = _squad->getActivePlatoon();
    hand building = nullptr;
    TownBase* town = nullptr;
    bool success = false;
    if (home)
    {
        if (_homeBuilding)
        {
            building = _homeBuilding->getHandle();
        }
    }
    else
    {
        if (_workBuilding)
        {
            building = _workBuilding->getHandle();
        }
    }

    if (!building.isNull())
    {
        for (auto it = activeSquad->things.begin(); it != activeSquad->things.end(); ++it)
        {
            Character* obj = reinterpret_cast<Character*>(*it);
            StateBroadcastData* state = obj->getStateBroadcast();
            if (state) state->homeBuilding = building;
            /*Ownerships* own = obj->getOwnerships();
            if (own)
            {
                own->setHomeBuilding(building, _squad->getSquadType());
            }*/
        }
        _squad->getOwnerships()->setHomeBuilding(building, _squad->getSquadType());
        //DebugLog("Assign " + activeSquad->getName() + " -" + building.getBuilding()->displayName);
        success = true;
    }
    
    return success;
}
void SquadSettingsInfo::unassignSquadHome()
{
    ActivePlatoon* activeSquad = _squad->getActivePlatoon();
    if (_squad->getOwnerships())
    {
        for (auto it = activeSquad->things.begin(); it != activeSquad->things.end(); ++it)
        {
            Character* obj = reinterpret_cast<Character*>(*it);
            StateBroadcastData* state = obj->getStateBroadcast();
            if(state) state->homeBuilding = nullptr;
            Ownerships* own = obj->getOwnerships();
            if (own)
            {
                own->setHomeBuilding(nullptr, _squad->getSquadType());
                own->setHomeTown(nullptr, _squad->getSquadType());
            }
        }
        _squad->getOwnerships()->setHomeBuilding(nullptr, _squad->getSquadType());
        _squad->getOwnerships()->setHomeTown(nullptr, _squad->getSquadType());
    }
}
bool SquadSettingsInfo::hasHome()
{
    return (_homeBuilding || _workBuilding);
}
bool SquadSettingsInfo::getManTurrets()
{
    return _manTurrets;
}
void SquadSettingsInfo::setManTurrets(bool val)
{
    _manTurrets = val;
}
bool SquadAutonomy::SquadSettingsInfo::getCloseGate()
{
    return _closeGate;
}
void SquadAutonomy::SquadSettingsInfo::setCloseGate(bool val)
{
    _closeGate = val;
    ClearTask(_squad, STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT);
}
bool SquadSettingsInfo::getDoSleep()
{
    return _doSleep;
}
void SquadSettingsInfo::setDoSleep(bool val)
{
    _doSleep = val;
}
bool SquadSettingsInfo::getRestUntilHealed()
{
    return _restUntilHealed;
}
bool SquadSettingsInfo::getUsePaidBeds()
{
    return _usePaidBeds;
}
float SquadSettingsInfo::getStartWorkTime()
{
    return _startWorkTime;
}
float SquadSettingsInfo::getEndWorkTime()
{
    return _endWorkTime;
}
void SquadSettingsInfo::setStartWorkTime(float time)
{
    _startWorkTime = time;
}
void SquadSettingsInfo::setEndWorkTime(float time)
{
    _endWorkTime = time;
}
void SquadSettingsInfo::setRestUntilHealed(bool val)
{
    _restUntilHealed = val;
}
void SquadSettingsInfo::setUsePaidBeds(bool val)
{
    _usePaidBeds = val;
}
bool SquadSettingsInfo::isRestTime()
{
    //if (!_doSleep) return false;
    int currentHour = static_cast<int>(ou->getTimeStamp_inGameHours().getTotalHours()) % 24;
    int endTime = static_cast<int>(_endWorkTime);
    int startTime = static_cast<int>(_startWorkTime);
    bool restTime = false;
    //DebugLog("Current time: " + Ogre::StringConverter::toString(currentHour));
    //DebugLog("start time: " + Ogre::StringConverter::toString(startTime) + " end time : " + Ogre::StringConverter::toString(endTime));
    if (endTime == startTime) return true;
    if (endTime > startTime)
    {
        if (currentHour >= endTime || currentHour < startTime)
        {
            restTime = true;
        }
    }
    else
    {
        if (currentHour >= endTime && currentHour < startTime)
        {
            restTime = true;
        }
    }
    return restTime;
}