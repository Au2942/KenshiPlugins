#include "SquadAutonomy.h"
#include "SquadAutonomySettings.h"
#include "SquadAutonomyModSettings.h"

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


SquadAutonomySettings* SquadAutonomySettings::getSingletonPtr()
{
    static std::unique_ptr<SquadAutonomySettings> singleton;
    if (!singleton)
    {
        singleton.reset(new SquadAutonomySettings());
    }

    return singleton.get();
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

bool SquadAutonomySettings::saveSettings(std::wstring savePath)
{
    std::ofstream saveFile(savePath);
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

        saveFile << std::boolalpha; // output bools as text

        for (int i = 0; i < settings->squadSettings.size(); ++i)
        {
            SquadSettingsInfo* settingsInfo = settings->squadSettings[i].get();
            Platoon* squad = settingsInfo->getSquad();
            if (!squad) continue;
            if (!squad->activePlatoon) continue;
            //DebugLog("Saving squad " + Ogre::StringConverter::toString(i));
            hand platoonHand = squad->getHandle();
            if (platoonHand)
            {
                saveFile << "Squad: <" + squad->activePlatoon->getName() + "> ";
                saveFile << platoonHand.index << ' ';
                saveFile << platoonHand.serial << ' ';
                saveFile << platoonHand.type << ' ';
                saveFile << platoonHand.container << ' ';
                saveFile << platoonHand.containerSerial << '\n';
                saveFile << "\tEnable: " << settingsInfo->isEnabled() << '\n';
            }
            Building* home = settingsInfo->getBuilding(true);
            saveFile << "\tHomeBuilding:";
            if (home)
            {
                hand homeHand = home->getHandle();
                saveFile << " <" << home->displayName << "> ";
                saveFile << homeHand.index << ' ';
                saveFile << homeHand.serial << ' ';
                saveFile << homeHand.type << ' ';
                saveFile << homeHand.container << ' ';
                saveFile << homeHand.containerSerial;
            }
            saveFile << '\n';
            Building* work = settingsInfo->getBuilding(false);
            saveFile << "\tWorkBuilding:";
            if (work)
            {
                hand workHand = work->getHandle();
                saveFile << " <" << work->displayName << "> ";
                saveFile << workHand.index << ' ';
                saveFile << workHand.serial << ' ';
                saveFile << workHand.type << ' ';
                saveFile << workHand.container << ' ';
                saveFile << workHand.containerSerial;
            }
            saveFile << '\n';
            auto packages = settingsInfo->getSquadPackages();
            saveFile << "\tPackages:\n";
            if (packages.size() > 0)
            {
                for (auto it = packages.begin(); it != packages.end(); ++it)
                {
                    auto data = it->second;

                    for (int j = 0; j < data.size(); ++j)
                    {
                        saveFile << "\t\t" << it->first << ':';
                        saveFile << " <" + data[j]->name + '>';
                        saveFile << '\n';
                    }
                }
            }
            saveFile << "\tEndPackages:" << '\n';
            saveFile << "\tOptions:" << '\n';
            saveFile << "\t\tStartWorkTime: " << settingsInfo->getStartWorkTime() << '\n';
            saveFile << "\t\tEndWorkTime: " << settingsInfo->getEndWorkTime() << '\n';
            saveFile << "\t\tRestUntilHealed: " << settingsInfo->getRestUntilHealed() << '\n';
            saveFile << "\t\t\tRestThreshold: " << settingsInfo->getRestThreshold() << '\n';
            saveFile << "\t\t\tHealedThreshold: " << settingsInfo->getHealedThreshold() << '\n';
            saveFile << "\t\tUsePaidBeds: " << settingsInfo->getUsePaidBeds() << '\n';
            saveFile << "\t\tDoSleep: " << settingsInfo->getDoSleep() << '\n';

            saveFile << "\t\tLabourScience: " << settingsInfo->getLabourScience() << '\n';
            saveFile << "\t\tLabourLabouring: " << settingsInfo->getLabourLabouring() << '\n';
            saveFile << "\t\tLabourFarming: " << settingsInfo->getLabourFarming() << '\n';
            saveFile << "\t\tLabourEngineer: " << settingsInfo->getLabourEngineer() << '\n';
            saveFile << "\t\tLabourCooking: " << settingsInfo->getLabourCooking() << '\n';
            saveFile << "\t\tLabourRobotics: " << settingsInfo->getLabourRobotics() << '\n';
            saveFile << "\t\tLabourMedic: " << settingsInfo->getLabourMedic() << '\n';
            saveFile << "\t\tLabourWeaponSmith: " << settingsInfo->getLabourWeaponSmith() << '\n';
            saveFile << "\t\tLabourArmourSmith: " << settingsInfo->getLabourArmourSmith() << '\n';
            saveFile << "\t\tLabourCrossbowSmith: " << settingsInfo->getLabourCrossbowSmith() << '\n';
            saveFile << "\t\tLabourAutomaticMachine: " << settingsInfo->getLabourAutomaticMachine() << '\n';
            saveFile << "\t\tLabourOther: " << settingsInfo->getLabourOther() << '\n';

            saveFile << "\t\tAttackEnemies: " << settingsInfo->getAttackEnemies() << '\n';
            saveFile << "\t\tProtectAllies: " << settingsInfo->getProtectAllies() << '\n';
            saveFile << "\t\tDoMedic: " << settingsInfo->getDoMedic() << '\n';
            saveFile << "\t\tDoRescue: " << settingsInfo->getDoRescue() << '\n';

            saveFile << "\t\tManTurrets: " << settingsInfo->getManTurrets() << '\n';
            saveFile << "\t\tStayInsideGate: " << settingsInfo->getStayInsideGate() << '\n';
            saveFile << "\t\tCloseGate: " << settingsInfo->getCloseGate() << '\n';
            saveFile << "\tEndOptions:" << '\n';
            saveFile << "EndSquad:" << '\n';
        }
        return true;
    }
}

bool SquadAutonomySettings::loadSettings(std::wstring savePath)
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
            std::map<int, std::vector<GameData*>> packages;
            float startWorkTime = 0.0;
            float endWorkTime = 24.0;
            bool doSleep = false;
            bool restUntilHealed = true;
            float restThreshold = 50.0;
            float healedThreshold = 90.0;
            bool usePaidBeds = false;
            // Labour
            float science = 1.0;
            float labouring = 1.0;
            float farming = 1.0;
            float engineer = 1.0;
            float cooking = 1.0;
            float robotics = 1.0;
            float medic = 1.0;
            float weaponSmith = 1.0;
            float armourSmith = 1.0;
            float crossbowSmith = 1.0;
            float automaticMachine = 1.0;
            float other = 1.0;
            // Combat
            bool attackEnemies = true;
            bool protectAllies = true;
            bool doMedic = true;
            bool doRescue = true;
            // Guard
            bool manTurrets = false;
            bool stayInsideGate = false;
            bool closeGate = false;
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

                        if (start != std::string::npos && end != std::string::npos && end > start)
                        {
                            std::string packageName = line.substr(start + 1, end - start - 1);
                            for (int i = 0; i < _AIPackageList.size(); ++i)
                            {
                                if (packageName == _AIPackageList[i]->name)
                                {
                                    const bool alreadyHasPackage = std::find(packages[priority].begin(), packages[priority].end(), _AIPackageList[i]) != packages[priority].end();
                                    if (!alreadyHasPackage)
                                    {
                                        packages[priority].push_back(_AIPackageList[i]);
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
                        if (type == "StartWorkTime")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> startWorkTime;
                            continue;
                        }
                        if (type == "EndWorkTime")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> endWorkTime;
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
                        if (type == "RestThreshold")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> restThreshold;
                            continue;
                        }
                        if (type == "HealedThreshold")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> healedThreshold;
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
                        if (type == "DoSleep")
                        {
                            if (dataLine == "true")
                            {
                                doSleep = true;
                            }
                            continue;
                        }
                        if (type == "LabourScience")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> science;
                            continue;
                        }
                        if (type == "LabourLabouring")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> labouring;
                            continue;
                        }
                        if (type == "LabourFarming")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> farming;
                            continue;
                        }
                        if (type == "LabourEngineer")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> engineer;
                            continue;
                        }
                        if (type == "LabourCooking")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> cooking;
                            continue;
                        }
                        if (type == "LabourRobotics")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> robotics;
                            continue;
                        }
                        if (type == "LabourMedic")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> medic;
                            continue;
                        }
                        if (type == "LabourWeaponSmith")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> weaponSmith;
                            continue;
                        }
                        if (type == "LabourArmourSmith")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> armourSmith;
                            continue;
                        }
                        if (type == "LabourCrossbowSmith")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> crossbowSmith;
                            continue;
                        }
                        if (type == "LabourAutomaticMachine")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> automaticMachine;
                            continue;
                        }
                        if (type == "LabourOther")
                        {
                            if (dataLine == "") continue;
                            std::stringstream ss(dataLine);
                            ss >> other;
                            continue;
                        }
                        if (type == "AttackEnemies")
                        {
                            if (dataLine == "false")
                            {
                                attackEnemies = false;
                            }
                            continue;
                        }
                        if (type == "ProtectAllies")
                        {
                            if (dataLine == "false")
                            {
                                protectAllies = false;
                            }
                            continue;
                        }
                        if (type == "DoMedic")
                        {
                            if (dataLine == "false")
                            {
                                doMedic = false;
                            }
                            continue;
                        }
                        if (type == "DoRescue")
                        {
                            if (dataLine == "false")
                            {
                                doRescue = false;
                            }
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
                        if (type == "StayInsideGate")
                        {
                            if (dataLine == "true")
                            {
                                stayInsideGate = true;
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
                std::unique_ptr<SquadSettingsInfo> settingsInfo(new SquadSettingsInfo(squad));
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
                settingsInfo->setStartWorkTime(startWorkTime);
                settingsInfo->setEndWorkTime(endWorkTime);
                settingsInfo->setDoSleep(doSleep);
                settingsInfo->setRestUntilHealed(restUntilHealed);
                settingsInfo->setRestThreshold(restThreshold);
                settingsInfo->setHealedThreshold(healedThreshold);
                settingsInfo->setUsePaidBeds(usePaidBeds);

                settingsInfo->setLabourScience(science);
                settingsInfo->setLabourLabouring(labouring);
                settingsInfo->setLabourFarming(farming);
                settingsInfo->setLabourEngineer(engineer);
                settingsInfo->setLabourCooking(cooking);
                settingsInfo->setLabourRobotics(robotics);
                settingsInfo->setLabourMedic(medic);
                settingsInfo->setLabourWeaponSmith(weaponSmith);
                settingsInfo->setLabourArmourSmith(armourSmith);
                settingsInfo->setLabourCrossbowSmith(crossbowSmith);
                settingsInfo->setLabourAutomaticMachine(automaticMachine);
                settingsInfo->setLabourOther(other);

                settingsInfo->setAttackEnemies(attackEnemies);
                settingsInfo->setProtectAllies(protectAllies);
                settingsInfo->setDoMedic(doMedic);
                settingsInfo->setDoRescue(doRescue);

                settingsInfo->setManTurrets(manTurrets);
                settingsInfo->setStayInsideGate(stayInsideGate);
                settingsInfo->setCloseGate(closeGate);
                settingsInfo->enableAutonomy(enable, false);
                squadSettings.push_back(std::move(settingsInfo));
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
            return squadSettings[i].get();
        }
    }
    if (createNew)
    {
        std::unique_ptr<SquadSettingsInfo> newSettings(new SquadSettingsInfo(squad));
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