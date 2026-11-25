#include "SquadAutonomySettings.h"
#include "SquadAutonomyModSettings.h"
#include "SquadAutonomySettingsUtils.h"

#include <Debug.h>

#include <util/KenshiModUtil.h>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

#include <ogre/OgreStringConverter.h>
#include <boost/scoped_ptr.hpp>

#include <kenshi/Globals.h>
#include <kenshi/RootObjectFactory.h>
#include <kenshi/GameWorld.h>
#include <kenshi/Building/UseableStuff.h>
#include <kenshi/Character.h>
#include <kenshi/Town.h>
#include <kenshi/Item.h>
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
    _init();
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

void SquadAutonomySettings::_init()
{
    _saveFileName = L"SquadAutonomySave.json";
    _initGameData();
}

void SquadAutonomySettings::_populateItemDatasByFunctionList(lektor<GameData*> datas, ItemFunction itemFunc)
{
    std::sort(datas.begin(), datas.end(), [](GameData* a, GameData* b)
    {
        return a->name < b->name;
    });
    for (uint32_t i = 0; i < datas.size(); ++i)
    {
        auto data = datas[i];
        if (!data->filesdata["physics file"].empty())
        {
            _addItemToItemDatasByFunctionList(data, itemFunc);
        }
    }
    datas.clear();
}

void SquadAutonomy::SquadAutonomySettings::_addItemToItemDatasByFunctionList(GameData* data, ItemFunction itemFunc)
{
    auto itr = _itemDatasByFunctionList.find(itemFunc);
    if (itr == _itemDatasByFunctionList.end())
    {
        _itemDatasByFunctionList[itemFunc] = std::vector<GameData*>();
    }
    _itemDatasByFunctionList[itemFunc].push_back(data);
    Logger::logDebug("Loaded item: " + data->name + " func: " + sc::toString(static_cast<int>(itemFunc)));
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
    ou->gamedata.getDataOfType(datas, ITEM);
    ou->gamedata.getDataOfType(datas, NEST_ITEM);
    ou->gamedata.getDataOfType(datas, MAP_ITEM);
    for (uint32_t i = 0; i < datas.size(); ++i)
    {
        auto data = datas[i];
        ItemFunction itemFunc = static_cast<ItemFunction>(data->idata["item function"]);
        if (data->type != NEST_ITEM || !data->filesdata["physics file"].empty())
        {
            _addItemToItemDatasByFunctionList(data, itemFunc);
        }
    }
    datas.clear();
    ou->gamedata.getDataOfType(datas, WEAPON);
    ou->gamedata.getDataOfType(datas, CROSSBOW);
    _populateItemDatasByFunctionList(datas, ITEM_WEAPON);
    ou->gamedata.getDataOfType(datas, ARMOUR);
    _populateItemDatasByFunctionList(datas, ITEM_CLOTHING);
    ou->gamedata.getDataOfType(datas, CONTAINER);
    _populateItemDatasByFunctionList(datas, ITEM_CONTAINER);
    ou->gamedata.getDataOfType(datas, RESEARCH);
    _populateItemDatasByFunctionList(datas, ITEM_BLUEPRINT);
}

void SquadAutonomySettings::initSaveDocument()
{
    _saveDocument.SetObject();
}

bool SquadAutonomySettings::saveSettings(const std::wstring& savePath)
{
    std::wstring saveFilePath = savePath + _saveFileName;
    std::ofstream saveFile(saveFilePath);
    if (!saveFile.is_open())
    {
        DebugLog("Save: Cannot open file");
        return false;
    }

    initSaveDocument();
    rapidjson::Document::AllocatorType& allocator = _saveDocument.GetAllocator();
    rapidjson::Value squadSettingsInfos;
    squadSettingsInfos.SetArray();
    for (auto it = squadSettings.begin(); it != squadSettings.end(); ++it)
    {
        rapidjson::Value info = (*it)->getInfoMember(allocator);
        if (!info.IsNull()) squadSettingsInfos.PushBack(info, allocator);
    }
    _saveDocument.AddMember("squadSettingsInfo", squadSettingsInfos, allocator);
    rapidjson::OStreamWrapper osw(saveFile);
    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
    _saveDocument.Accept(writer);
    DebugLog("Saving settings to " + converter.to_bytes(saveFilePath));

    return true;
}

bool SquadAutonomySettings::loadSettings(const std::wstring& savePath)
{
    //initialized = false;
    std::wstring saveFilePath = savePath + _saveFileName;
    std::ifstream saveFile(saveFilePath);
    if (!saveFile.is_open())
    {
        // backward compatibility
        saveFilePath = savePath + L"SquadAutonomy.save";
        saveFile.open(saveFilePath);
        if (!saveFile.is_open())
        {
            DebugLog("Load: Cannot open save file");
            return false;
        }
        DebugLog("Loading settings file at " + converter.to_bytes(saveFilePath));
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
        // should delete the old file as well probably
        //DeleteFileW(saveFilePath.c_str());
        return true;
    }

    rapidjson::IStreamWrapper isw(saveFile);
    _saveDocument.ParseStream(isw);
    if (_saveDocument.HasParseError() || !_saveDocument.IsObject())
    {
        initSaveDocument();
        return false;
    }
    DebugLog("Loading settings file at " + converter.to_bytes(saveFilePath));
    squadSettings.clear();
    auto itr = _saveDocument.FindMember("squadSettingsInfo");
    if (itr != _saveDocument.MemberEnd())
    {
        for (auto subitr = itr->value.Begin(); subitr != itr->value.End(); ++subitr)
        {
            squadSettings.push_back(SquadSettingsInfo::readFromMember(*subitr));
        }
    }
    saveFile.close();
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
std::unordered_map<ItemFunction, std::vector<GameData*>>* SquadAutonomy::SquadAutonomySettings::getItemDatasList()
{
    return &_itemDatasByFunctionList;
}
const std::vector<GameData*> SquadAutonomy::SquadAutonomySettings::getItemDatas(ItemFunction itemFunc)
{
    auto itr = _itemDatasByFunctionList.find(itemFunc);
    if (itr != _itemDatasByFunctionList.end())
    {
        return (*itr).second;
    }
    return std::vector<GameData*>();
}
std::vector<TownBase*>* SquadAutonomy::SquadAutonomySettings::getTownBaseList()
{
    return &_townBaseList;
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
    _autoMovementSpeed(false),
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
    _labourOther(1.0),
    _attackEnemies(true),
    _ignoreEnemiesInNonHostile(true),
    _protectAllies(true),
    _doMedic(true),
    _doRescue(true),
    _rescueOwnSquadOnly(true),
    _manTurrets(false),
    _stayInsideGate(false),
    _closeGate(false),
    _doShopping(false),
    _buyItems(),
    _sellItems(),
    _doLooting(false),
    _doAnimalLooting(true),
    _putLootsInStorage(false),
    _putResourcesInStorage(false),
    _ignoreEquipped(true),
    _forbidItems(),
    _stopAtTownsAlongTheWay(false),
    _targetTownIndex(0),
    _targetTowns()
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

std::unique_ptr<SquadSettingsInfo> SquadAutonomy::SquadSettingsInfo::readFromMember(rapidjson::Value& member)
{
    auto itr = member.FindMember("squad");
    hand hand;
    if (itr != member.MemberEnd())
    {
        rapidjson::Value& squad = itr->value;
        auto itr = squad.FindMember("hand");
        if (itr != squad.MemberEnd())
        {
            hand = parseHandMember(itr->value);
        }
    }
    if (!hand.isValid())
    {
        return nullptr;
    }
    Platoon* squad = hand.getPlatoon();
    hand.setNull();
    if (!squad)
    {
        return nullptr;
    }
    std::unique_ptr<SquadSettingsInfo> settingsInfo(new SquadSettingsInfo(*squad));
    settingsInfo->enableAutonomy(false);
    settingsInfo->unassignSquadHome();
    bool enable = false;
    DebugLog("Load Squad " + squad->activePlatoon->getName());

    itr = member.FindMember("enable");
    if (itr != member.MemberEnd())
    {
        enable = itr->value.GetBool();
    }

    itr = member.FindMember("homeBuilding");
    if (itr != member.MemberEnd())
    {
        settingsInfo->setBuilding(parseBuildingMember(itr->value), true);
    }
    itr = member.FindMember("workBuilding");
    if (itr != member.MemberEnd())
    {
        settingsInfo->setBuilding(parseBuildingMember(itr->value), false);
    }

    itr = member.FindMember("packages");
    if (itr != member.MemberEnd())
    {
        settingsInfo->readSquadPackagesFromMember(itr->value);
    }

    itr = member.FindMember("options");
    if (itr != member.MemberEnd())
    {
        settingsInfo->readSquadOptionsFromMember(itr->value);
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
    
    /*
    stream << "Squad:";
    writeHandSettingToStream(stream, platoonHand, _squad->activePlatoon->getName());
    stream << '\n';

    stream << "\tEnable: " << _enabled << '\n';

    writeBuildingSettingToStream(stream, _homeBuilding, "HomeBuilding");
    writeBuildingSettingToStream(stream, _workBuilding, "WorkBuilding");

    writeSquadPackagesToStream(stream);
    writeSquadOptionsToStream(stream);

    stream << "EndSquad:" << '\n';
    */
    return true;
}

rapidjson::Value SquadSettingsInfo::getInfoMember(rapidjson::Document::AllocatorType& allocator) const
{
    if (!_squad || !_squad->activePlatoon)
    {
        return rapidjson::Value();
    }

    hand platoonHand = _squad->getHandle();
    if (!platoonHand)
    {
        return rapidjson::Value();
    }
    
    rapidjson::Value settingsInfo;
    settingsInfo.SetObject();
    settingsInfo.AddMember("squad", getHandMember(allocator, platoonHand, _squad->activePlatoon->getName()), allocator);
    settingsInfo.AddMember("enable", _enabled, allocator);
    rapidjson::Value home;
    if (_homeBuilding) home = getHandMember(allocator, _homeBuilding->getHandle(), _homeBuilding->displayName);
    settingsInfo.AddMember("homeBuilding", home, allocator);
    rapidjson::Value work;
    if (_workBuilding) work = getHandMember(allocator, _workBuilding->getHandle(), _workBuilding->displayName);
    settingsInfo.AddMember("workBuilding", work, allocator);
    settingsInfo.AddMember("packages", getSquadPackagesMember(allocator), allocator);
    settingsInfo.AddMember("options", getSquadOptionsMember(allocator), allocator);
    //doc.AddMember("settingsInfo", settingsInfo, allocator);

    return settingsInfo;
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

    addTasksFromPackage(data);
    addSignalFromPackage(data);
}

void SquadSettingsInfo::addTasksFromPackage(GameData* data)
{
    lektor<GameData*> aiGoalDatas;
    lektor<GameData*> inheritDatas;
    data->getAllFromListAsDatas("inherits from", inheritDatas, data->sourceContainer, AI_PACKAGE);
    for (int i = 0; i < inheritDatas.size(); ++i)
    {
        addTasksFromPackage(inheritDatas[i]);
    }
    addTasksFromGoals(data, aiGoalDatas);
}

void SquadSettingsInfo::addTasksFromGoals(GameData* data, lektor<GameData*> &goalDatas)
{
    std::string keys[] = { "Leader AI Goals", "Slave AI Goals", "Squad AI Goals", "Squad2 AI Goals" };
    Logger::log(data->name);
    size_t length = sizeof(keys) / sizeof(keys[0]);
    for (int i = 0; i < length; ++i)
    {
        Logger::log(keys[i]);
        data->getAllFromListAsDatas(keys[i], goalDatas, data->getSourceContainer(), AI_TASK);
        for (int j = 0; j < goalDatas.size(); ++j)
        {
            _squadTasks.insert(static_cast<TaskType>(goalDatas[j]->idata["enum"]));
            Logger::log("Add task: " + goalDatas[j]->name);
        }
        goalDatas.clear();
    }
}

void SquadSettingsInfo::addSignalFromPackage(GameData* data)
{
    _squadSignals.insert(static_cast<BlackboardSignalFunctions>(data->idata["signal func"]));
    Logger::log("Add signal: " + sc::toString(data->idata["signal func"]));
}

void SquadSettingsInfo::setPackages(const std::map<int, std::vector<GameData*>>& packages)
{
    _squadPackages = packages;
}
void SquadSettingsInfo::clearPackages()
{
    _squadPackages.clear();
    _squadTasks.clear();
    _squadSignals.clear();
}
const std::map<int, std::vector<GameData*>>& SquadSettingsInfo::getSquadPackages()
{
    return _squadPackages;
}

bool SquadAutonomy::SquadSettingsInfo::hasTask(TaskType key)
{
    auto it = _squadTasks.find(key);
    if (it != _squadTasks.end()) return true;
    else return false;
}

bool SquadAutonomy::SquadSettingsInfo::hasSignal(BlackboardSignalFunctions key)
{
    auto it = _squadSignals.find(key);
    if (it != _squadSignals.end()) return true;
    else return false;
}

// I forgot what I was trying to do here
void getTasksFromData(GameData* data, std::unordered_set<TaskType>& type)
{
    type.clear();
    lektor<GameData*> aiTasks;
    data->getAllFromListAsDatas("Leader AI goal", aiTasks, data->getSourceContainer(), AI_TASK);
    for (int i = 0; i < aiTasks.size(); ++i)
    {
        // I forgot what I was trying to do here
    }
}

bool SquadSettingsInfo::enableAutonomy(bool enable, bool endAction)
{
    bool success = false;
    if (_enabled != enable)
    {
        if (enable)
        {

            if (!assignSquadHome(false)) //try set home to work
            {
                if (!assignSquadHome(true)) //if fail set home to home
                {
                    SetNearestFriendlyTownAsHome(_squad);
                }
            }

            success = SetAI(_squad, _squadPackages, endAction);
        }
        else
        {
            success = ResetAI(_squad, endAction);
            unassignSquadHome();
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
            Ownerships* own = obj->getOwnerships();
            if (own)
            {
                own->_homeBuilding = building;
                own->_homeTown = building.getBuilding()->getTown();
            }
        }
        _squad->getOwnerships()->_homeBuilding = building;
        _squad->getOwnerships()->_homeTown = building.getBuilding()->getTown();

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
                own->_homeBuilding = nullptr;
                own->_homeTown = nullptr;
            }
        }
        _squad->getOwnerships()->_homeBuilding = nullptr;
        _squad->getOwnerships()->_homeTown = nullptr;
    }
}
bool SquadSettingsInfo::hasHome()
{
    return (_homeBuilding || _workBuilding);
}
bool SquadSettingsInfo::getDoSleep() const
{
    return _doSleep;
}
void SquadSettingsInfo::setDoSleep(bool val)
{
    _doSleep = val;
}
float SquadSettingsInfo::getStartWorkTime() const
{
    return _startWorkTime;
}
void SquadSettingsInfo::setStartWorkTime(float time)
{
    _startWorkTime = time;
}
float SquadSettingsInfo::getEndWorkTime() const
{
    return _endWorkTime;
}
void SquadSettingsInfo::setEndWorkTime(float time)
{
    _endWorkTime = time;
}
bool SquadAutonomy::SquadSettingsInfo::getAutoMovementSpeed() const
{
    return _autoMovementSpeed;
}
void SquadAutonomy::SquadSettingsInfo::setAutoMovementSpeed(bool val)
{
    _autoMovementSpeed = val;
}
bool SquadSettingsInfo::getRestUntilHealed() const
{
    return _restUntilHealed;
}
void SquadSettingsInfo::setRestUntilHealed(bool val)
{
    _restUntilHealed = val;
}
float SquadAutonomy::SquadSettingsInfo::getRestThreshold() const
{
    return _restThreshold;
}
float SquadAutonomy::SquadSettingsInfo::getRestThresholdP() const
{
    return _restThreshold/100.0;
}
void SquadAutonomy::SquadSettingsInfo::setRestThreshold(float val)
{
    _restThreshold = val;
}
float SquadSettingsInfo::getHealedThreshold() const
{
    return _healedThreshold;
}
float SquadSettingsInfo::getHealedThresholdP() const
{
    return _healedThreshold/100.0;
}
void SquadSettingsInfo::setHealedThreshold(float val)
{
    _healedThreshold = val;
}
bool SquadSettingsInfo::getUsePaidBeds() const
{
    return _usePaidBeds;
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

float SquadAutonomy::SquadSettingsInfo::getLabourScience() const
{
    return _labourScience;
}

void SquadAutonomy::SquadSettingsInfo::setLabourScience(float val)
{
    _labourScience = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourEngineer() const
{
    return _labourEngineer;
}

void SquadAutonomy::SquadSettingsInfo::setLabourEngineer(float val)
{
    _labourEngineer = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourRobotics() const
{
    return _labourRobotics;
}

void SquadAutonomy::SquadSettingsInfo::setLabourRobotics(float val)
{
    _labourRobotics = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourMedic() const
{
    return _labourMedic;
}

void SquadAutonomy::SquadSettingsInfo::setLabourMedic(float val)
{
    _labourMedic = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourLabouring() const
{
    return _labourLabouring;
}

void SquadAutonomy::SquadSettingsInfo::setLabourLabouring(float val)
{
    _labourLabouring = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourFarming() const
{
    return _labourFarming;
}

void SquadAutonomy::SquadSettingsInfo::setLabourFarming(float val)
{
    _labourFarming = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourCooking() const
{
    return _labourCooking;
}

void SquadAutonomy::SquadSettingsInfo::setLabourCooking(float val)
{
    _labourCooking = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourWeaponSmith() const
{
    return _labourWeaponSmith;
}

void SquadAutonomy::SquadSettingsInfo::setLabourWeaponSmith(float val)
{
    _labourWeaponSmith = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourArmourSmith() const
{
    return _labourArmourSmith;
}

void SquadAutonomy::SquadSettingsInfo::setLabourArmourSmith(float val)
{
    _labourArmourSmith = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourCrossbowSmith() const
{
    return _labourCrossbowSmith;
}

void SquadAutonomy::SquadSettingsInfo::setLabourCrossbowSmith(float val)
{
    _labourCrossbowSmith = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourAutomaticMachine() const
{
    return _labourAutomaticMachine;
}

void SquadAutonomy::SquadSettingsInfo::setLabourAutomaticMachine(float val)
{
    _labourAutomaticMachine = val;
}

float SquadAutonomy::SquadSettingsInfo::getLabourOther() const
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

bool SquadSettingsInfo::getAttackEnemies() const
{
    return _attackEnemies;
}
void SquadSettingsInfo::setAttackEnemies(bool val)
{
    _attackEnemies = val;
}
bool SquadAutonomy::SquadSettingsInfo::getIgnoreEnemiesInNonHostile() const
{
    return _ignoreEnemiesInNonHostile;
}
void SquadAutonomy::SquadSettingsInfo::setIgnoreEnemiesInNonHostile(bool val)
{
    _ignoreEnemiesInNonHostile = val;
}
bool SquadSettingsInfo::getProtectAllies() const
{
    return _protectAllies;
}
void SquadSettingsInfo::setProtectAllies(bool val)
{
    _protectAllies = val;
}
bool SquadSettingsInfo::getDoMedic() const
{
    return _doMedic;
}
void SquadSettingsInfo::setDoMedic(bool val)
{
    _doMedic = val;
}

bool SquadAutonomy::SquadSettingsInfo::getDoRescue() const
{
    return _doRescue;
}

void SquadAutonomy::SquadSettingsInfo::setDoRescue(bool val)
{
    _doRescue = val;
}

bool SquadAutonomy::SquadSettingsInfo::getRescueOwnSquadOnly() const
{
    return _rescueOwnSquadOnly;
}

void SquadAutonomy::SquadSettingsInfo::setRescueOwnSquadOnly(bool val)
{
    _rescueOwnSquadOnly = val;
}

bool SquadSettingsInfo::getManTurrets() const
{
    return _manTurrets;
}
void SquadSettingsInfo::setManTurrets(bool val)
{
    _manTurrets = val;
}
bool SquadSettingsInfo::getStayInsideGate() const
{
    return _stayInsideGate;
}
void SquadSettingsInfo::setStayInsideGate(bool val)
{
    _stayInsideGate = val;
    ClearTask(_squad, STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT);
}
bool SquadAutonomy::SquadSettingsInfo::getCloseGate() const
{
    return _closeGate;
}
void SquadAutonomy::SquadSettingsInfo::setCloseGate(bool val)
{
    _closeGate = val;
    ClearTask(_squad, STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT);
}

bool SquadAutonomy::SquadSettingsInfo::getDoShopping() const
{
    return _doShopping;
}

void SquadAutonomy::SquadSettingsInfo::setDoShopping(bool val)
{
    _doShopping = val;
}

std::unordered_map<ItemFunction, ShoppingItemInfo> SquadAutonomy::SquadSettingsInfo::getBuyItems() const
{
    return _buyItems;
}

std::unordered_map<ItemFunction, ShoppingItemInfo>* SquadAutonomy::SquadSettingsInfo::getBuyItemsPtr()
{
    return &_buyItems;
}

void SquadAutonomy::SquadSettingsInfo::setBuyItems(std::unordered_map<ItemFunction, ShoppingItemInfo> val)
{
    _buyItems = val;
}

void SquadAutonomy::SquadSettingsInfo::addBuyItem(ItemFunction itemFunction, int quantity)
{
    ShoppingItemInfo item(quantity);
    _buyItems[itemFunction] = item;
}

std::unordered_map<ItemFunction, ShoppingItemInfo> SquadAutonomy::SquadSettingsInfo::getSellItems() const
{
    return _sellItems;
}

std::unordered_map<ItemFunction, ShoppingItemInfo>* SquadAutonomy::SquadSettingsInfo::getSellItemsPtr()
{
    return &_sellItems;
}

void SquadAutonomy::SquadSettingsInfo::setSellItems(std::unordered_map<ItemFunction, ShoppingItemInfo> val)
{
    _sellItems = val;
}

void SquadAutonomy::SquadSettingsInfo::addSellItem(ItemFunction itemFunction, int quantity)
{
    ShoppingItemInfo item(quantity);
    _sellItems[itemFunction] = item;
}

bool SquadAutonomy::SquadSettingsInfo::getDoLooting() const
{
    return _doLooting;
}

void SquadAutonomy::SquadSettingsInfo::setDoLooting(bool val)
{
    _doLooting = val;
}

bool SquadAutonomy::SquadSettingsInfo::getDoAnimalLooting() const
{
    return _doAnimalLooting;
}

void SquadAutonomy::SquadSettingsInfo::setDoAnimalLooting(bool val)
{
    _doAnimalLooting = val;
}

bool SquadAutonomy::SquadSettingsInfo::getPutLootsInStorage() const
{
    return _putLootsInStorage;
}

void SquadAutonomy::SquadSettingsInfo::setPutLootsInStorage(bool val)
{
    _putLootsInStorage = val;
}

bool SquadAutonomy::SquadSettingsInfo::getPutResourcesInStorage() const
{
    return _putResourcesInStorage;
}

void SquadAutonomy::SquadSettingsInfo::setPutResourcesInStorage(bool val)
{
    _putResourcesInStorage = val;
}

bool SquadAutonomy::SquadSettingsInfo::getIgnoreEquipped() const
{
    return _ignoreEquipped;
}

void SquadAutonomy::SquadSettingsInfo::setIgnoreEquipped(bool val)
{
    _ignoreEquipped = val;
}

std::unordered_set<ItemFunction> SquadAutonomy::SquadSettingsInfo::getForbidItems() const
{
    return _forbidItems;
}

bool SquadAutonomy::SquadSettingsInfo::hasForbidItem(ItemFunction itemFunction)
{
    if (_forbidItems.find(itemFunction) != _forbidItems.end())
    {
        return true;
    }
    return false;
}

void SquadAutonomy::SquadSettingsInfo::addForbidItem(ItemFunction itemFunction)
{
    _forbidItems.insert(itemFunction);
}

void SquadAutonomy::SquadSettingsInfo::clearForbidItems()
{
    _forbidItems.clear();
}

bool SquadAutonomy::SquadSettingsInfo::getStopAtTownsAlongTheWay() const
{
    return _stopAtTownsAlongTheWay;
}

void SquadAutonomy::SquadSettingsInfo::setStopAtTownsAlongTheWay(bool val)
{
    _stopAtTownsAlongTheWay = val;
}

void SquadAutonomy::SquadSettingsInfo::addTargetTown(TownBase* town)
{
    _targetTowns.push_back(town);
}

void SquadAutonomy::SquadSettingsInfo::clearTargetTown()
{
    _targetTowns.clear();
}

TownBase* SquadAutonomy::SquadSettingsInfo::getCurrentTargetTown()
{
    return _targetTowns[_targetTownIndex];
}

void SquadAutonomy::SquadSettingsInfo::nextTargetTown()
{
    _targetTownIndex = (_targetTownIndex + 1) % _targetTowns.size();
}

int SquadAutonomy::SquadSettingsInfo::getTargetTownIndex()
{
    return _targetTownIndex;
}

int& SquadAutonomy::SquadSettingsInfo::getTargetTownIndexRef()
{
    return _targetTownIndex;
}

void SquadAutonomy::SquadSettingsInfo::setTargetTownIndex(int val)
{
    _targetTownIndex = val;
}

int SquadAutonomy::SquadSettingsInfo::getTargetTownIndex() const
{
    return _targetTownIndex;
}

std::vector<TownBase*> SquadAutonomy::SquadSettingsInfo::getTargetTowns() const
{
    return _targetTowns;
}

std::vector<TownBase*>* SquadAutonomy::SquadSettingsInfo::getTargetTownsPtr()
{
    return &_targetTowns;
}

rapidjson::Value SquadSettingsInfo::getHandMember(rapidjson::Document::AllocatorType& allocator, hand handToWrite, const std::string& name)
{
    if (!handToWrite.isValid())
    {
        return rapidjson::Value();
    }

    rapidjson::Value obj;
    obj.SetObject();
    rapidjson::Value handName;
    handName.SetString(name.c_str(), allocator);
    if (!name.empty()) obj.AddMember("name", handName, allocator);
    rapidjson::Value objHand;
    objHand.SetArray();
    objHand.PushBack<int>(static_cast<int>(handToWrite.type), allocator);
    objHand.PushBack<unsigned int>(handToWrite.container, allocator);
    objHand.PushBack<unsigned int>(handToWrite.containerSerial, allocator);
    objHand.PushBack<unsigned int>(handToWrite.index, allocator);
    objHand.PushBack<unsigned int>(handToWrite.serial, allocator);
    obj.AddMember("hand", objHand, allocator);
    return obj;

}

rapidjson::Value SquadSettingsInfo::getSquadPackagesMember(rapidjson::Document::AllocatorType& allocator) const
{
    rapidjson::Value packages;
    packages.SetArray();
    if (!_squadPackages.empty())
    {
        for (auto it = _squadPackages.begin(); it != _squadPackages.end(); ++it)
        {
            auto data = it->second;

            for (int i = 0; i < data.size(); ++i)
            {
                rapidjson::Value package;
                rapidjson::Value packageName;
                packageName.SetString(data[i]->name.c_str(), allocator);
                package.SetObject();
                package.AddMember("priority", it->first, allocator);
                package.AddMember("name", packageName, allocator);
                packages.PushBack(package, allocator);
            }
        }
    }
    return packages;
}

rapidjson::Value SquadAutonomy::SquadSettingsInfo::getSquadOptionsMember(rapidjson::Document::AllocatorType& allocator) const
{
    rapidjson::Value options;
    options.SetObject();

    rapidjson::Value general;
    general.SetObject();
    general.AddMember("startWorkTime", _startWorkTime, allocator);
    general.AddMember("endWorkTime", _endWorkTime, allocator);
    general.AddMember("autoMovementSpeed", _autoMovementSpeed, allocator);
    general.AddMember("restUntilHealed", _restUntilHealed, allocator);
    general.AddMember("restThreshold", _restThreshold, allocator);
    general.AddMember("healedThreshold", _healedThreshold, allocator);
    general.AddMember("usePaidBeds", _usePaidBeds, allocator);
    general.AddMember("doSleep", _doSleep, allocator);
    options.AddMember("general", general, allocator);

    rapidjson::Value labour;
    labour.SetObject();
    labour.AddMember("labourScience", _labourScience, allocator);
    labour.AddMember("labourLabouring", _labourLabouring, allocator);
    labour.AddMember("labourFarming", _labourFarming, allocator);
    labour.AddMember("labourEngineer", _labourEngineer, allocator);
    labour.AddMember("labourCooking", _labourCooking, allocator);
    labour.AddMember("labourRobotics", _labourRobotics, allocator);
    labour.AddMember("labourMedic", _labourMedic, allocator);
    labour.AddMember("labourWeaponSmith", _labourWeaponSmith, allocator);
    labour.AddMember("labourArmourSmith", _labourArmourSmith, allocator);
    labour.AddMember("labourCrossbowSmith", _labourCrossbowSmith, allocator);
    labour.AddMember("labourAutomaticMachine", _labourAutomaticMachine, allocator);
    labour.AddMember("labourOther", _labourOther, allocator);
    options.AddMember("labour", labour, allocator);

    rapidjson::Value combat;
    combat.SetObject();
    combat.AddMember("attackEnemies", _attackEnemies, allocator);
    combat.AddMember("ignoreEnemiesInNonHostile", _ignoreEnemiesInNonHostile, allocator);
    combat.AddMember("protectAllies", _protectAllies, allocator);
    combat.AddMember("doMedic", _doMedic, allocator);
    combat.AddMember("doRescue", _doRescue, allocator);
    combat.AddMember("rescueOwnSquadOnly", _rescueOwnSquadOnly, allocator);
    options.AddMember("combat", combat, allocator);

    rapidjson::Value guard;
    guard.SetObject();
    guard.AddMember("manTurrets", _manTurrets, allocator);
    guard.AddMember("stayInsideGate", _stayInsideGate, allocator);
    guard.AddMember("closeGate", _closeGate, allocator);
    options.AddMember("guard", guard, allocator);

    rapidjson::Value shopping;
    shopping.SetObject();
    shopping.AddMember("doShopping", _doShopping, allocator);
    shopping.AddMember("buyItems", getShoppingItemsMember(allocator, _buyItems, true), allocator);
    shopping.AddMember("sellItems", getShoppingItemsMember(allocator, _sellItems, false), allocator);
    options.AddMember("shopping", shopping, allocator);

    rapidjson::Value inventory;
    inventory.SetObject();
    inventory.AddMember("doLooting", _doLooting, allocator);
    inventory.AddMember("doAnimalLooting", _doAnimalLooting, allocator);
    inventory.AddMember("putLootsInStorage", _putLootsInStorage, allocator);
    inventory.AddMember("putResourcesInStorage", _putResourcesInStorage, allocator);
    inventory.AddMember("ignoreEquipped", _ignoreEquipped, allocator);
    inventory.AddMember("forbidItems", getForbidItemsMember(allocator, _forbidItems), allocator);
    options.AddMember("inventory", inventory, allocator);

    rapidjson::Value travel;
    travel.SetObject();
    travel.AddMember("stopAtTownsAlongTheWay", _stopAtTownsAlongTheWay, allocator);
    travel.AddMember("targetTownIndex", _targetTownIndex, allocator);
    travel.AddMember("targetTowns", getTargetTownsMember(allocator, _targetTowns), allocator);
    options.AddMember("travel", travel, allocator);

    return options;
}
rapidjson::Value SquadSettingsInfo::getShoppingItemsMember(rapidjson::Document::AllocatorType& allocator, const std::unordered_map<ItemFunction, ShoppingItemInfo> shoppingItems, bool buying)
{
    rapidjson::Value items;
    items.SetArray();
    for (auto iter = shoppingItems.begin(); iter != shoppingItems.end(); ++iter)
    {
        ShoppingItemInfo info = iter->second;
        rapidjson::Value item;
        item.SetObject();
        item.AddMember("itemFunction", static_cast<int>(iter->first), allocator);
        item.AddMember("quantity", info.quantity, allocator);
        item.AddMember("onlyGoodDeal", info.onlyGoodDeal, allocator);
        if (!buying) item.AddMember("onlyStolenItems", info.onlyStolenItems, allocator);

        rapidjson::Value shopItemDatas;
        shopItemDatas.SetArray();
        for (auto dataIter = info.shopItemDatas.begin(); dataIter != info.shopItemDatas.end(); ++dataIter)
        {
            GameData* data = *dataIter;
            rapidjson::Value itemData;
            itemData.SetObject();
            itemData.AddMember("itemType", static_cast<int>(data->type), allocator);
            rapidjson::Value dataName;
            dataName.SetString((data->name).c_str(), allocator);
            itemData.AddMember("name", dataName, allocator);
            shopItemDatas.PushBack(itemData, allocator);
        }
        item.AddMember("shopItemDatas", shopItemDatas, allocator);

        item.AddMember("excludeTowns", info.excludeTowns, allocator);
        rapidjson::Value shopTowns;
        shopTowns.SetArray();
        for (auto townIter = info.shopTowns.begin(); townIter != info.shopTowns.end(); ++townIter)
        {
            TownBase* town = *townIter;
            shopTowns.PushBack(getHandMember(allocator, town->getHandle(), KenshiModUtil::getTownBiomeName(town)), allocator);
        }
        item.AddMember("shopTowns", shopTowns, allocator);

        items.PushBack(item, allocator);
    }
    return items;
}

rapidjson::Value SquadSettingsInfo::getForbidItemsMember(rapidjson::Document::AllocatorType& allocator, const std::unordered_set<ItemFunction> forbidItems)
{
    rapidjson::Value items;
    items.SetArray();
    for (auto iter = forbidItems.begin(); iter != forbidItems.end(); ++iter)
    {
        rapidjson::Value item;
        item.SetObject();
        item.AddMember("itemFunction", static_cast<int>(*iter), allocator);

        items.PushBack(item, allocator);
    }
    return items;
}

rapidjson::Value SquadAutonomy::SquadSettingsInfo::getTargetTownsMember(rapidjson::Document::AllocatorType& allocator, const std::vector<TownBase*> targetTowns)
{
    rapidjson::Value towns;
    towns.SetArray();
    for (int i = 0; i < targetTowns.size(); ++i)
    {
        towns.PushBack(getHandMember(allocator, targetTowns[i]->getHandle(), KenshiModUtil::getTownBiomeName(targetTowns[i])), allocator);
    }
    return towns;
}

hand SquadSettingsInfo::parseHandMember(rapidjson::Value& member)
{
    if (!member.IsArray() || member.Size() != 5) return nullptr;
    hand hand;
    hand.type = static_cast<itemType>(member[0].GetInt());
    hand.container = member[1].GetUint();
    hand.containerSerial = member[2].GetUint();
    hand.index = member[3].GetUint();
    hand.serial = member[4].GetUint();
    return hand;
}

Building* SquadSettingsInfo::parseBuildingMember(rapidjson::Value& member)
{

    auto itr = member.FindMember("hand");
    if (itr != member.MemberEnd())
    {
        hand hand = parseHandMember(itr->value);
        if (hand.isValid())
        {
            return hand.getBuilding();
        }
    }

    return nullptr;
}

void SquadAutonomy::SquadSettingsInfo::readSquadPackagesFromMember(rapidjson::Value& member)
{
    std::vector<GameData*>* _AIPackageList = SquadAutonomySettings::getSingleton().getAIPackageList();
    for (auto itr = member.Begin(); itr != member.End(); ++itr)
    {
        int priority = -1;
        std::string packageName = "";
        auto packageItr = itr->FindMember("priority");
        if (packageItr != itr->MemberEnd())
        {
            priority = packageItr->value.GetInt();
        }
        else continue;
        packageItr = itr->FindMember("name");
        if (packageItr != itr->MemberEnd())
        {
            packageName = packageItr->value.GetString();
        }

        auto packageInListIt = std::find_if(
            _AIPackageList->begin(),
            _AIPackageList->end(),
            [&packageName](GameData* packageData) { return packageName == packageData->name; }
        );
        DebugLog("Package: " + sc::toString(priority) + " " + packageName);
        if (*packageInListIt) addPackage(priority, *packageInListIt);
    }
}

void SquadSettingsInfo::readSquadOptionsFromMember(rapidjson::Value& member)
{
    for (auto itr = member.MemberBegin(); itr != member.MemberEnd(); ++itr)
    {
        std::string key = itr->name.GetString();
        rapidjson::Value& subMember = itr->value;
        if (key == "general")
        {
            for (auto subItr = subMember.MemberBegin(); subItr != subMember.MemberEnd(); ++subItr)
            {
                key = subItr->name.GetString();
                if (key == "startWorkTime")
                {
                    _startWorkTime = subItr->value.GetFloat();
                }
                else if (key == "endWorkTime")
                {
                    _endWorkTime = subItr->value.GetFloat();
                }
                else if (key == "autoMovementSpeed")
                {
                    _autoMovementSpeed = subItr->value.GetBool();
                }
                else if (key == "restUntilHealed")
                {
                    _restUntilHealed = subItr->value.GetBool();
                }
                else if (key == "restThreshold")
                {
                    _restThreshold = subItr->value.GetFloat();
                }
                else if (key == "healedThreshold")
                {
                    _healedThreshold = subItr->value.GetFloat();
                }
                else if (key == "usePaidBeds")
                {
                    _usePaidBeds = subItr->value.GetBool();
                }
                else if (key == "doSleep")
                {
                    _doSleep = subItr->value.GetBool();
                }
            }
        }
        else if (key == "labour")
        {
            for (auto subItr = subMember.MemberBegin(); subItr != subMember.MemberEnd(); ++subItr)
            {
                key = subItr->name.GetString();
                if (key == "labourScience")
                {
                    _labourScience = subItr->value.GetFloat();
                }
                else if (key == "labourLabouring")
                {
                    _labourLabouring = subItr->value.GetFloat();
                }
                else if (key == "labourEngineer")
                {
                    _labourEngineer = subItr->value.GetFloat();
                }
                else if (key == "labourCooking")
                {
                    _labourCooking = subItr->value.GetFloat();
                }
                else if (key == "labourRobotics")
                {
                    _labourRobotics = subItr->value.GetFloat();
                }
                else if (key == "labourMedic")
                {
                    _labourMedic = subItr->value.GetFloat();
                }
                else if (key == "labourWeaponSmith")
                {
                    _labourWeaponSmith = subItr->value.GetFloat();
                }
                else if (key == "labourArmourSmith")
                {
                    _labourArmourSmith = subItr->value.GetFloat();
                }
                else if (key == "labourCrossbowSmith")
                {
                    _labourCrossbowSmith = subItr->value.GetFloat();
                }
                else if (key == "labourAutomaticMachine")
                {
                    _labourAutomaticMachine = subItr->value.GetFloat();
                }
                else if (key == "labourOther")
                {
                    _labourOther = subItr->value.GetFloat();
                }
            }
        }
        else if (key == "combat")
        {
            for (auto subItr = subMember.MemberBegin(); subItr != subMember.MemberEnd(); ++subItr)
            {
                key = subItr->name.GetString();
                if (key == "attackEnemies")
                {
                    _attackEnemies = subItr->value.GetBool();
                }
                else if (key == "ignoreEnemiesInNonHostile")
                {
                    _ignoreEnemiesInNonHostile = subItr->value.GetBool();
                }
                else if (key == "protectAllies")
                {
                    _protectAllies = subItr->value.GetBool();
                }
                else if (key == "doMedic")
                {
                    _doMedic = subItr->value.GetBool();
                }
                else if (key == "doRescue")
                {
                    _doRescue = subItr->value.GetBool();
                }
                else if (key == "rescueOwnSquadOnly")
                {
                    _rescueOwnSquadOnly = subItr->value.GetBool();
                }
            }
        }
        else if (key == "guard")
        {
            for (auto subItr = subMember.MemberBegin(); subItr != subMember.MemberEnd(); ++subItr)
            {
                key = subItr->name.GetString();

                if (key == "manTurrets")
                {
                    _manTurrets = subItr->value.GetBool();
                }
                else if (key == "stayInsideGate")
                {
                    _stayInsideGate = subItr->value.GetBool();
                }
                else if (key == "closeGate")
                {
                    _closeGate = subItr->value.GetBool();
                }
            }
        }
        else if (key == "shopping")
        {
            for (auto subItr = subMember.MemberBegin(); subItr != subMember.MemberEnd(); ++subItr)
            {
                key = subItr->name.GetString();

                if (key == "doShopping")
                {
                    _doShopping = subItr->value.GetBool();
                }
                else if (key == "buyItems")
                {
                    _buyItems = readShoppingItemsFromMember(subItr->value, true);
                }
                else if (key == "sellItems")
                {
                    _sellItems = readShoppingItemsFromMember(subItr->value, false);
                }
            }
        }
        else if (key == "inventory")
        {
            for (auto subItr = subMember.MemberBegin(); subItr != subMember.MemberEnd(); ++subItr)
            {
                key = subItr->name.GetString();

                if (key == "doLooting")
                {
                    _doLooting = subItr->value.GetBool();
                }
                else if (key == "doAnimalLooting")
                {
                    _doAnimalLooting = subItr->value.GetBool();
                }
                else if (key == "putLootsInStorage")
                {
                    _putLootsInStorage = subItr->value.GetBool();
                }
                else if (key == "putResourcesInStorage")
                {
                    _putResourcesInStorage = subItr->value.GetBool();
                }
                else if (key == "ignoreEquipped")
                {
                    _ignoreEquipped = subItr->value.GetBool();
                }
                else if (key == "forbidItems")
                {
                    _forbidItems = readForbidItemsFromMember(subItr->value);
                }
            }
        }
        else if (key == "travel")
        {
            for (auto subItr = subMember.MemberBegin(); subItr != subMember.MemberEnd(); ++subItr)
            {
                key = subItr->name.GetString();

                if (key == "stopAtTownsAlongTheWay")
                {
                    _stopAtTownsAlongTheWay = subItr->value.GetBool();
                }
                else if (key == "targetTownIndex")
                {
                    _targetTownIndex = subItr->value.GetInt();
                }
                else if (key == "targetTowns")
                {
                    _targetTowns = readTargetTownsFromMember(subItr->value);
                }
            }
        }
    }
}

std::unordered_map<ItemFunction, ShoppingItemInfo> SquadSettingsInfo::readShoppingItemsFromMember(rapidjson::Value& member, bool buying)
{
    // array of objs with member itemFunction(int), quantity(int), onlyGoodDeal(bool), shopTowns(array of townbase hand array)
    std::unordered_map<ItemFunction, ShoppingItemInfo> shoppingItems;
    for (auto itr = member.Begin(); itr != member.End(); ++itr)
    {
        ItemFunction itemFunc = ITEM_NO_FUNCTION;
        auto subItr = itr->FindMember("itemFunction");
        if (subItr != itr->MemberEnd())
        {
            itemFunc = static_cast<ItemFunction>(subItr->value.GetInt());
        }
        else continue;
        int quantity = 0;
        SettingsUtils::parseIntSetting(*itr, "quantity", quantity);
        bool onlyGoodDeal = false;
        SettingsUtils::parseBoolSetting(*itr, "onlyGoodDeal", onlyGoodDeal);
        bool onlyStolenItems = false;
        if (!buying)
        {
            SettingsUtils::parseBoolSetting(*itr, "onlyStolenItems", onlyStolenItems);
        }
        subItr = itr->FindMember("shopItemDatas");
        std::unordered_set<GameData*> shopItemDatas;

        SquadAutonomySettings& settings = SquadAutonomySettings::getSingleton();
        auto itemDatas = settings.getItemDatas(itemFunc);
        if (itemDatas.size() > 0)
        {
            if (subItr != itr->MemberEnd())
            {
                for (auto dataItr = subItr->value.Begin(); dataItr != subItr->value.End(); ++dataItr)
                {
                    auto dataSubItr = dataItr->FindMember("itemType");
                    itemType type = NULL_ITEM;
                    std::string name = "";
                    if (dataSubItr != dataItr->MemberEnd())
                    {
                        type = static_cast<itemType>(dataSubItr->value.GetInt());
                    }
                    else continue;

                    dataSubItr = dataItr->FindMember("name");
                    if (dataSubItr != dataItr->MemberEnd())
                    {
                        name = dataSubItr->value.GetString();
                    }
                    for (int i = 0; i < itemDatas.size(); ++i)
                    {
                        GameData* data = itemDatas[i];
                        if (data->type == type && data->name == name)
                        {
                            shopItemDatas.insert(data);
                            break;
                        }
                    }
                }
            }
        }


        bool excludeTowns = false;
        SettingsUtils::parseBoolSetting(*itr, "excludeTowns", excludeTowns);
        subItr = itr->FindMember("shopTowns");
        std::unordered_set<TownBase*> shopTowns;
        if (subItr != itr->MemberEnd())
        {
            // iterating over an array of townbase* hand array
            for (auto shopTownsItr = subItr->value.Begin(); shopTownsItr != subItr->value.End(); ++shopTownsItr)
            {
                auto handItr = shopTownsItr->FindMember("hand");
                if (handItr != shopTownsItr->MemberEnd())
                {
                    hand townHand = parseHandMember(handItr->value);
                    if (townHand)
                    {
                        TownBase* town = townHand.getTown();
                        if (town) shopTowns.insert(town);
                    }
                }
            }
        }
        ShoppingItemInfo shoppingItemInfo(quantity);
        shoppingItemInfo.onlyGoodDeal = onlyGoodDeal;
        shoppingItemInfo.onlyStolenItems = onlyStolenItems;
        shoppingItemInfo.shopItemDatas = shopItemDatas;
        shoppingItemInfo.excludeTowns = excludeTowns;
        shoppingItemInfo.shopTowns = shopTowns;
        shoppingItems[itemFunc] = shoppingItemInfo;
    }
    return shoppingItems;
}

std::unordered_set<ItemFunction> SquadAutonomy::SquadSettingsInfo::readForbidItemsFromMember(rapidjson::Value& member)
{
    std::unordered_set<ItemFunction> forbidItems;

    for (auto itr = member.Begin(); itr != member.End(); ++itr)
    {
        ItemFunction itemFunc = ITEM_NO_FUNCTION;
        auto subItr = itr->FindMember("itemFunction");
        if (subItr != itr->MemberEnd())
        {
            itemFunc = static_cast<ItemFunction>(subItr->value.GetInt());
        }
        else continue;
        forbidItems.insert(itemFunc);
    }

    return forbidItems;
}

std::vector<TownBase*> SquadAutonomy::SquadSettingsInfo::readTargetTownsFromMember(rapidjson::Value& member)
{
    std::vector<TownBase*> targetTowns;

    for (auto itr = member.Begin(); itr != member.End(); ++itr)
    {
        auto subItr = itr->FindMember("hand");
        if (subItr != itr->MemberEnd())
        {
            hand townHand = parseHandMember(subItr->value);
            if (townHand)
            {
                TownBase* town = townHand.getTown();
                if (town) targetTowns.push_back(town);
            }
        }
    }
    return targetTowns;
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

std::unordered_map<ItemFunction, ShoppingItemInfo> SquadSettingsInfo::readShoppingItemsFromStream(std::istream& stream)
{
    // itemFunction(int) quantity(int) onlyGoodDeal:onlyGoodDeal(bool) town:town hand(5 ints with spaces inbetween)...
    std::unordered_map<ItemFunction, ShoppingItemInfo> shoppingItems;
    std::string line;

    while (std::getline(stream, line))
    {
        std::istringstream iss(line);
        int itemFunction = 0;
        int quantity = 0;
        bool onlyGoodDeal = false;
        iss >> itemFunction
            >> quantity;
        ShoppingItemInfo shoppingItem(quantity);
        shoppingItems[static_cast<ItemFunction>(itemFunction)] = shoppingItem;
    }

    return shoppingItems;
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

        if (*packageInListIt) addPackage(priority, *packageInListIt);

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
        if (type == "OverwriteSpeed")
        {
            SettingsUtils::parseBoolSetting(dataLine, _autoMovementSpeed);
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
        if (type == "RescueOwnSquadOnly")
        {
            SettingsUtils::parseBoolSetting(dataLine, _rescueOwnSquadOnly);
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


