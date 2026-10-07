#include "SquadAutonomy.h"
#include "SquadAutonomyPanel.h"
#include "SquadAutonomyLocalization.h"

#include <Debug.h>

#include <ogre/OgreStringConverter.h>
#include <boost/scoped_ptr.hpp>
#include <ogre/OgrePrerequisites.h>
#include <kenshi/util/OgreUnordered.h>

#include <kenshi/Globals.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>

#include <kenshi/Town.h>
#include <kenshi/Building/Building.h>
#include <kenshi/GameWorld.h>

using namespace SquadAutonomy;

bool SquadAutonomyPanel::initialized = false;
std::string lineBoxAIPackage;

void initLineKey()
{
    lineBoxAIPackage = Localization::gettext("AI package");
}

SquadAutonomyPanel* SquadAutonomyPanel::getSingletonPtr()
{
    static boost::scoped_ptr<SquadAutonomyPanel> singleton(new SquadAutonomyPanel());
    return singleton.get();
}

SquadAutonomyPanel::SquadAutonomyPanel() : _selectedSquad(nullptr), _panel(nullptr), _selectedAIPackageIndex(0), _category(0), _priority(0.0f)
{
    
    initLineKey();
    create();
    initialized = true;

}
SquadAutonomyPanel::~SquadAutonomyPanel()
{
    if (_options != nullptr)
    {
        delete _options;
    }
}

void SquadAutonomyPanel::create()
{
    if (this->_panel != nullptr)
    {
        this->_panel->show(false);
        gui->destroy(this->_panel);
    }

    this->_panel = gui->createDatapanel(0.25f, 0.375f, 0.25f, 0.5f, true, "Window", true);
    this->_panel->setCaption(Localization::gettext("Squad Autonomy"));
    this->_panel->setPanelName("SquadAutonomy");

    _options = new AutonomyOptions();
    if (this->_options != nullptr)
    {
        this->_options->init();
        this->_options->setPanel(this->_panel);
        this->_options->setSelectedSquad(this->_selectedSquad);
        this->_options->refresh();
    }

    this->_panel->showTabs(true);
    this->_panel->addTab(this->_category, Localization::gettext("Main"), "");
    this->_panel->addTab(_options->getCategory(), Localization::gettext("Options"), "");
    this->_panel->changeCategory(this->_category);

    refresh();

    this->_panel->show(false);
}

void SquadAutonomyPanel::refresh()
{
    if (this->_panel == nullptr)
        return;
    if (!_selectedSquad) return;
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    auto squadSettings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
    this->_panel->setLineSpacing(32.0);
    this->_panel->clearPage(this->_category);

    if (_selectedSquad->activePlatoon) this->_panel->setCaption(Localization::gettext("Squad Autonomy:") + ' ' + _selectedSquad->activePlatoon->getName());

    DataPanelLine_Text* textbox;

    auto button = this->_panel->setLineToggleButton("", Localization::gettext("Enable Autonomy"), this->_category);
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_toggleAI);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
    if (squadSettings)
    {
        button->button->setStateSelected(squadSettings->isEnabled());
    }
    this->_panel->addSpace(this->_category, 0.25f);

    button = this->_panel->setLineTextButton("", Localization::gettext("Set Squad Home Building"), this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_setHome);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

    button = this->_panel->setLineTextButton("", Localization::gettext("Set Squad Work Building"), this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_setWork);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

    /*button = this->_panel->setLineTextButton("", "Make Building a Bar", this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_setBar);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());*/

    button = this->_panel->setLineTextButton("", Localization::gettext("Clear Buildings"), this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_clearBuildings);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
    this->_panel->addSpace(this->_category, 0.25f);
    this->_panel->setLineSpacing(24.0);
    auto editbox = this->_panel->setLineTextEditable(Localization::gettext("Search"), "", this->_category, true, false, MyGUI::Align::Left, 0.95f);
    editbox->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_changeAIPackageSearchText);

    auto dropbox = this->_panel->setLineDropBox(lineBoxAIPackage, this->_category, &this->_selectedAIPackageIndex, false, 1.0f);
    dropbox->w1->setSize(dropbox->w1->getWidth(), editbox->editBox->getHeight());
    _updateAIPackageList("");
    editbox->getEditBox()->setSize(dropbox->listBox->getSize());
    this->_panel->addSpace(this->_category, 0.25f);

    auto slider = this->_panel->setLineSliderEditable(Localization::gettext("Priority"), this->_category, true, 0.0f, 5.0f, &this->_priority);
    slider->nameText->setEnabled(false);
    slider->setPrecision(0);
    this->_panel->addSpace(this->_category, 0.25f);
    this->_panel->setLineSpacing(32.0);
    button = this->_panel->setLineTextButton("", Localization::gettext("Add AI Package"), this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_addAI);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

    button = this->_panel->setLineTextButton("", Localization::gettext("Clear AI Packages"), this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_clearAI);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
    this->_panel->addSpace(this->_category, 0.25f);
    this->_panel->setLineSpacing(24.0);
    if (squadSettings)
    {
        Building* building = squadSettings->getBuilding(true);
        TownBase* town;
        if (building)
        {
            std::string townText;
            textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left);
            textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
            townText = Localization::gettext("Home:") + ' ' + building->displayName;
            town = building->getTown();
            if (town)
            {
                townText += ", " + town->getKnownName();
            }
            textbox->editBox->setCaption(townText);
        }
        building = squadSettings->getBuilding(false);
        if (building)
        {
            std::string townText;
            textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left);
            textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
            townText = Localization::gettext("Work:") + ' ' + squadSettings->getBuilding(false)->displayName;
            town = building->getTown();
            if (town)
            {
                townText += ", " + town->getKnownName();
            }
            textbox->editBox->setCaption(townText);
        }

        auto squadPackages = squadSettings->getSquadPackages();
        if (squadPackages.size() > 0)
        {
            //DebugLog("Squad Settings exist: " + Ogre::StringConverter::toString(squadPackages.size()));
            for (auto it = squadPackages.begin(); it != squadPackages.end(); ++it)
            {
                auto packages = it->second;
                //DebugLog(Ogre::StringConverter::toString(it->first) + " Packages: " + Ogre::StringConverter::toString(packages.size()));
                for (int i = 0; i < packages.size(); ++i)
                {
                    //DebugLog(Ogre::StringConverter::toString(it->first) + " " + packages[i]->name);
                    textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left);
                    textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
                    textbox->editBox->setCaption(Ogre::StringConverter::toString(it->first) + " " + packages[i]->name);
                }
            }
        }
    }
    //textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left); //buffer text for scrolling
    //this->_panel->addSpace(this->_category, 6.0f);

}


void SquadAutonomyPanel::show()
{
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    if (!this->_selectedSquad) return;
    this->_panel->changeCategory(this->_category);
    this->_panel->show(true);
    MyGUI::LayerManager::getInstancePtr()->upLayerItem(this->_panel->getWidget());
    refresh();
    _options->refresh();
}

void SquadAutonomyPanel::hide()
{
    this->_panel->show(false);
}

bool SquadAutonomyPanel::isVisible()
{
    return this->_panel->isVisible();
}

void SquadAutonomyPanel::selectSquad(Platoon* squad)
{
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    if (!squad) return;
    _selectedSquad = squad;
    _options->setSelectedSquad(squad);
}

void SquadAutonomyPanel::_changeAIPackageSearchText(DataPanelLine* line)
{
    std::string keyword = "";
    if (line != nullptr && line->classType == DataPanelLine::DPL_TEXT_EDIT)
        keyword = reinterpret_cast<DataPanelLine_TextEditable*>(line)->editBox->getOnlyText();

    _updateAIPackageList(keyword);
}

void SquadAutonomyPanel::_updateAIPackageList(const std::string& keyword)
{
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    SquadAutonomySettings& settings = SquadAutonomySettings::getSingleton();
    auto dropBox = reinterpret_cast<DataPanelLine_DropBox*>(this->_panel->getLine(lineBoxAIPackage, this->_category));
    if (dropBox == nullptr)
        return;

    int currentSelected = this->_selectedAIPackageIndex;
    if (currentSelected < 0)
        currentSelected = 0;
    int selectVal = -1;
    dropBox->clearValues();
    if (keyword.empty())
    {
        selectVal = currentSelected;
        for (uint32_t i = 0; i < settings.getAIPackageList()->size(); ++i)
        {
            dropBox->addAValue(settings.getAIPackageList()->at(i)->name, i);
        }
    }
    else
    {
        std::string s1 = keyword;
        std::transform(s1.begin(), s1.end(), s1.begin(), [](char c) { return std::toupper(c); });
        for (uint32_t i = 0; i < settings.getAIPackageList()->size(); ++i)
        {
            std::string& name = settings.getAIPackageList()->at(i)->name;
            std::string s2 = name;
            std::transform(s2.begin(), s2.end(), s2.begin(), [](char c) { return std::toupper(c); });
            if (s2.find(s1) != std::string::npos)
            {
                dropBox->addAValue(name, i);
                if (selectVal == -1 || i == currentSelected)
                    selectVal = i;
            }
        }
    }
    dropBox->setSelectedValue(selectVal);
}

void SquadAutonomyPanel::_toggleAI(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    if (_selectedSquad && _selectedSquad->activePlatoon)
    {
        auto settings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
        if (line != nullptr && line->classType == DataPanelLine::DPL_BUTTON)
        {
            auto button = reinterpret_cast<DataPanelLine_Button*>(line)->button;
            if (settings && settings->enableAutonomy(!button->getStateSelected()))
            {
                refresh();
                if (settings->isEnabled())
                {
                    ou->showPlayerAMessage(_selectedSquad->activePlatoon->getName() + Localization::gettext(": Autonomy enable"), true);
                }
                else
                {
                    ou->showPlayerAMessage(_selectedSquad->activePlatoon->getName() + Localization::gettext(": Autonomy disable"), true);
                }
            }
        }
    }
    else
    {
        ou->showPlayerAMessage(Localization::gettext("No squad selected/invalid squad"), true);
    }
}

void SquadAutonomyPanel::_addAI(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    if (_selectedSquad)
    {
        GameData* data;
        SquadAutonomySettings& settings = SquadAutonomySettings::getSingleton();
        SquadSettingsInfo* settingsInfo = settings.getSquadSettings(_selectedSquad, true);
        if (_selectedAIPackageIndex < settings.getAIPackageList()->size()) data = settings.getAIPackageList()->at(_selectedAIPackageIndex);
        if (data && settingsInfo)
        {
            settingsInfo->addPackage(static_cast<int>(_priority), data);
            //DebugLog("Add " + data->name + " package");

            settingsInfo->updateSquadPackages();

            refresh();
        }
    }
    else
    {
        ou->showPlayerAMessage(Localization::gettext("No squad selected/invalid squad"), true);
    }
}

void SquadAutonomyPanel::_clearAI(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    if (_selectedSquad)
    {
        auto settings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
        if (settings)
        {
            settings->clearPackages();

            settings->updateSquadPackages();

            refresh();
        }
    }
    else
    {
        ou->showPlayerAMessage(Localization::gettext("No squad selected/invalid squad"), true);
    }
}

void SquadAutonomyPanel::_setHome(DataPanelLine* line)
{
    _setBuilding(true);
}

void SquadAutonomyPanel::_setWork(DataPanelLine* line)
{
    _setBuilding(false);
}

void SquadAutonomyPanel::_setBar(DataPanelLine* line)
{
/*    //use platoon->sethomebuildingdesignation instead
    if (!SquadAutonomySettings::getSingletonPtr()->initialized) return;
    Building* building = gui->selectedObject.getBuilding();
    if (!building)
    {
        ou->showPlayerAMessage(Localization::gettext("Select a building!"), true);
        return;
    }
    if (building->isFurnitureOrDoor())
    {
        if (building->isDoor())
        {
            //DebugLog("Is a door of " + building->doorParentBuilding()->displayName);
            building = building->doorParentBuilding();
        }
        else if (building->isFurniture())
        {
            //DebugLog("Is a furniture of " + building->furnitureParentBuilding()->displayName);
            building = building->furnitureParentBuilding();
        }
    }
    GameData* squadTemplate = (*SquadAutonomySettings::getSingletonPtr()->getSquadTemplate())[0];
    building->residentSquadTemplate = squadTemplate;
    //DebugLog("Designation: " + Ogre::StringConverter::toString(squadTemplate->idata["building designation"]));
    building->setDesignation(static_cast<BuildingDesignation>(squadTemplate->idata["building designation"]));*/
}

void SquadAutonomyPanel::_clearBuildings(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    if (!_selectedSquad)
    {
        ou->showPlayerAMessage(Localization::gettext("No squad selected/invalid squad"), true);
        return;
    }
    auto settings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
    settings->setBuilding(nullptr, true);
    settings->setBuilding(nullptr, false);
    settings->unassignSquadHome();
    ou->showPlayerAMessage(Localization::gettext("Buildings cleared"), true);

    refresh();

}

void SquadAutonomyPanel::_setBuilding(bool home)
{
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    if (!_selectedSquad || !_selectedSquad->activePlatoon)
    {
        ou->showPlayerAMessage(Localization::gettext("No squad selected/invalid squad"), true);
        return;
    }
    if (!gui->selectedObject)
    {
        ou->showPlayerAMessage(Localization::gettext("No object selected!"), true);
        return;
    }
    auto settings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);

    Building* building = gui->selectedObject.getBuilding();
    if (!building)
    {
        ou->showPlayerAMessage(Localization::gettext("Select a building!"), true);
        return;
    }
    if (building->isFurnitureOrDoor())
    {
        if (building->isDoor())
        {
            //DebugLog("Is a door of " + building->doorParentBuilding()->displayName);
            building = building->doorParentBuilding();
        }
        else if (building->isFurniture())
        {
            //DebugLog("Is a furniture of " + building->furnitureParentBuilding()->displayName);
            building = building->furnitureParentBuilding();
        }
    }

    //_selectedSquad->me->getOwnerships()->setHomeBuilding(building, _selectedSquad->me->getSquadType());
    settings->setBuilding(building, home);
    std::string report = Localization::gettext("Set") + ' ' + _selectedSquad->activePlatoon->getName();
    if (home) report += ' ' + Localization::gettext("home building:") + ' ' + building->displayName;
    else report += ' ' + Localization::gettext("work building:") + ' ' + building->displayName;
    TownBase* town = building->getCurrentTownLocation();
    if (town)
    {
        //_selectedSquad->me->getOwnerships()->setHomeTown(town, _selectedSquad->me->squadType);
        report += ", " + town->getKnownName();
    }
    ou->showPlayerAMessage(report, true);

    settings->updateSquadPackages();
    refresh();
}

void SquadAutonomyPanel::AutonomyOptions::init()
{
    _basic = new BasicOptions();
    _labour = new LabourOptions();
    _combat = new CombatOptions();
    _guard = new GuardOptions();
}

void SquadAutonomy::SquadAutonomyPanel::AutonomyOptions::updateOptions(SquadSettingsInfo* settings)
{
    if (_basic)
    {
        _basic->startWorkTime = settings->getStartWorkTime();
        _basic->endWorkTime = settings->getEndWorkTime();
        _basic->doSleep = settings->getDoSleep();
        _basic->restUntilHealed = settings->getRestUntilHealed();
        _basic->restThreshold = settings->getRestThreshold();
        _basic->healedThreshold = settings->getHealedThreshold();
        _basic->usePaidBeds = settings->getUsePaidBeds();
    }
    if (_labour)
    {
        _labour->science = settings->getLabourScience();
        _labour->labouring = settings->getLabourLabouring();
        _labour->farming = settings->getLabourFarming();
        _labour->cooking = settings->getLabourCooking();
        _labour->medic = settings->getLabourMedic();
        _labour->engineer = settings->getLabourEngineer();
        _labour->robotics = settings->getLabourRobotics();
        _labour->weaponSmith = settings->getLabourWeaponSmith();
        _labour->armourSmith = settings->getLabourArmourSmith();
        _labour->crossbowSmith = settings->getLabourCrossbowSmith();
        _labour->automaticMachine = settings->getLabourAutomaticMachine();
        _labour->other = settings->getLabourOther();
    }
    if (_combat)
    {
        _combat->attackEnemies = settings->getAttackEnemies();
        _combat->protectAllies = settings->getProtectAllies();
        _combat->doMedic = settings->getDoMedic();
        _combat->doRescue = settings->getDoRescue();
    }
    if (_guard)
    {
        _guard->manTurrets = settings->getManTurrets();
        _guard->stayInsideGate = settings->getStayInsideGate();
        _guard->closeGate = settings->getCloseGate();
    }
}

void SquadAutonomyPanel::AutonomyOptions::refresh()
{
    if (!this->_panel)
        return;
    if (!this->_selectedSquad) return;
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    auto settings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);

    this->_panel->setLineSpacing(24.0);
    this->_panel->clearPage(this->_category);
    

    if (this->_selectedSquad->activePlatoon) this->_panel->setCaption(Localization::gettext("Squad Autonomy:") + ' ' + _selectedSquad->activePlatoon->getName());


    auto dropBox = this->_panel->setLineDropBox(Localization::gettext("Category"), this->_category, &this->_subCategory, false, 1.0f);
    //dropBox->clearValues();
    if (_subCategory < 0) _subCategory = 0;
    else if (_subCategory > 3) _subCategory = 3;
    dropBox->addAValue(Localization::gettext("Basic"), 0);
    dropBox->addAValue(Localization::gettext("Labour"), 1);
    dropBox->addAValue(Localization::gettext("Combat"), 2);
    dropBox->addAValue(Localization::gettext("Guard"), 3);
    dropBox->setSelectedValue(_subCategory);

    if (!settings) return;
    dropBox->getComboBox()->eventComboAccept += MyGUI::newDelegate(this, &SquadAutonomyPanel::AutonomyOptions::refreshOptions);
    updateOptions(settings);
    switch (_subCategory)
    {
    case 1: 
        _labour->refresh(this, _category, _panel, settings); 
        break;
    case 2: 
        _combat->refresh(this, _category, _panel, settings); 
        break;
    case 3: 
        _guard->refresh(this, _category, _panel, settings); 
        break;
    default: 
        _basic->refresh(this, _category, _panel, settings); 
        break;
    }
    
}
void SquadAutonomyPanel::AutonomyOptions::refreshOptions(MyGUI::ComboBox* sender, size_t index)
{
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    if (!this->_selectedSquad) return;
    refresh();
}
void SquadAutonomyPanel::AutonomyOptions::updateSettings(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    if (!this->_selectedSquad) return;
    auto squadSettings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
    if (_basic)
    {
        squadSettings->setStartWorkTime(_basic->startWorkTime);
        squadSettings->setEndWorkTime(_basic->endWorkTime);
        squadSettings->setDoSleep(_basic->doSleep);
        squadSettings->setRestUntilHealed(_basic->restUntilHealed);
        squadSettings->setRestThreshold(_basic->restThreshold);
        squadSettings->setHealedThreshold(_basic->healedThreshold);
        squadSettings->setUsePaidBeds(_basic->usePaidBeds);
    }
    if (_labour)
    {
        squadSettings->setLabourScience(_labour->science);
        squadSettings->setLabourLabouring(_labour->labouring);
        squadSettings->setLabourFarming(_labour->farming);
        squadSettings->setLabourCooking(_labour->cooking);
        squadSettings->setLabourMedic(_labour->medic);
        squadSettings->setLabourEngineer(_labour->engineer);
        squadSettings->setLabourRobotics(_labour->robotics);
        squadSettings->setLabourWeaponSmith(_labour->weaponSmith);
        squadSettings->setLabourArmourSmith(_labour->armourSmith);
        squadSettings->setLabourCrossbowSmith(_labour->crossbowSmith);
        squadSettings->setLabourAutomaticMachine(_labour->automaticMachine);
        squadSettings->setLabourOther(_labour->other);
    }
    if (_combat)
    {
        squadSettings->setAttackEnemies(_combat->attackEnemies);
        squadSettings->setProtectAllies(_combat->protectAllies);
        squadSettings->setDoMedic(_combat->doMedic);
        squadSettings->setDoRescue(_combat->doRescue);
    }
    if (_guard)
    {
        squadSettings->setManTurrets(_guard->manTurrets);
        squadSettings->setStayInsideGate(_guard->stayInsideGate);
        squadSettings->setCloseGate(_guard->closeGate);
        squadSettings->updateSquadPackages();
    }
}
void SquadAutonomyPanel::AutonomyOptions::updateSettingsAndRefresh(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized) return;
    if (!this->_selectedSquad) return;
    updateSettings(line);
    refresh();
}

void SquadAutonomyPanel::AutonomyOptions::BasicOptions::refresh(AutonomyOptions* options, int category, DatapanelGUI* panel, SquadSettingsInfo* settings)
{

    auto startSlider = panel->setLineSliderEditable(Localization::gettext("Start Work Time"), category, true, 0.0f, 24.0f, &this->startWorkTime);
    startSlider->nameText->setEnabled(false);
    startSlider->setPrecision(0);
    startSlider->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
    auto endSlider = panel->setLineSliderEditable(Localization::gettext("End Work Time"), category, true, 0.0f, 24.0f, &this->endWorkTime);
    endSlider->nameText->setEnabled(false);
    endSlider->setPrecision(0);
    endSlider->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
    auto checkbox = panel->setLineCheckbox(Localization::gettext("Rest Until Healed When Wounded"), &restUntilHealed, category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettingsAndRefresh);
    if (restUntilHealed)
    {
        auto textbox = panel->setLineText("", "", category, true, MyGUI::Align::Left);
        textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
        textbox->editBox->setCaption(Localization::gettext("Rest Health Threshold"));

        auto slider = panel->setLineSliderEditable("Rest Health Threshold", category, false, 0.0f, 100.0f, &this->restThreshold);
        //slider->nameText->setEnabled(false);
        slider->setPrecision(0);
        slider->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
            options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);

        textbox = panel->setLineText("", "", category, true, MyGUI::Align::Left);
        textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
        textbox->editBox->setCaption(Localization::gettext("Healed Health Threshold"));


        slider = panel->setLineSliderEditable("Healed Health Threshold", category, false, 0.0f, 100.0f, &this->healedThreshold);
        //slider->nameText->setEnabled(false);
        slider->setPrecision(0);
        slider->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
            options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
    }
    checkbox = panel->setLineCheckbox(Localization::gettext("Allow Using Paid Beds"), &usePaidBeds, category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
    checkbox = panel->setLineCheckbox(Localization::gettext("Allow Sleep Outside Work Hours"), &doSleep, category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
}

DataPanelLine_SliderEditable* SquadAutonomyPanel::AutonomyOptions::createOptionsSlider(std::string key, float* var, int precision, float min, float max, bool seperateTextLine)
{
    DataPanelLine_Text* textbox;
    if (seperateTextLine)
    {
        textbox = _panel->setLineText("", "", _category, true, MyGUI::Align::Left);
        textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
        textbox->editBox->setCaption(key);
    }
    auto slider = _panel->setLineSliderEditable(key, _category, !seperateTextLine, min, max, var);
    slider->setPrecision(precision);
    int wrapCount = slider->nameText->getTextSize().width / slider->nameText->getTextRegion().width;
    int margin = slider->nameText->getHeight() - slider->nameText->getTextRegion().height;
    /*DebugLog(slider->nameText->getCaption().asUTF8() + " textbox size : " + Ogre::StringConverter::toString(slider->nameText->getTextRegion().width) + " text size: " + Ogre::StringConverter::toString(slider->nameText->getTextSize().width)
    + " wrapCount: " + Ogre::StringConverter::toString(wrapCount));*/
    slider->nameText->setSize(slider->nameText->getWidth(), margin + slider->nameText->getTextRegion().height*(wrapCount+1));
    slider->nameText->setEditWordWrap(true);
    int nameTextMid = slider->nameText->getTop() + slider->nameText->getHeight() * 0.5;
    slider->sliderBar->setPosition(slider->sliderBar->getLeft(), nameTextMid - slider->sliderBar->getHeight() * 0.5);
    slider->valueEditBox->setPosition(slider->valueEditBox->getLeft(), nameTextMid - slider->valueEditBox->getHeight() * 0.5);
    slider->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &AutonomyOptions::updateSettings);
    return slider;
}

void SquadAutonomyPanel::AutonomyOptions::LabourOptions::setAllTo(MyGUI::Widget* sender)
{
    science = setAll;
    labouring = setAll;
    farming = setAll;
    cooking = setAll;
    medic = setAll;
    engineer = setAll;
    robotics = setAll;
    weaponSmith = setAll;
    armourSmith = setAll;
    crossbowSmith = setAll;
    automaticMachine = setAll;
    other = setAll;
    auto options = *(sender->getUserData<AutonomyOptions*>());
    if (options) options->updateSettingsAndRefresh(nullptr);
}

void SquadAutonomyPanel::AutonomyOptions::LabourOptions::refresh(AutonomyOptions* options, int category, DatapanelGUI* panel, SquadSettingsInfo* settings)
{
    panel->setLineSpacing(28.0);
    auto textbox = panel->setLineText("", *_MainColorCode + Localization::gettext("Stats Used:") + ' ' + Localization::gettext("(bigger number = higher priority, 0 = forbid)"), category, false, MyGUI::Align::Left);
    textbox->editBox->setColour(MyGUI::Colour(1.0, 1.0, 1.0));
    const float maxPriority = 10.0;
    options->createOptionsSlider(Localization::gettext("Science"), &science, 0, 0.0, maxPriority, false);
    options->createOptionsSlider(Localization::gettext("Labouring (Mining)"), &labouring, 0, 0.0, maxPriority, false);
    options->createOptionsSlider(Localization::gettext("Farming"), &farming, 0, 0.0, maxPriority, false);
    options->createOptionsSlider(Localization::gettext("Cooking"), &cooking, 0, 0.0, maxPriority, false);
    options->createOptionsSlider(Localization::gettext("Medic"), &medic, 0, 0.0, maxPriority, false);
    options->createOptionsSlider(Localization::gettext("Engineer"), &engineer, 0, 0.0, maxPriority, false);
    options->createOptionsSlider(Localization::gettext("Robotics"), &robotics, 0, 0.0, maxPriority, false);
    options->createOptionsSlider(Localization::gettext("Weapon Smith"), &weaponSmith, 0, 0.0, maxPriority, false);
    options->createOptionsSlider(Localization::gettext("Armour Smith"), &armourSmith, 0, 0.0, maxPriority, false);
    options->createOptionsSlider(Localization::gettext("Crossbow Smith"), &crossbowSmith, 0, 0.0, maxPriority, false);
    options->createOptionsSlider(Localization::gettext("Automatic Machine"), &automaticMachine, 0, 0.0, maxPriority, false);
    options->createOptionsSlider(Localization::gettext("Other"), &other, 0, 0.0, maxPriority, false);

    panel->addSpace(category, 0.25);
    auto slider = panel->setLineSliderEditable(Localization::gettext("Set All"), category, true, 0.0, maxPriority, &setAll);
    slider->setPrecision(0);
    auto editBox = slider->valueEditBox;
    auto valueEditPos = editBox->getPosition();
    auto button = panel->setLineButton("", Localization::gettext("Set"), category);
    button->button->setPosition(valueEditPos.left, valueEditPos.top + editBox->getHeight());
    button->button->setSize(editBox->getWidth(), editBox->getHeight());
    slider->sliderBar->setPosition(slider->sliderBar->getLeft(), (editBox->getTop() + editBox->getHeight()) - slider->sliderBar->getHeight() * 0.5);
    slider->nameText->setPosition(slider->nameText->getLeft(), (editBox->getTop() + editBox->getHeight()) - slider->nameText->getHeight() * 0.5);
    //button->button->setCaption(Localization::gettext("Set"));
    button->button->setUserData(options);
    button->button->eventMouseButtonClick += MyGUI::newDelegate(this, &LabourOptions::setAllTo);
    panel->addSpace(category, 1.0);
}

void SquadAutonomyPanel::AutonomyOptions::CombatOptions::refresh(AutonomyOptions* options, int category, DatapanelGUI* panel, SquadSettingsInfo* settings)
{

    auto checkbox = panel->setLineCheckbox(Localization::gettext("Attack Enemies"), &attackEnemies, category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
    checkbox = panel->setLineCheckbox(Localization::gettext("Protect Allies"), &protectAllies, category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
    checkbox = panel->setLineCheckbox(Localization::gettext("Do Medic"), &doMedic, category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
    checkbox = panel->setLineCheckbox(Localization::gettext("Rescue Downed Allies"), &doRescue, category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
}

void SquadAutonomyPanel::AutonomyOptions::GuardOptions::refresh(AutonomyOptions* options, int category, DatapanelGUI* panel, SquadSettingsInfo* settings)
{

    auto checkbox = panel->setLineCheckbox(Localization::gettext("Man Turrets"), &manTurrets, category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
    auto textbox = panel->setLineText("", *_MainColorCode + Localization::gettext("Gate Guard:"), category, false, MyGUI::Align::Left);
    checkbox = panel->setLineCheckbox(Localization::gettext("Stay Inside Gate"), &stayInsideGate, category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
    checkbox = panel->setLineCheckbox(Localization::gettext("Close Gate"), &closeGate, category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(options),
        options, &SquadAutonomyPanel::AutonomyOptions::updateSettings);
}