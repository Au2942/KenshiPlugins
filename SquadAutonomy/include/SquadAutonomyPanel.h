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

        void show();
        void hide();
        bool isVisible();

        void selectSquad(Platoon& selectedSquad);

    private:
        class MainTab;
        class OptionsTab;

        void createModSettingsButton();

        Platoon* _selectedSquad;
        DatapanelGUI* _panel;

        std::unique_ptr<MainTab> _mainTab;
        std::unique_ptr<OptionsTab> _optionsTab;
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
        OptionsTab(DatapanelGUI& panel, int category);

        void refresh();

        int getCategory() const;

        void setSelectedSquad(Platoon& selectedSquad);

    private:
        class OptionsBase;
        class BasicOptions;
        class LabourOptions;
        class CombatOptions;
        class GuardOptions;

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

        DataPanelLine_SliderEditable* createOptionsSlider(std::string, float*, int, float, float, bool = false);

        DatapanelGUI* _panel;
        Platoon* _selectedSquad;
        int _category;
        int _subCategory;

        std::unique_ptr<BasicOptions> _basic;
        std::unique_ptr<LabourOptions> _labour;
        std::unique_ptr<CombatOptions> _combat;
        std::unique_ptr<GuardOptions> _guard;
        //bool _manTurrets;
        //bool _closeGate;
        //bool _doSleep;
        //float _startWorkTime;
        //float _endWorkTime;
        //bool _restUntilHealed;
        //bool _usePaidBeds;
        //bool _buySupplies;
        //item type?
        //bool _sellLoots;
        //item type?
    };

    class SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        OptionsBase(OptionsTab& optionsTab, DatapanelGUI& panel, int category);

    protected:
        OptionsTab* _optionsTab;
        DatapanelGUI* _panel;
        int _category;
    };

    class SquadAutonomyPanel::OptionsTab::BasicOptions : public SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        BasicOptions(OptionsTab& optionsTab, DatapanelGUI& panel, int category);

        void refresh();

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
    };

    class SquadAutonomyPanel::OptionsTab::LabourOptions : public SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        LabourOptions(OptionsTab& optionsTab, DatapanelGUI& panel, int category);

        void refresh();

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
        CombatOptions(OptionsTab& optionsTab, DatapanelGUI& panel, int category);

        void refresh();

        void readFromSquadSettings(const SquadSettingsInfo& squadSettings);
        void writeToSquadSettings(SquadSettingsInfo& squadSettings) const;

    private:
        bool attackEnemies;
        bool protectAllies;
        bool doMedic;
        bool doRescue;
    };

    class SquadAutonomyPanel::OptionsTab::GuardOptions : public SquadAutonomyPanel::OptionsTab::OptionsBase
    {
    public:
        GuardOptions(OptionsTab& optionsTab, DatapanelGUI& panel, int category);

        void refresh();

        void readFromSquadSettings(const SquadSettingsInfo& squadSettings);
        void writeToSquadSettings(SquadSettingsInfo& squadSettings) const;

    private:
        bool manTurrets;
        bool stayInsideGate;
        bool closeGate;
    };
}
