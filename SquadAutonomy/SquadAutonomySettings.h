#pragma once
#include <kenshi/GameData.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Platoon.h>


namespace SquadAutonomy
{
    class SquadSettingsInfo
    {
    public:
        SquadSettingsInfo(Platoon* squad);
        Platoon* getSquad();
        PlayerInterface* getPlayerInterface();
        bool isEnabled();
        void addPackage(int priority, GameData* data);
        void setPackages(const std::map<int, lektor<GameData*> >& packages);
        void clearPackages();
        std::map<int, lektor<GameData*>> getSquadPackages();
        bool enableAutonomy(bool enable, bool endAction = true);
        void updateSquadPackages();
        Building* getBuilding(bool home);
        void setBuilding(Building* building, bool home);
        bool assignSquadHome(bool home);
        void unassignSquadHome();
        bool hasHome();
        bool getManTurrets();
        void setManTurrets(bool val);
        bool getDoSleep();
        bool getRestUntilHealed();
        bool getUsePaidBeds();
        float getStartWorkTime();
        float getEndWorkTime();
        void setStartWorkTime(float time);
        void setEndWorkTime(float time);
        void setDoSleep(bool val);
        void setRestUntilHealed(bool val);
        void setUsePaidBeds(bool val);
        bool isRestTime();

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
        float _endWorkTime;
        float _startWorkTime;
        bool _restUntilHealed;
        bool _usePaidBeds;
    };

    class SquadAutonomySettings
    {
    public:
        static SquadAutonomySettings* getSingletonPtr();
        static bool initialized;
        lektor<SquadSettingsInfo*> squadSettings;
        SquadAutonomySettings();
        bool saveSettings(std::string);
        bool loadSettings(std::string);
        hand* createHandfromLine(std::string);
        SquadSettingsInfo* getSquadSettings(Platoon*, bool createNew = false);
        lektor<GameData*>* getAIPackageList();
        lektor<GameData*>* getSquadTemplate();
        void removeSquadSettings(Platoon*);
        std::string getConfigPath();

    private:
        lektor<GameData*> _AIPackageList;
        lektor<GameData*> _squadTemplateList;
        lektor<std::string> _cfgPackageList;
        std::string _cfgFileName;
        std::string _cfgPath;
        void _loadConfig();
        void _initGameData();
    };


}
