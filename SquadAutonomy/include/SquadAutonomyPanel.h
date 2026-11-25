#pragma once
#include "SquadAutonomySettings.h"
#include <kenshi/Platoon.h>
#include <kenshi/gui/DataPanelLine.h>

namespace SquadAutonomy
{
    class SquadAutonomyPanel
    {
    public:
        static SquadAutonomyPanel& getSingleton();

        SquadAutonomyPanel();
        ~SquadAutonomyPanel();

        void create();
        
        void refresh();
        void onTabChanged(DatapanelGUI* panel, int category);
        void onPanelClosed(MyGUI::Widget* sender, const std::string& name);

        void show();
        void hide();
        bool isVisible();

        void selectSquad(Platoon& selectedSquad);

    private:
        class PopupPanel;
        class MainTab;
        class OptionsTab;

        void createModSettingsButton();

        Platoon* _selectedSquad;
        DatapanelGUI* _panel;
        std::unique_ptr<PopupPanel> _popupPanel;

        std::unique_ptr<MainTab> _mainTab;
        std::unique_ptr<OptionsTab> _optionsTab;
    };

    class SquadAutonomyPanel::PopupPanel
    {
    public:
        PopupPanel();
        void create();
        void refresh();
        void show(bool showBottomPanel = true);
        void hide();
        bool isVisible();
        DatapanelGUI* getPanel() const;
        template<typename T>
        void setupBottomButton(std::string caption, T* object, void (T::* callback)(MyGUI::Widget*));
        template<typename T>
        void onBottomButtonClick(T* object, void (T::* callback)(MyGUI::Widget*));
        void clearBottomButtonClick();
        void setBottomButtonCaption(std::string caption);
    private:
        DatapanelGUI* _panel;
        MyGUI::Widget* _bottomPanel;
        MyGUI::Button* _bottomButton;
    };

    class SquadAutonomyPanel::MainTab
    {
    public:
        MainTab(DatapanelGUI& panel, int tabId);

        void refresh();

        int getCategory() const;

        void setSelectedSquad(Platoon& selectedSquad);

    private:
        void toggleAI(DataPanelLine* line); //button for enabling/setting packages to a squad
        void addAI(DataPanelLine* line); //set the packages in the setting
        void clearAI(DataPanelLine* line); //clear all packages in the setting
        void setHome(DataPanelLine* line);
        void setWork(DataPanelLine* line);
        void setBar(DataPanelLine* line);
        void clearBuildings(DataPanelLine* line);
        void changeAIPackageSearchText(DataPanelLine* line);

        void setBuilding(bool home);
        void updateAIPackageList(const std::string& keyword = std::string());

        static std::string getBuildingName(const Building& building);

        DatapanelGUI* _panel;
        Platoon* _selectedSquad;
        int _category;

        float _priority;
        int _selectedAIPackageIndex;
    };

    class SquadAutonomyPanel::OptionsTab
    {
    public:
        OptionsTab(DatapanelGUI& panel, PopupPanel& popupPanel, int category);

        void refresh();

        int getCategory() const;
        MyGUI::Widget* getFloatingPanelPtr();
        void setFloatingPanelPtr(MyGUI::Widget* ptr);
        void setSelectedSquad(Platoon& selectedSquad);
        void updateFloatingPanel(int category);

        void updateTownList(DatapanelGUI& panel, const std::string& keyword = "", const std::string& dropBoxKey = "", int category = 0, int index = 0);
        void updateItemDataByFunctionList(DatapanelGUI& panel, const std::string& keyword = "", const std::string& dropBoxKey = "", int category = 0, int index = 0, ItemFunction itemFunc = ITEM_NO_FUNCTION);

    private:
        class OptionsBase;
        class BasicOptions;
        class LabourOptions;
        class CombatOptions;
        class GuardOptions;
        class ShoppingOptions;
        class InventoryOptions;
        class TravelOptions;

        // Disable copy and move operations by declaring them private.
        // Reason: sub-options (_basic etc) store `this` pointer, executing copy/move will leave them with the wrong pointer.
        // If copy/move restriction needs to be lifted, implement updating this pointer in sub-options, or remove it from their fields.
        OptionsTab(const OptionsTab& other);
        OptionsTab(OptionsTab&& other);
        OptionsTab& operator=(const OptionsTab& other);
        OptionsTab& operator=(OptionsTab&& other);

        void refreshCallback(MyGUI::ComboBox*, size_t);
        void writeToSelectedSquadSettings(DataPanelLine*);
        void writeToSelectedSquadSettingsAndRefresh(DataPanelLine*);

        void readFromSquadSettings(const SquadSettingsInfo& squadSettings);

        DatapanelGUI* _panel;
        PopupPanel* _popupPanel;
        MyGUI::Widget* _floatingPanel;
        Platoon* _selectedSquad;
        int _category;
        int _subCategory;

        std::unique_ptr<BasicOptions> _basic;
        std::unique_ptr<LabourOptions> _labour;
        std::unique_ptr<CombatOptions> _combat;
        std::unique_ptr<GuardOptions> _guard;
        std::unique_ptr<ShoppingOptions> _shopping;
        std::unique_ptr<InventoryOptions> _inventory;
        std::unique_ptr<TravelOptions> _travel;

    };

    class SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        OptionsBase(OptionsTab& optionsTab, DatapanelGUI& panel, PopupPanel& popupPanel, int category);

    protected:
        OptionsTab* _optionsTab;
        DatapanelGUI* _panel;
        PopupPanel* _popupPanel;
        int _category;
    };

    class SquadAutonomyPanel::OptionsTab::BasicOptions : public SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        BasicOptions(OptionsTab& optionsTab, DatapanelGUI& panel, PopupPanel& popupPanel, int category);

        bool refresh(SquadSettingsInfo& settings);

        void readFromSquadSettings(const SquadSettingsInfo& squadSettings);
        void writeToSquadSettings(SquadSettingsInfo& squadSettings) const;

    private:
        float startWorkTime;
        float endWorkTime;
        bool restUntilHealed;
        float restThreshold;
        float healedThreshold;
        bool usePaidBeds;
        bool doSleep;
        bool autoMovementSpeed;
    };

    class SquadAutonomyPanel::OptionsTab::LabourOptions : public SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        LabourOptions(OptionsTab& optionsTab, DatapanelGUI& panel, PopupPanel& popupPanel, int category);

        bool refresh(SquadSettingsInfo& settings);

        void setAllTo(MyGUI::Widget*);

        void readFromSquadSettings(const SquadSettingsInfo& squadSettings);
        void writeToSquadSettings(SquadSettingsInfo& squadSettings) const;

    private:
        float setAll;
        float science;
        float labouring;
        float farming;
        float cooking;
        float medic;
        float engineer;
        float robotics;
        float weaponSmith;
        float armourSmith;
        float crossbowSmith;
        float automaticMachine;
        float other;
    };

    class SquadAutonomyPanel::OptionsTab::CombatOptions : public SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        CombatOptions(OptionsTab& optionsTab, DatapanelGUI& panel, PopupPanel& popupPanel, int category);

        bool refresh(SquadSettingsInfo& settings);

        void readFromSquadSettings(const SquadSettingsInfo& squadSettings);
        void writeToSquadSettings(SquadSettingsInfo& squadSettings) const;

    private:
        bool attackEnemies;
        bool ignoreEnemiesInNonHostile;
        bool protectAllies;
        bool doMedic;
        bool doRescue;
        bool rescueOwnSquadOnly;
    };

    class SquadAutonomyPanel::OptionsTab::GuardOptions : public SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        GuardOptions(OptionsTab& optionsTab, DatapanelGUI& panel, PopupPanel& popupPanel, int category);

        bool refresh(SquadSettingsInfo& settings);

        void readFromSquadSettings(const SquadSettingsInfo& squadSettings);
        void writeToSquadSettings(SquadSettingsInfo& squadSettings) const;

    private:
        bool manTurrets;
        bool stayInsideGate;
        bool closeGate;
    };

    class SquadAutonomyPanel::OptionsTab::ShoppingOptions : public SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        ShoppingOptions(OptionsTab& optionsTab, DatapanelGUI& panel, PopupPanel& popupPanel, int category);
        bool refresh(SquadSettingsInfo& settings);
        //void updateItemFunctionList(DataPanelLine_DropBox* dropbox);
        void addBuyItem(DataPanelLine* line);
        void addSellItem(DataPanelLine* line);
        void clearBuyItem(DataPanelLine* line);
        void clearSellItem(DataPanelLine* line);

        void updateSelectedItemInfo(MyGUI::Widget* sender);

        void openBuyItemDetail(MyGUI::Widget* sender);
        void openSellItemDetail(MyGUI::Widget* sender);
        void setupTradeItemDetail(ItemFunction func);
        void refreshShopItemDetail(DataPanelLine* line);

        void changeItemSearchText(DataPanelLine* line);
        void addItem(DataPanelLine* line);
        void clearItems(DataPanelLine* line);
        void changeTownSearchText(DataPanelLine* line);
        void addTown(DataPanelLine* line);
        void clearTowns(DataPanelLine* line);

        void readFromSquadSettings(const SquadSettingsInfo& squadSettings);
        void writeToSquadSettings(SquadSettingsInfo& squadSettings) const;
    private:
        bool doShopping;
        int itemFunctionListIndex;
        float quantity;
        bool isBuyItem;
        std::pair<const ItemFunction, ShoppingItemInfo>* selectedItem;
        std::pair<const ItemFunction, ShoppingItemInfo>* checkItem;
        ItemFunction itemItemFunction;
        float itemQuantity;
        bool itemOnlyGoodDeal;
        bool itemOnlyStolenItems;
        int itemItemDataListIndex;
        std::unordered_set<GameData*> itemItemDatas;
        bool itemExcludeTowns;
        int townListIndex;
        std::unordered_set<TownBase*> itemShopTowns;
    };

    class SquadAutonomyPanel::OptionsTab::InventoryOptions : public SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        InventoryOptions(OptionsTab& optionsTab, DatapanelGUI& panel, PopupPanel& popupPanel, int category);

        bool refresh(SquadSettingsInfo& settings);
        void addForbidItem(DataPanelLine* line);
        void clearForbidItems(DataPanelLine* line);
        void readFromSquadSettings(const SquadSettingsInfo& squadSettings);
        void writeToSquadSettings(SquadSettingsInfo& squadSettings) const;

    private:
        bool doLooting;
        bool doAnimalLooting;
        bool putLootsInStorage;
        bool putResourcesInStorage;
        bool ignoreEquipped;
        int itemFunctionListIndex;
    };

    class SquadAutonomyPanel::OptionsTab::TravelOptions : public SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        TravelOptions(OptionsTab& optionsTab, DatapanelGUI& panel, PopupPanel& popupPanel, int category);

        bool refresh(SquadSettingsInfo& settings);
        void readFromSquadSettings(const SquadSettingsInfo& squadSettings);
        void writeToSquadSettings(SquadSettingsInfo& squadSettings) const;
        void changeCurrentTownSlider(DataPanelLine* line);

        void changeTownSearchText(DataPanelLine* line);
        void addTown(DataPanelLine* line);
        void clearTowns(DataPanelLine* line);

    private:
        float currentTownTargetIndex;
        bool stopAtTownsAlongTheWay;
        int townListIndex;

    };




}
