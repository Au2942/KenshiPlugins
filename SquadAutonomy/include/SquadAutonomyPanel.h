#pragma once
#include "SquadAutonomySettings.h"
#include <kenshi/Platoon.h>
#include <kenshi/gui/DataPanelLine.h>

namespace SquadAutonomy
{
    class SquadAutonomyPanel
    {
    public:
        static SquadAutonomyPanel* getSingletonPtr();
        static bool initialized;
        SquadAutonomyPanel();
        ~SquadAutonomyPanel();

        void create();
        void refresh();
        void show();
        void hide();
        bool isVisible();
        void selectSquad(Platoon*);
        class AutonomyOptions;
        AutonomyOptions* getOptionsTab()
        {
            return _options;
        }
    private:
        int _category;
        Platoon* _selectedSquad;
        DatapanelGUI* _panel;
        int _selectedAIPackageIndex;
        float _priority;

        AutonomyOptions* _options;

        void _toggleAI(DataPanelLine* line); //button for enabling/setting packages to a squad
        void _addAI(DataPanelLine* line); //set the packages in the setting
        void _clearAI(DataPanelLine* line); //clear all packages in the setting
        void _setHome(DataPanelLine* line);
        void _setWork(DataPanelLine* line);
        void _setBar(DataPanelLine* line);
        void _clearBuildings(DataPanelLine* line);
        void _setBuilding(bool home);

        void _changeAIPackageSearchText(DataPanelLine* line);
        void _updateAIPackageList(const std::string& keyword);
    };
    class SquadAutonomyPanel::AutonomyOptions
    {
    public:
        void refresh();
        void refreshOptions(MyGUI::ComboBox*, size_t);
        void updateSettings(DataPanelLine*);
        void updateSettingsAndRefresh(DataPanelLine*);
        AutonomyOptions() : _category(1), _subCategory(0), _selectedSquad(nullptr), _panel(nullptr)
        {}
        void init();
        void setPanel(DatapanelGUI* panel)
        {
            _panel = panel;
        }
        void setSelectedSquad(Platoon* squad)
        {
            _selectedSquad = squad;
        }
        int getCategory()
        {
            return _category;
        }
        void updateOptions(SquadSettingsInfo*);
        class BasicOptions;
        class LabourOptions;
        class CombatOptions;
        class GuardOptions;

    private:
        DataPanelLine_SliderEditable* createOptionsSlider(std::string, float*, int, float, float, bool = false);
        int _category;
        int _subCategory;
        Platoon* _selectedSquad;
        DatapanelGUI* _panel;
        BasicOptions* _basic;
        LabourOptions* _labour;
        CombatOptions* _combat;
        GuardOptions* _guard;
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

    class SquadAutonomyPanel::AutonomyOptions::BasicOptions
    {
    public:
        void refresh(AutonomyOptions* ,int, DatapanelGUI*, SquadSettingsInfo *);
        BasicOptions() : startWorkTime(0.0f), endWorkTime(24.0f), doSleep(false), restUntilHealed(true), restThreshold(50.0), healedThreshold(90.0), usePaidBeds(false)
        {}
        float startWorkTime;
        float endWorkTime;
        bool restUntilHealed;
        float restThreshold;
        float healedThreshold;
        bool usePaidBeds;
        bool doSleep;
    };
    class SquadAutonomyPanel::AutonomyOptions::LabourOptions
    {
    public:
        void refresh(AutonomyOptions*, int, DatapanelGUI*, SquadSettingsInfo*);
        LabourOptions() : setAll(0.0), science(1.0),
            labouring(1.0), farming(1.0), 
            medic(1.0), cooking(1.0), engineer(1.0), robotics(1.0), weaponSmith(1.0), armourSmith(1.0), crossbowSmith(1.0), automaticMachine(1.0), other(1.0)
        {}
        void setAllTo(MyGUI::Widget*);
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
    class SquadAutonomyPanel::AutonomyOptions::CombatOptions
    {
    public:
        void refresh(AutonomyOptions*, int, DatapanelGUI*, SquadSettingsInfo*);
        CombatOptions() : attackEnemies(true), protectAllies(true), doMedic(true), doRescue(true)
        {}
        bool attackEnemies;
        bool protectAllies;
        bool doMedic;
        bool doRescue;
    };
    class SquadAutonomyPanel::AutonomyOptions::GuardOptions
    {
    public:
        void refresh(AutonomyOptions*, int, DatapanelGUI*, SquadSettingsInfo*);
        GuardOptions() : manTurrets(false), stayInsideGate(false), closeGate(false)
        {}
        bool manTurrets;
        bool stayInsideGate;
        bool closeGate;
    };
}