#pragma once
#include <kenshi/GameData.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Faction.h>
#include <kenshi/Platoon.h>


namespace SquadAutonomy
{
    class SquadSettingsInfo
    {
    public:
        SquadSettingsInfo(Platoon* squad) : _enabled(false), _squad(squad), _pi(nullptr), _homeBuilding(nullptr), _workBuilding(nullptr),
            _startWorkTime(0.0), _endWorkTime(24.0), _doSleep(false), _usePaidBeds(false), _restUntilHealed(true), _restThreshold(50.0), _healedThreshold(90.0),
            _science(true), 
            _labouring(true), _farming(true), 
            _medic(true), _cooking(true), _engineer(true), _robotics(true), _weaponSmith(true), _armourSmith(true), _crossbowSmith(true), _automaticMachine(true),
            _attackEnemies(true), _protectAllies(true), _doMedic(true), _doRescue(true),
            _manTurrets(false), _stayInsideGate(false), _closeGate(false)
        {
            Faction* faction = squad->getFaction();
            if (faction)
            {
                _pi = faction->isPlayer;
            }
        }
        Platoon* getSquad();
        PlayerInterface* getPlayerInterface();
        bool isEnabled();
        void addPackage(int priority, GameData* data);
        void setPackages(const std::map<int, lektor<GameData*> >& packages);
        void clearPackages();
        std::map<int, lektor<GameData*>> getSquadPackages();
        bool hasTask(TaskType);
        bool enableAutonomy(bool enable, bool endAction = true);
        void updateSquadPackages(bool endAction = true);
        Building* getBuilding(bool home);
        void setBuilding(Building* building, bool home);
        bool assignSquadHome(bool home);
        void unassignSquadHome();
        bool hasHome();

        float getStartWorkTime();
        float getEndWorkTime();
        bool getDoSleep();
        bool getRestUntilHealed();
        float getRestThreshold();
        float getHealedThreshold();
        float getRestThresholdP();
        float getHealedThresholdP();
        bool getUsePaidBeds();
        void setStartWorkTime(float time);
        void setEndWorkTime(float time);
        void setDoSleep(bool val);
        void setRestUntilHealed(bool val);
        void setRestThreshold(float val);
        void setHealedThreshold(float val);
        void setUsePaidBeds(bool val);
        bool isRestTime();
        
        bool getLabourScience();
        void setLabourScience(bool val);
        bool getLabourLabouring();
        void setLabourLabouring(bool val);
        bool getLabourFarming();
        void setLabourFarming(bool val);
        bool getLabourEngineer();
        void setLabourEngineer(bool val);
        bool getLabourRobotics();
        void setLabourRobotics(bool val);
        bool getLabourMedic();
        void setLabourMedic(bool val);
        bool getLabourCooking();
        void setLabourCooking(bool val);
        bool getLabourWeaponSmith();
        void setLabourWeaponSmith(bool val);
        bool getLabourArmourSmith();
        void setLabourArmourSmith(bool val);
        bool getLabourCrossbowSmith();
        void setLabourCrossbowSmith(bool val);
        bool getLabourAutomaticMachine();
        void setLabourAutomaticMachine(bool val);
        bool CanDoLabour(UseableStuff*);

        bool getAttackEnemies();
        void setAttackEnemies(bool val);
        bool getProtectAllies();
        void setProtectAllies(bool val);
        bool getDoMedic();
        void setDoMedic(bool val);
        bool getDoRescue();
        void setDoRescue(bool val);

        bool getManTurrets();
        void setManTurrets(bool val);
        bool getStayInsideGate();
        void setStayInsideGate(bool val);
        bool getCloseGate();
        void setCloseGate(bool val);
    private:
        Platoon* _squad;
        PlayerInterface* _pi;
        bool _enabled;
        std::map<int, lektor<GameData*>> _squadPackages;
        //TownBase* _homeTown;
        Building* _homeBuilding;
        //TownBase* _workTown;
        Building* _workBuilding;
        // Basic
        float _startWorkTime;
        float _endWorkTime;
        bool _doSleep;
        bool _restUntilHealed;
        float _restThreshold;
        float _healedThreshold;
        bool _usePaidBeds;
        // Labour
        bool _science;
        bool _labouring;
        bool _farming;
        bool _engineer;
        bool _cooking;
        bool _robotics;
        bool _medic;
        bool _weaponSmith;
        bool _armourSmith;
        bool _crossbowSmith;
        bool _automaticMachine;
        // Combat
        bool _attackEnemies;
        bool _protectAllies;
        bool _doMedic;
        bool _doRescue;
        // Guard
        bool _manTurrets;
        bool _stayInsideGate;
        bool _closeGate;
    };

    class SquadAutonomySettings
    {
    public:
        static SquadAutonomySettings* getSingletonPtr();
        static bool initialized;
        lektor<SquadSettingsInfo*> squadSettings;
        SquadAutonomySettings();
        bool saveSettings(std::wstring);
        bool loadSettings(std::wstring);
        hand* createHandfromLine(std::string);
        SquadSettingsInfo* getSquadSettings(Platoon*, bool createNew = false);
        lektor<GameData*>* getAIPackageList();
        lektor<GameData*>* getSquadTemplate();
        void removeSquadSettings(Platoon*);
        std::wstring getConfigPath();

    private:
        lektor<GameData*> _AIPackageList;
        lektor<GameData*> _squadTemplateList;
        lektor<std::string> _cfgPackageList;
        std::wstring _cfgFileName;
        std::wstring _cfgPath;
        void _loadConfig();
        void _initGameData();
    };


}
