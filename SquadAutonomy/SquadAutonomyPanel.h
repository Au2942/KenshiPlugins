#pragma once
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
        void updateOptions(DataPanelLine*);
        void updateOptionsAndRefresh(DataPanelLine*);
        SquadAutonomyPanel::AutonomyOptions() : _category(1), _selectedSquad(nullptr), _panel(nullptr), _startWorkTime(0.0f), _endWorkTime(24.0f), _manTurrets(false), _doSleep(false), _restUntilHealed(true), _usePaidBeds(false)
        {
        }
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

    private:
        int _category;
        Platoon* _selectedSquad;
        DatapanelGUI* _panel;
        bool _manTurrets;
        bool _closeGate;
        bool _doSleep;
        float _startWorkTime;
        float _endWorkTime;
        bool _restUntilHealed;
        bool _usePaidBeds;
        //bool _buySupplies;
        //item type?
        //bool _sellLoots;
        //item type?
    };
}