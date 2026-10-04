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
            _science(1.0), 
            _labouring(1.0), _farming(1.0), 
            _medic(1.0), _cooking(1.0), _engineer(1.0), _robotics(1.0), _weaponSmith(1.0), _armourSmith(1.0), _crossbowSmith(1.0), _automaticMachine(1.0),
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
        void setPackages(const std::map<int, std::vector<GameData*>> &packages);
        void clearPackages();
        const std::map<int, std::vector<GameData*>>& getSquadPackages();
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
        
        float getLabourScience();
        void setLabourScience(float val);
        float getLabourLabouring();
        void setLabourLabouring(float val);
        float getLabourFarming();
        void setLabourFarming(float val);
        float getLabourEngineer();
        void setLabourEngineer(float val);
        float getLabourRobotics();
        void setLabourRobotics(float val);
        float getLabourMedic();
        void setLabourMedic(float val);
        float getLabourCooking();
        void setLabourCooking(float val);
        float getLabourWeaponSmith();
        void setLabourWeaponSmith(float val);
        float getLabourArmourSmith();
        void setLabourArmourSmith(float val);
        float getLabourCrossbowSmith();
        void setLabourCrossbowSmith(float val);
        float getLabourAutomaticMachine();
        void setLabourAutomaticMachine(float val);
        int getLabourPriority(UseableStuff*);

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
        std::map<int, std::vector<GameData*>> _squadPackages;
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
        float _science;
        float _labouring;
        float _farming;
        float _engineer;
        float _cooking;
        float _robotics;
        float _medic;
        float _weaponSmith;
        float _armourSmith;
        float _crossbowSmith;
        float _automaticMachine;
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
        std::vector<std::unique_ptr<SquadSettingsInfo>> squadSettings;
        SquadAutonomySettings();
        bool saveSettings(std::wstring);
        bool loadSettings(std::wstring);
        hand* createHandfromLine(std::string);
        SquadSettingsInfo* getSquadSettings(Platoon*, bool createNew = false);
        std::vector<GameData*>* getAIPackageList();
        std::vector<GameData*>* getSquadTemplate();
        void removeSquadSettings(Platoon*);

    private:
        std::vector<GameData*> _AIPackageList;
        std::vector<GameData*> _squadTemplateList;
        void _initGameData();
    };


}
