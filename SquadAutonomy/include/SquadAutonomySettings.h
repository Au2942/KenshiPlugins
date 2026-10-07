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
        SquadSettingsInfo(Platoon& squad);

        static std::unique_ptr<SquadSettingsInfo> readFromStream(std::istream& stream);
        bool writeToStream(std::ostream& stream) const;

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
        float getLabourOther();
        void setLabourOther(float val);
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
        static hand parseHandSetting(const std::string& dataLine);
        static Building* parseBuildingSetting(const std::string& dataLine);

        void readSquadPackagesFromStream(std::istream& stream);
        void readSquadOptionsFromStream(std::istream& stream);

        static bool writeHandSettingToStream(std::ostream& stream, hand handToWrite, const std::string& name);
        static bool writeBuildingSettingToStream(std::ostream& stream, const Building* building, const std::string& tag);
        void writeSquadPackagesToStream(std::ostream& stream) const;
        void writeSquadOptionsToStream(std::ostream& stream) const;

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
        bool _restUntilHealed;
        float _restThreshold;
        float _healedThreshold;
        bool _usePaidBeds;
        bool _doSleep;
        // Labour
        float _labourScience;
        float _labourLabouring;
        float _labourFarming;
        float _labourEngineer;
        float _labourCooking;
        float _labourRobotics;
        float _labourMedic;
        float _labourWeaponSmith;
        float _labourArmourSmith;
        float _labourCrossbowSmith;
        float _labourAutomaticMachine;
        float _labourOther;
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
        static SquadAutonomySettings& getSingleton();
        static bool initialized;
        std::vector<std::unique_ptr<SquadSettingsInfo>> squadSettings;
        bool saveSettings(const std::wstring&);
        bool loadSettings(const std::wstring&);
        SquadSettingsInfo* getSquadSettings(Platoon*, bool createNew = false);
        std::vector<GameData*>* getAIPackageList();
        std::vector<GameData*>* getSquadTemplate();
        void removeSquadSettings(Platoon*);

    private:
        // Make default constructor private to only allow creation as the singleton.
        SquadAutonomySettings();

        // Disable copy and move operations for the singleton by declaring them private.
        SquadAutonomySettings(const SquadAutonomySettings& other);
        SquadAutonomySettings(SquadAutonomySettings&& other);
        SquadAutonomySettings& operator=(const SquadAutonomySettings& other);
        SquadAutonomySettings& operator=(SquadAutonomySettings&& other);

        std::vector<GameData*> _AIPackageList;
        std::vector<GameData*> _squadTemplateList;
        void _initGameData();
    };


}
