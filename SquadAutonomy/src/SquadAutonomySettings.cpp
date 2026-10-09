#include "SquadAutonomy.h"
#include "SquadAutonomySettings.h"
#include "SquadAutonomyModSettings.h"
#include "SquadAutonomySettingsUtils.h"

#include <Debug.h>

#include <ogre/OgreStringConverter.h>
#include <boost/scoped_ptr.hpp>

#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Building/UseableStuff.h>
#include <kenshi/Character.h>
#include <kenshi/StateBroadcastData.h>
#include <kenshi/Platoon.h>
#include <kenshi/Faction.h>
#include <kenshi/AI/AITaskSystem.h>
#include <kenshi/Tasker.h>
#include <kenshi/Building/Building.h>
#include <kenshi/util/lektor.h>

using namespace SquadAutonomy;

bool SquadAutonomySettings::initialized = false;

SquadAutonomySettings::SquadAutonomySettings()
{
    _initGameData();
    initialized = true;
}


SquadAutonomySettings& SquadAutonomySettings::getSingleton()
{
    static std::unique_ptr<SquadAutonomySettings> singleton;
    if (!singleton)
    {
        singleton.reset(new SquadAutonomySettings());
    }

    return *singleton.get();
}

void SquadAutonomySettings::_initGameData()
{
    lektor<GameData*> datas;
    ou->gamedata.getDataOfType(datas, AI_PACKAGE);
    std::string identifier = "<SquadAutonomy>";
    DebugLog("Initialized AI Packages");
    for (uint32_t i = 0; i < datas.size(); ++i)
    {
        const std::string& packageName = datas[i]->name;
        const std::vector<std::string>& configPackages = ModSettings::getSingleton().getConfigPackages();

        auto packageIt = std::find(configPackages.begin(), configPackages.end(), packageName);

        if (packageIt != configPackages.end())
        {
            DebugLog("Load package (cfg): " + packageName);
            _AIPackageList.push_back(datas[i]);
            continue;
        }

        if (packageName.size() >= identifier.size() && packageName.compare(packageName.size() - identifier.size(), identifier.size(), identifier) == 0)
        {
            DebugLog("Load package: " + packageName);
            _AIPackageList.push_back(datas[i]);
        }
    }
    std::sort(_AIPackageList.begin(), _AIPackageList.end(), [](GameData* a, GameData* b)
    {
        return a->name < b->name;
    });

    auto newEndIt = std::unique(_AIPackageList.begin(), _AIPackageList.end());
    _AIPackageList.erase(newEndIt, _AIPackageList.end());

    datas.clear();
    ou->gamedata.getDataOfType(datas, SQUAD_TEMPLATE);
    {
        for (uint32_t i = 0; i < datas.size(); ++i)
        {
            std::string packageName = datas[i]->name;
            if (packageName.size() >= identifier.size() && packageName.compare(packageName.size() - identifier.size(), identifier.size(), identifier) == 0)
            {
                DebugLog("Load package: " + packageName);
                _squadTemplateList.push_back(datas[i]);
            }
        }
    }
}

bool SquadAutonomySettings::saveSettings(const std::wstring& savePath)
{
    std::ofstream saveFile(savePath);
    if (!saveFile.is_open())
    {
        DebugLog("Save: Cannot open file");
        return false;
    }

    DebugLog("Saving settings to " + converter.to_bytes(savePath));
    //DebugLog("Saving " + Ogre::StringConverter::toString(squadSettings.size()) + " squads");

    saveFile << std::boolalpha; // output bools as text

    for (auto it = squadSettings.begin(); it != squadSettings.end(); ++it)
    {
        (*it)->writeToStream(saveFile);
    }

    return true;
}

bool SquadAutonomySettings::loadSettings(const std::wstring& savePath)
{
    //initialized = false;
    std::ifstream saveFile(savePath);
    if (!saveFile.is_open())
    {
        DebugLog("Load: Cannot open save file");
        return false;
    }

    DebugLog("Loading settings file at " + converter.to_bytes(savePath));
    squadSettings.clear();

    while (true)
    {
        std::unique_ptr<SquadSettingsInfo> currentSquadSettings = SquadSettingsInfo::readFromStream(saveFile);
        if (currentSquadSettings)
        {
            squadSettings.push_back(std::move(currentSquadSettings));
        }
        else
        {
            break;
        }
    }

    saveFile.close();
    //initialized = true;
    return true;
}

SquadSettingsInfo* SquadAutonomySettings::getSquadSettings(Platoon* squad, bool createNew)
{
    if (!squad)
    {
        return nullptr;
    }

    for (int i = 0; i < squadSettings.size(); ++i)
    {
        if (squad == squadSettings[i]->getSquad())
        {
            return squadSettings[i].get();
        }
    }
    if (createNew)
    {
        std::unique_ptr<SquadSettingsInfo> newSettings(new SquadSettingsInfo(*squad));
        squadSettings.push_back(std::move(newSettings));
        return squadSettings.back().get();
    }
    return nullptr;
}
std::vector<GameData*>* SquadAutonomySettings::getAIPackageList()
{
    return &_AIPackageList;
}
std::vector<GameData*>* SquadAutonomySettings::getSquadTemplate()
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
        squadSettings.erase(squadSettings.begin() + index);
    }
}


SquadSettingsInfo::SquadSettingsInfo(Platoon& squad) :
    _enabled(false),
    _squad(&squad),
    _pi(nullptr),
    _homeBuilding(nullptr),
    _workBuilding(nullptr),
    _startWorkTime(0.0),
    _endWorkTime(24.0),
    _doSleep(false),
    _usePaidBeds(false),
    _restUntilHealed(true),
    _restThreshold(50.0),
    _healedThreshold(90.0),
    _labourScience(1.0),
    _labourLabouring(1.0),
    _labourFarming(1.0),
    _labourMedic(1.0),
    _labourCooking(1.0),
    _labourEngineer(1.0),
    _labourRobotics(1.0),
    _labourWeaponSmith(1.0),
    _labourArmourSmith(1.0),
    _labourCrossbowSmith(1.0),
    _labourAutomaticMachine(1.0),
    _attackEnemies(true),
    _protectAllies(true),
    _doMedic(true),
    _doRescue(true),
    _manTurrets(false),
    _stayInsideGate(false),
    _closeGate(false)
{
    Faction* faction = squad.getFaction();
    if (faction)
    {
        _pi = faction->isPlayer;
    }
}

std::unique_ptr<SquadSettingsInfo> SquadSettingsInfo::readFromStream(std::istream& stream)
{
    std::string squadTag;

    auto streamOriginalPosition = stream.tellg();
    stream >> squadTag;

    if (squadTag != "Squad:")
    {
        stream.seekg(streamOriginalPosition);
        return nullptr;
    }

    std::string dataLine;
    std::getline(stream, dataLine);

    hand squadHand = parseHandSetting(dataLine);
    if (!squadHand.isValid())
    {
        return nullptr;
    }

    Platoon* squad = squadHand.getPlatoon();
    if (!squad)
    {
        return nullptr;
    }

    std::unique_ptr<SquadSettingsInfo> settingsInfo(new SquadSettingsInfo(*squad));
    settingsInfo->enableAutonomy(false);
    settingsInfo->unassignSquadHome();

    bool enable = false;

    DebugLog("Load Squad " + squad->activePlatoon->getName());

    std::string line;

    while (std::getline(stream, line))
    {
        line.erase(0, line.find_first_not_of(" \t"));
        auto colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }

        std::string type = line.substr(0, colon);
        //DebugLog(type);
        if (type == "EndSquad")
        {
            break;
        }

        if (type == "Enable")
        {
            dataLine = line.substr(colon + 1);
            SettingsUtils::parseBoolSetting(dataLine, enable);
            continue;
        }
        if (type == "HomeBuilding")
        {
            dataLine = line.substr(colon + 1);
            Building* home = parseBuildingSetting(dataLine);
            if (home)
            {
                settingsInfo->setBuilding(home, true);
                DebugLog("Load home: " + home->displayName);
            }
            continue;
        }
        if (type == "WorkBuilding")
        {
            dataLine = line.substr(colon + 1);
            Building* work = parseBuildingSetting(dataLine);
            if (work)
            {
                settingsInfo->setBuilding(work, false);
                DebugLog("Load work: " + work->displayName);
            }
            continue;
        }
        if (type == "Packages")
        {
            settingsInfo->readSquadPackagesFromStream(stream);
            continue;
        }
        if (type == "Options")
        {
            settingsInfo->readSquadOptionsFromStream(stream);
            continue;
        }
    }

    settingsInfo->enableAutonomy(enable, false);

    return settingsInfo;
}

bool SquadSettingsInfo::writeToStream(std::ostream &stream) const
{
    if (!_squad || !_squad->activePlatoon)
    {
        return false;
    }

    //DebugLog("Saving squad " + Ogre::StringConverter::toString(i));
    hand platoonHand = _squad->getHandle();
    if (!platoonHand)
    {
        return false;
    }

    stream << "Squad:";
    writeHandSettingToStream(stream, platoonHand, _squad->activePlatoon->getName());
    stream << '\n';

    stream << "\tEnable: " << _enabled << '\n';

    writeBuildingSettingToStream(stream, _homeBuilding, "HomeBuilding");
    writeBuildingSettingToStream(stream, _workBuilding, "WorkBuilding");

    writeSquadPackagesToStream(stream);
    writeSquadOptionsToStream(stream);

    stream << "EndSquad:" << '\n';

    return true;
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
    if (find != _squadPackages.end())
    {
        const bool alreadyHasPackage = std::find(find->second.begin(), find->second.end(), data) != find->second.end();
        if (!alreadyHasPackage)
        {
            find->second.push_back(data);
        }
    }
    else
    {
        _squadPackages[priority].push_back(data);
    }
}
void SquadSettingsInfo::setPackages(const std::map<int, std::vector<GameData*>>& packages)
{
    _squadPackages = packages;
}
void SquadSettingsInfo::clearPackages()
{
    _squadPackages.clear();
}
const std::map<int, std::vector<GameData*>>& SquadSettingsInfo::getSquadPackages()
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
float SquadAutonomy::SquadSettingsInfo::getRestThreshold()
{
    return _restThreshold;
}
float SquadSettingsInfo::getHealedThreshold()
{
    return _healedThreshold;
}
float SquadAutonomy::SquadSettingsInfo::getRestThresholdP()
{
    return _restThreshold/100.0;
}
float SquadSettingsInfo::getHealedThresholdP()
{
    return _healedThreshold/100.0;
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
void SquadAutonomy::SquadSettingsInfo::setRestThreshold(float val)
{
    _restThreshold = val;
}
void SquadSettingsInfo::setHealedThreshold(float val)
{
    _healedThreshold = val;
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

float SquadAutonomy::SquadSettingsInfo::getLabourScience()
{
    return _labourScience;
}

void SquadAutonomy::SquadSettingsInfo::setLabourScience(float val)
{
    _labourScience = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourEngineer()
{
    return _labourEngineer;
}

void SquadAutonomy::SquadSettingsInfo::setLabourEngineer(float val)
{
    _labourEngineer = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourRobotics()
{
    return _labourRobotics;
}

void SquadAutonomy::SquadSettingsInfo::setLabourRobotics(float val)
{
    _labourRobotics = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourMedic()
{
    return _labourMedic;
}

void SquadAutonomy::SquadSettingsInfo::setLabourMedic(float val)
{
    _labourMedic = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourLabouring()
{
    return _labourLabouring;
}

void SquadAutonomy::SquadSettingsInfo::setLabourLabouring(float val)
{
    _labourLabouring = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourFarming()
{
    return _labourFarming;
}

void SquadAutonomy::SquadSettingsInfo::setLabourFarming(float val)
{
    _labourFarming = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourCooking()
{
    return _labourCooking;
}

void SquadAutonomy::SquadSettingsInfo::setLabourCooking(float val)
{
    _labourCooking = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourWeaponSmith()
{
    return _labourWeaponSmith;
}

void SquadAutonomy::SquadSettingsInfo::setLabourWeaponSmith(float val)
{
    _labourWeaponSmith = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourArmourSmith()
{
    return _labourArmourSmith;
}

void SquadAutonomy::SquadSettingsInfo::setLabourArmourSmith(float val)
{
    _labourArmourSmith = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourCrossbowSmith()
{
    return _labourCrossbowSmith;
}

void SquadAutonomy::SquadSettingsInfo::setLabourCrossbowSmith(float val)
{
    _labourCrossbowSmith = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourAutomaticMachine()
{
    return _labourAutomaticMachine;
}

void SquadAutonomy::SquadSettingsInfo::setLabourAutomaticMachine(float val)
{
    _labourAutomaticMachine = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourOther()
{
    return _labourOther;
}

void SquadAutonomy::SquadSettingsInfo::setLabourOther(float val)
{
    _labourOther = val;
}

int SquadAutonomy::SquadSettingsInfo::getLabourPriority(UseableStuff* useable)
{
    if (useable->numOperatorsMax <= 0)
    {
        return _labourAutomaticMachine;
    }
    auto statUsed = useable->getStatUsed();
    switch (statUsed)
    {
    case STAT_SCIENCE:
        return _labourScience;
        break;
    case STAT_LABOURING:
        return _labourLabouring;
        break;
    case STAT_FARMING:
        return _labourFarming;
        break;
    case STAT_ENGINEERING:
        return _labourEngineer;
        break;
    case STAT_COOKING:
        return _labourCooking;
        break;
    case STAT_ROBOTICS:
        return _labourRobotics;
        break;
    case STAT_MEDIC:
        return _labourMedic;
        break;
    case STAT_SMITHING_WEAPON:
        return _labourWeaponSmith;
        break;
    case STAT_SMITHING_ARMOUR:
        return _labourArmourSmith;
        break;
    case STAT_SMITHING_BOW:
        return _labourCrossbowSmith;
        break;
    default:
        return _labourOther;
    }
    
}

bool SquadSettingsInfo::getAttackEnemies()
{
    return _attackEnemies;
}
void SquadSettingsInfo::setAttackEnemies(bool val)
{
    _attackEnemies = val;
}
bool SquadSettingsInfo::getProtectAllies()
{
    return _protectAllies;
}
void SquadSettingsInfo::setProtectAllies(bool val)
{
    _protectAllies = val;
}
bool SquadSettingsInfo::getDoMedic()
{
    return _doMedic;
}
void SquadSettingsInfo::setDoMedic(bool val)
{
    _doMedic = val;
}

bool SquadAutonomy::SquadSettingsInfo::getDoRescue()
{
    return _doRescue;
}

void SquadAutonomy::SquadSettingsInfo::setDoRescue(bool val)
{
    _doRescue = val;
}

bool SquadSettingsInfo::getManTurrets()
{
    return _manTurrets;
}
void SquadSettingsInfo::setManTurrets(bool val)
{
    _manTurrets = val;
}
bool SquadSettingsInfo::getStayInsideGate()
{
    return _stayInsideGate;
}
void SquadSettingsInfo::setStayInsideGate(bool val)
{
    _stayInsideGate = val;
    ClearTask(_squad, STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT);
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

hand SquadSettingsInfo::parseHandSetting(const std::string& dataLine)
{
    size_t start = dataLine.rfind('>');
    if (start == std::string::npos)
    {
        return hand();
    }

    std::string numbers = dataLine.substr(start + 1);
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

    return hand(index, serial, static_cast<itemType>(type), container, containerSerial);
}

Building* SquadSettingsInfo::parseBuildingSetting(const std::string& dataLine)
{
    hand buildingHand = parseHandSetting(dataLine);
    if (!buildingHand.isValid())
    {
        return nullptr;
    }

    return buildingHand.getBuilding();
}

void SquadSettingsInfo::readSquadPackagesFromStream(std::istream& stream)
{
    std::string line;

    while (std::getline(stream, line))
    {
        line.erase(0, line.find_first_not_of(" \t"));
        int priority;
        size_t colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }

        std::string type = line.substr(0, colon);
        //DebugLog(type);
        if (type == "EndPackages")
        {
            break;
        }

        std::stringstream ss(type);
        if (!(ss >> priority))
        {
            continue;
        }

        size_t start = line.find('<', colon);
        size_t end = line.rfind('>');
        if (!(start != std::string::npos && end != std::string::npos && end > start))
        {
            continue;
        }

        std::string packageName = line.substr(start + 1, end - start - 1);

        std::vector<GameData*>* _AIPackageList = SquadAutonomySettings::getSingleton().getAIPackageList();

        auto packageInListIt = std::find_if(
            _AIPackageList->begin(),
            _AIPackageList->end(),
            [&packageName](GameData* packageData){ return packageName == packageData->name; }
        );

        if (packageInListIt != _AIPackageList->end())
        {
            const bool alreadyHasPackage = std::find(_squadPackages[priority].begin(), _squadPackages[priority].end(), *packageInListIt) != _squadPackages[priority].end();
            if (!alreadyHasPackage)
            {
                _squadPackages[priority].push_back(*packageInListIt);
            }
        }
    }
}

void SquadSettingsInfo::readSquadOptionsFromStream(std::istream& stream)
{
    std::string line;

    while (std::getline(stream, line))
    {
        line.erase(0, line.find_first_not_of(" \t"));
        size_t colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }
        std::string type = line.substr(0, colon);
        std::string dataLine = line.substr(colon + 1);
        dataLine.erase(0, dataLine.find_first_not_of(" \t"));
        //DebugLog(type);
        //DebugLog(dataLine);
        if (type == "StartWorkTime")
        {
            SettingsUtils::parseFloatSetting(dataLine, _startWorkTime);
            continue;
        }
        if (type == "EndWorkTime")
        {
            SettingsUtils::parseFloatSetting(dataLine, _endWorkTime);
            continue;
        }
        if (type == "RestUntilHealed")
        {
            SettingsUtils::parseBoolSetting(dataLine, _restUntilHealed);
            continue;
        }
        if (type == "RestThreshold")
        {
            SettingsUtils::parseFloatSetting(dataLine, _restThreshold);
            continue;
        }
        if (type == "HealedThreshold")
        {
            SettingsUtils::parseFloatSetting(dataLine, _healedThreshold);
            continue;
        }
        if (type == "UsePaidBeds")
        {
            SettingsUtils::parseBoolSetting(dataLine, _usePaidBeds);
            continue;
        }
        if (type == "DoSleep")
        {
            SettingsUtils::parseBoolSetting(dataLine, _doSleep);
            continue;
        }
        if (type == "LabourScience")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourScience);
            continue;
        }
        if (type == "LabourLabouring")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourLabouring);
            continue;
        }
        if (type == "LabourFarming")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourFarming);
            continue;
        }
        if (type == "LabourEngineer")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourEngineer);
            continue;
        }
        if (type == "LabourCooking")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourCooking);
            continue;
        }
        if (type == "LabourRobotics")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourRobotics);
            continue;
        }
        if (type == "LabourMedic")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourMedic);
            continue;
        }
        if (type == "LabourWeaponSmith")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourWeaponSmith);
            continue;
        }
        if (type == "LabourArmourSmith")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourArmourSmith);
            continue;
        }
        if (type == "LabourCrossbowSmith")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourCrossbowSmith);
            continue;
        }
        if (type == "LabourAutomaticMachine")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourAutomaticMachine);
            continue;
        }
        if (type == "LabourOther")
        {
            SettingsUtils::parseFloatSetting(dataLine, _labourOther);
            continue;
        }
        if (type == "AttackEnemies")
        {
            SettingsUtils::parseBoolSetting(dataLine, _attackEnemies);
            continue;
        }
        if (type == "ProtectAllies")
        {
            SettingsUtils::parseBoolSetting(dataLine, _protectAllies);
            continue;
        }
        if (type == "DoMedic")
        {
            SettingsUtils::parseBoolSetting(dataLine, _doMedic);
            continue;
        }
        if (type == "DoRescue")
        {
            SettingsUtils::parseBoolSetting(dataLine, _doRescue);
            continue;
        }
        if (type == "ManTurrets")
        {
            SettingsUtils::parseBoolSetting(dataLine, _manTurrets);
            continue;
        }
        if (type == "StayInsideGate")
        {
            SettingsUtils::parseBoolSetting(dataLine, _stayInsideGate);
            continue;
        }
        if (type == "CloseGate")
        {
            SettingsUtils::parseBoolSetting(dataLine, _closeGate);
            continue;
        }
        if (type == "EndOptions")
        {
            break;
        }
    }
}

bool SquadSettingsInfo::writeHandSettingToStream(std::ostream& stream, hand handToWrite, const std::string& name)
{
    if (!handToWrite)
    {
        return false;
    }

    stream
        << " <" << name << "> "
        << handToWrite.index << ' '
        << handToWrite.serial << ' '
        << handToWrite.type << ' '
        << handToWrite.container << ' '
        << handToWrite.containerSerial;

    return true;
}

bool SquadSettingsInfo::writeBuildingSettingToStream(std::ostream &stream, const Building *building, const std::string& tag)
{
    bool writeOk = true;

    stream << '\t' << tag << ':';
    if (building)
    {
        writeOk = writeHandSettingToStream(stream, building->getHandle(), building->displayName);
    }
    stream << '\n';

    return writeOk;
}

void SquadSettingsInfo::writeSquadPackagesToStream(std::ostream &stream) const
{
    stream << "\tPackages:\n";
    if (!_squadPackages.empty())
    {
        for (auto it = _squadPackages.begin(); it != _squadPackages.end(); ++it)
        {
            auto data = it->second;

            for (int j = 0; j < data.size(); ++j)
            {
                stream
                    << "\t\t" << it->first << ':'
                    << " <" + data[j]->name + '>'
                    << '\n';
            }
        }
    }
    stream << "\tEndPackages:" << '\n';
}

void SquadSettingsInfo::writeSquadOptionsToStream(std::ostream &stream) const
{
    stream
        << "\tOptions:" << '\n'
        << "\t\tStartWorkTime: " << _startWorkTime << '\n'
        << "\t\tEndWorkTime: " << _endWorkTime << '\n'
        << "\t\tRestUntilHealed: " << _restUntilHealed << '\n'
        << "\t\t\tRestThreshold: " << _restThreshold << '\n'
        << "\t\t\tHealedThreshold: " << _healedThreshold << '\n'
        << "\t\tUsePaidBeds: " << _usePaidBeds << '\n'
        << "\t\tDoSleep: " << _doSleep << '\n'

        << "\t\tLabourScience: " << _labourScience << '\n'
        << "\t\tLabourLabouring: " << _labourLabouring << '\n'
        << "\t\tLabourFarming: " << _labourFarming << '\n'
        << "\t\tLabourEngineer: " << _labourEngineer << '\n'
        << "\t\tLabourCooking: " << _labourCooking << '\n'
        << "\t\tLabourRobotics: " << _labourRobotics << '\n'
        << "\t\tLabourMedic: " << _labourMedic << '\n'
        << "\t\tLabourWeaponSmith: " << _labourWeaponSmith << '\n'
        << "\t\tLabourArmourSmith: " << _labourArmourSmith << '\n'
        << "\t\tLabourCrossbowSmith: " << _labourCrossbowSmith << '\n'
        << "\t\tLabourAutomaticMachine: " << _labourAutomaticMachine << '\n'
        << "\t\tLabourOther: " << _labourOther << '\n'

        << "\t\tAttackEnemies: " << _attackEnemies << '\n'
        << "\t\tProtectAllies: " << _protectAllies << '\n'
        << "\t\tDoMedic: " << _doMedic << '\n'
        << "\t\tDoRescue: " << _doRescue << '\n'

        << "\t\tManTurrets: " << _manTurrets << '\n'
        << "\t\tStayInsideGate: " << _stayInsideGate << '\n'
        << "\t\tCloseGate: " << _closeGate << '\n'
        << "\tEndOptions:" << '\n';
}
