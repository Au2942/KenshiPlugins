#pragma once
#include "SquadAutonomy.h"

#include <rapidjson/document.h>

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
        static std::unique_ptr<SquadSettingsInfo> readFromMember(rapidjson::Value& member);
        bool writeToStream(std::ostream& stream) const;
        rapidjson::Value getInfoMember(rapidjson::Document::AllocatorType& allocator) const;

        Platoon* getSquad();
        PlayerInterface* getPlayerInterface();
        bool isEnabled();
        void addPackage(int priority, GameData* data);
        void setPackages(const std::map<int, std::vector<GameData*>> &packages);
        void clearPackages();
        const std::map<int, std::vector<GameData*>>& getSquadPackages();
        bool hasTask(TaskType);
        bool hasSignal(BlackboardSignalFunctions key);
        bool enableAutonomy(bool enable, bool endAction = true);
        void updateSquadPackages(bool endAction = true);
        Building* getBuilding(bool home);
        void setBuilding(Building* building, bool home);
        bool assignSquadHome(bool home);
        void unassignSquadHome();
        bool hasHome();

        float getStartWorkTime() const;
        void setStartWorkTime(float time);
        float getEndWorkTime() const;
        void setEndWorkTime(float time);
        bool getAutoMovementSpeed() const;
        void setAutoMovementSpeed(bool val);
        bool getDoSleep() const;
        void setDoSleep(bool val);
        bool getRestUntilHealed() const;
        void setRestUntilHealed(bool val);
        float getRestThreshold() const;
        void setRestThreshold(float val);
        float getRestThresholdP() const;
        float getHealedThreshold() const;
        float getHealedThresholdP() const;
        void setHealedThreshold(float val);
        bool getUsePaidBeds() const;
        void setUsePaidBeds(bool val);
        bool isRestTime();
        
        float getLabourScience() const;
        void setLabourScience(float val);
        float getLabourLabouring() const;
        void setLabourLabouring(float val);
        float getLabourFarming() const;
        void setLabourFarming(float val);
        float getLabourEngineer() const;
        void setLabourEngineer(float val);
        float getLabourRobotics() const;
        void setLabourRobotics(float val);
        float getLabourMedic() const;
        void setLabourMedic(float val);
        float getLabourCooking() const;
        void setLabourCooking(float val);
        float getLabourWeaponSmith() const;
        void setLabourWeaponSmith(float val);
        float getLabourArmourSmith() const;
        void setLabourArmourSmith(float val);
        float getLabourCrossbowSmith() const;
        void setLabourCrossbowSmith(float val);
        float getLabourAutomaticMachine() const;
        void setLabourAutomaticMachine(float val);
        float getLabourOther() const;
        void setLabourOther(float val);
        int getLabourPriority(UseableStuff*);

        bool getAttackEnemies() const;
        void setAttackEnemies(bool val);
        bool getProtectAllies() const;
        void setProtectAllies(bool val);
        bool getDoMedic() const;
        void setDoMedic(bool val);
        bool getDoRescue() const;
        void setDoRescue(bool val);
        bool getRescueOwnSquadOnly() const;
        void setRescueOwnSquadOnly(bool val);

        bool getManTurrets() const;
        void setManTurrets(bool val);
        bool getStayInsideGate() const;
        void setStayInsideGate(bool val);
        bool getCloseGate() const;
        void setCloseGate(bool val);

        bool getDoShopping() const;
        void setDoShopping(bool val);
        std::unordered_map<ItemFunction, ShoppingItemInfo> getBuyItems() const;
        std::unordered_map<ItemFunction, ShoppingItemInfo>* getBuyItemsPtr();
        void setBuyItems(std::unordered_map<ItemFunction, ShoppingItemInfo> val);
        void addBuyItem(ItemFunction itemFunction, int quantity);
        std::unordered_map<ItemFunction, ShoppingItemInfo> getSellItems() const;
        std::unordered_map<ItemFunction, ShoppingItemInfo>* getSellItemsPtr();
        void setSellItems(std::unordered_map<ItemFunction, ShoppingItemInfo> val);
        void addSellItem(ItemFunction itemFunction, int quantity);

        bool getDoLooting() const;
        void setDoLooting(bool val);
        bool getPutLootsInStorage() const;
        void setPutLootsInStorage(bool val);
        bool getIgnoreEquipped() const;
        void setIgnoreEquipped(bool val);
        std::unordered_set<ItemFunction> getForbidItems() const;
        bool hasForbidItem(ItemFunction itemFunction);
        void addForbidItem(ItemFunction itemFunction);
        void clearForbidItems();

        void addTargetTown(TownBase* town);
        void clearTargetTown();
        TownBase* getCurrentTargetTown();
        void nextTargetTown();
        int getTargetTownIndex();
        int& getTargetTownIndexRef();
        void setTargetTownIndex(int val);
        int getTargetTownIndex() const;
        std::vector<TownBase*> getTargetTowns() const;
        std::vector<TownBase*>* getTargetTownsPtr();

    private:
        /*
        static bool writeHandSettingToStream(std::ostream& stream, hand handToWrite, const std::string& name);
        static bool writeBuildingSettingToStream(std::ostream& stream, const Building* building, const std::string& tag);
        static void writeTradeItemsToStream(std::ostream& stream, const std::unordered_map<ItemFunction, ShoppingItemInfo>);
        static void writeForbidItemsToStream(std::ostream& stream, const std::unordered_set<ItemFunction> forbidItems);
        void writeSquadPackagesToStream(std::ostream& stream) const;
        void writeSquadOptionsToStream(std::ostream& stream) const;
        */
        static rapidjson::Value getHandMember(rapidjson::Document::AllocatorType& allocator, hand handToWrite, const std::string& name);
        rapidjson::Value getSquadPackagesMember(rapidjson::Document::AllocatorType& allocator) const;
        rapidjson::Value getSquadOptionsMember(rapidjson::Document::AllocatorType& allocator) const;
        static rapidjson::Value getShoppingItemsMember(rapidjson::Document::AllocatorType& allocator, const std::unordered_map<ItemFunction, ShoppingItemInfo>, bool buying);
        static rapidjson::Value getForbidItemsMember(rapidjson::Document::AllocatorType& allocator, const std::unordered_set<ItemFunction> forbidItems);
        static rapidjson::Value getTargetTownsMember(rapidjson::Document::AllocatorType& allocator, const std::vector<TownBase*> targetTowns);

        static hand parseHandMember(rapidjson::Value& member);
        static Building* parseBuildingMember(rapidjson::Value& member);
        void readSquadPackagesFromMember(rapidjson::Value& member);
        void readSquadOptionsFromMember(rapidjson::Value& member);
        static std::unordered_map<ItemFunction, ShoppingItemInfo> readShoppingItemsFromMember(rapidjson::Value& member, bool buying);
        static std::unordered_set<ItemFunction> readForbidItemsFromMember(rapidjson::Value& member);
        static std::vector<TownBase*> readTargetTownsFromMember(rapidjson::Value& member);

        static hand parseHandSetting(const std::string& dataLine);
        static Building* parseBuildingSetting(const std::string& dataLine);
        static std::unordered_map <ItemFunction, ShoppingItemInfo> readShoppingItemsFromStream(std::istream& stream);
        void readSquadPackagesFromStream(std::istream& stream);
        void readSquadOptionsFromStream(std::istream& stream);

        void addTasksFromPackage(GameData* data);
        void addTasksFromGoals(GameData* data, lektor<GameData*>& goalDatas);
        void addSignalFromPackage(GameData* data);

        Platoon* _squad;
        PlayerInterface* _pi;
        bool _enabled;
        std::map<int, std::vector<GameData*>> _squadPackages;
        std::unordered_set<TaskType> _squadTasks;
        std::unordered_set<BlackboardSignalFunctions> _squadSignals;
        //TownBase* _homeTown;
        Building* _homeBuilding;
        //TownBase* _workTown;
        Building* _workBuilding;
        // Basic
        float _startWorkTime;
        float _endWorkTime;
        bool _autoMovementSpeed;
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
        bool _rescueOwnSquadOnly;
        // Guard
        bool _manTurrets;
        bool _stayInsideGate;
        bool _closeGate;
        // Shopping
        bool _doShopping;
        std::unordered_map<ItemFunction, ShoppingItemInfo> _buyItems;
        std::unordered_map<ItemFunction, ShoppingItemInfo> _sellItems;
        // Inventory
        bool _doLooting;
        bool _putLootsInStorage;
        bool _ignoreEquipped;
        std::unordered_set<ItemFunction> _forbidItems;
        // Travel
        int _targetTownIndex;
        std::vector<TownBase*> _targetTowns;
    };

    class SquadAutonomySettings
    {
    public:
        static SquadAutonomySettings& getSingleton();
        static bool initialized;
        std::vector<std::unique_ptr<SquadSettingsInfo>> squadSettings;
        void initSaveDocument();
        bool saveSettings(const std::wstring&);
        bool loadSettings(const std::wstring&);
        SquadSettingsInfo* getSquadSettings(Platoon*, bool createNew = false);
        std::vector<GameData*>* getAIPackageList();
        std::vector<GameData*>* getSquadTemplate();
        std::unordered_map<ItemFunction, std::vector<GameData*>>* getItemDatasList();
        std::vector<GameData*>* getItemDatas(ItemFunction itemFunc);
        std::vector<TownBase*>* getTownBaseList();
        void removeSquadSettings(Platoon*);

    private:
        // Make default constructor private to only allow creation as the singleton.
        SquadAutonomySettings();

        // Disable copy and move operations for the singleton by declaring them private.
        SquadAutonomySettings(const SquadAutonomySettings& other);
        SquadAutonomySettings(SquadAutonomySettings&& other);
        SquadAutonomySettings& operator=(const SquadAutonomySettings& other);
        SquadAutonomySettings& operator=(SquadAutonomySettings&& other);

        void _populateItemDatasByFunctionList(lektor<GameData*> datas, ItemFunction itemFunc);
        void _addItemToItemDatasByFunctionList(GameData* data, ItemFunction itemFunc);

        rapidjson::Document _saveDocument;
        std::wstring _saveFileName;

        std::vector<GameData*> _AIPackageList;
        std::vector<GameData*> _squadTemplateList;
        std::vector<TownBase*> _townBaseList;
        std::unordered_map<ItemFunction, std::vector<GameData*>> _itemDatasByFunctionList;
        void _init();
        void _initGameData();
    };


}
