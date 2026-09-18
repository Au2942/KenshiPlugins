#include "SquadAutonomyPanel.h"
#include "SquadAutonomySettings.h"

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
    lineBoxAIPackage = "AI package";
}

SquadAutonomyPanel* SquadAutonomyPanel::getSingletonPtr()
{
    static boost::scoped_ptr<SquadAutonomyPanel> singleton(new SquadAutonomyPanel());
    return singleton.get();
}

SquadAutonomyPanel::SquadAutonomyPanel() : _selectedSquad(nullptr), _panel(nullptr), _selectedAIPackageIndex(0), _category(0), _priority(0.0f)
{
    _options = new AutonomyOptions();
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
    this->_panel->setCaption("Squad Autonomy");
    this->_panel->setPanelName("SquadAutonomy");

    if (this->_options != nullptr)
    {
        this->_options->setPanel(this->_panel);
        this->_options->setSelectedSquad(this->_selectedSquad);
        this->_options->refresh();
    }

    this->_panel->showTabs(true);
    this->_panel->addTab(this->_category, "Main", "");
    this->_panel->addTab(_options->getCategory(), "Options", "");
    this->_panel->changeCategory(this->_category);

    refresh();

    this->_panel->show(false);
}

void SquadAutonomyPanel::refresh()
{
    if (this->_panel == nullptr)
        return;
    if (!_selectedSquad || !_selectedSquad->activePlatoon) return;
    if (!SquadAutonomySettings::initialized) return;
    /*if (shouldLoad)
    {
        SquadAutonomySettings::getSingletonPtr()->loadSettings(savePath);
        shouldLoad = false;
    }*/

    this->_panel->setLineSpacing(32.0f);
    this->_panel->clearPage(this->_category);
    auto squadSettings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
    this->_panel->setCaption("Squad Autonomy: " + _selectedSquad->activePlatoon->getName());

    DataPanelLine_Text* textbox;

    auto button = this->_panel->setLineToggleButton("", "Enable Autonomy", this->_category);
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_toggleAI);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
    if (squadSettings)
    {
        button->button->setStateSelected(squadSettings->isEnabled());
    }
    this->_panel->addSpace(this->_category, 0.25f);

    button = this->_panel->setLineTextButton("", "Set Squad Home Building", this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_setHome);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

    button = this->_panel->setLineTextButton("", "Set Squad Work Building", this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_setWork);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

    /*button = this->_panel->setLineTextButton("", "Make Building a Bar", this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_setBar);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());*/

    button = this->_panel->setLineTextButton("", "Clear Buildings", this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_clearBuildings);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
    this->_panel->addSpace(this->_category, 0.25f);

    auto editbox = this->_panel->setLineTextEditable("Search", "", this->_category, true, false, MyGUI::Align::Left, 0.95f);
    editbox->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_changeAIPackageSearchText);

    auto dropbox = this->_panel->setLineDropBox(lineBoxAIPackage, this->_category, &this->_selectedAIPackageIndex, false, 1.0f);

    _updateAIPackageList("");
    editbox->getEditBox()->setSize(dropbox->listBox->getSize());
    this->_panel->addSpace(this->_category, 0.25f);

    auto slider = this->_panel->setLineSliderEditable("Priority", this->_category, true, 0.0f, 5.0f, &this->_priority);
    slider->nameText->setEnabled(false);
    slider->setPrecision(0);
    this->_panel->addSpace(this->_category, 0.25f);

    button = this->_panel->setLineTextButton("", "Add AI Package", this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_addAI);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

    button = this->_panel->setLineTextButton("", "Clear AI Packages", this->_category, 1.0f, "Kenshi_Button2");
    button->callback = new MyGUI::delegates::CMethodDelegate1<SquadAutonomyPanel, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::_clearAI);
    button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
    button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
    this->_panel->addSpace(this->_category, 0.25f);
    if (squadSettings)
    {
        Building* building = squadSettings->getBuilding(true);
        TownBase* town;
        if (building)
        {
            std::string townText;
            textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left);
            textbox->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
            townText = "Home: " + building->displayName;
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
            townText = "Work: " + squadSettings->getBuilding(false)->displayName;
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

void SquadAutonomyPanel::AutonomyOptions::refresh()
{
    if (this->_panel == nullptr)
        return;
    if (this->_selectedSquad == nullptr) return;
    if (!this->_selectedSquad->isFullyLoaded() || !this->_selectedSquad->activePlatoon) return;
    if (!SquadAutonomySettings::initialized) return;
    this->_panel->setLineSpacing(24.0f);
    this->_panel->clearPage(this->_category);
    auto squadSettings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
    //get value from settings
    _manTurrets = squadSettings->getManTurrets();
    _doSleep = squadSettings->getDoSleep();
    _startWorkTime = squadSettings->getStartWorkTime();
    _endWorkTime = squadSettings->getEndWorkTime();
    _usePaidBeds = squadSettings->getUsePaidBeds();
    _restUntilHealed = squadSettings->getRestUntilHealed();

    this->_panel->setCaption("Squad Autonomy: " + _selectedSquad->activePlatoon->getName());
    //DebugLog("refresh options!");
    auto startSlider = this->_panel->setLineSliderEditable("Start Work Time", this->_category, true, 0.0f, 24.0f, &this->_startWorkTime);
    startSlider->nameText->setEnabled(false);
    startSlider->setPrecision(0);
    startSlider->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::AutonomyOptions::updateOptions);
    auto endSlider = this->_panel->setLineSliderEditable("End Work Time", this->_category, true, 0.0f, 24.0f, &this->_endWorkTime);
    endSlider->nameText->setEnabled(false);
    endSlider->setPrecision(0);
    endSlider->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::AutonomyOptions::updateOptions);

    auto checkbox = this->_panel->setLineCheckbox("Man Turrets", &_manTurrets, this->_category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::AutonomyOptions::updateOptions);
    checkbox = this->_panel->setLineCheckbox("Allow Going to Bed", &_doSleep, this->_category);
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &SquadAutonomyPanel::AutonomyOptions::updateOptionsAndRefresh);


    if (squadSettings->getDoSleep())
    {
        checkbox = this->_panel->setLineCheckbox("Rest until healed", &_restUntilHealed, this->_category);
        checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
            this, &SquadAutonomyPanel::AutonomyOptions::updateOptions);
        checkbox = this->_panel->setLineCheckbox("Allow Using Paid Beds", &_usePaidBeds, this->_category);
        checkbox->callback = new MyGUI::delegates::CMethodDelegate1<AutonomyOptions, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
            this, &SquadAutonomyPanel::AutonomyOptions::updateOptions);
    }
}
void SquadAutonomyPanel::AutonomyOptions::updateOptions(DataPanelLine* line)
{
    if (!SquadAutonomySettings::initialized) return;
    if (this->_selectedSquad == nullptr) return;
    auto squadSettings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
    squadSettings->setManTurrets(_manTurrets);
    squadSettings->setStartWorkTime(_startWorkTime);
    squadSettings->setEndWorkTime(_endWorkTime);
    squadSettings->setDoSleep(_doSleep);
    squadSettings->setRestUntilHealed(_restUntilHealed);
    squadSettings->setUsePaidBeds(_usePaidBeds);
}
void SquadAutonomyPanel::AutonomyOptions::updateOptionsAndRefresh(DataPanelLine* line)
{
    if (!SquadAutonomySettings::initialized) return;
    if (!this->_selectedSquad->isFullyLoaded()) return;
    updateOptions(line);
    refresh();
}
void SquadAutonomyPanel::show()
{
    if (!SquadAutonomySettings::initialized) return;
    if (!this->_selectedSquad->isFullyLoaded()) return;
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
    if (!SquadAutonomySettings::initialized) return;
    if (!squad->isFullyLoaded()) return;
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
    if (!SquadAutonomySettings::initialized) return;
    SquadAutonomySettings* settings = SquadAutonomySettings::getSingletonPtr();
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
        for (uint32_t i = 0; i < settings->getAIPackageList()->size(); ++i)
        {
            dropBox->addAValue(settings->getAIPackageList()->at(i)->name, i);
        }
    }
    else
    {
        std::string s1 = keyword;
        std::transform(s1.begin(), s1.end(), s1.begin(), [](char c) { return std::toupper(c); });
        for (uint32_t i = 0; i < settings->getAIPackageList()->size(); ++i)
        {
            std::string& name = settings->getAIPackageList()->at(i)->name;
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
    if (!SquadAutonomySettings::initialized) return;
    if (_selectedSquad && _selectedSquad->activePlatoon)
    {
        auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
        if (line != nullptr && line->classType == DataPanelLine::DPL_BUTTON)
        {
            auto button = reinterpret_cast<DataPanelLine_Button*>(line)->button;
            if (settings && settings->enableAutonomy(!button->getStateSelected()))
            {
                refresh();
                if (settings->isEnabled())
                {
                    ou->showPlayerAMessage(_selectedSquad->activePlatoon->getName() + ": Autonomy enable", true);
                }
                else
                {
                    ou->showPlayerAMessage(_selectedSquad->activePlatoon->getName() + ": Autonomy disable", true);
                }
            }
        }
    }
    else
    {
        ou->showPlayerAMessage("No squad selected/invalid squad", true);
    }
}

void SquadAutonomyPanel::_addAI(DataPanelLine* line)
{
    if (!SquadAutonomySettings::initialized) return;
    if (_selectedSquad)
    {
        GameData* data;
        SquadAutonomySettings* settings = SquadAutonomySettings::getSingletonPtr();
        SquadSettingsInfo* settingsInfo = settings->getSquadSettings(_selectedSquad, true);
        if (_selectedAIPackageIndex < settings->getAIPackageList()->size()) data = settings->getAIPackageList()->at(_selectedAIPackageIndex);
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
        ou->showPlayerAMessage("No squad selected/invalid squad", true);
    }
}

void SquadAutonomyPanel::_clearAI(DataPanelLine* line)
{
    if (!SquadAutonomySettings::initialized) return;
    if (_selectedSquad)
    {
        auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
        if (settings)
        {
            settings->clearPackages();

            settings->updateSquadPackages();

            refresh();
        }
    }
    else
    {
        ou->showPlayerAMessage("No squad selected/invalid squad", true);
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
    //use platoon->sethomebuildingdesignation instead
    if (!SquadAutonomySettings::initialized) return;
    Building* building = gui->selectedObject.getBuilding();
    if (!building)
    {
        ou->showPlayerAMessage("Select a building!", true);
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
    building->setDesignation(static_cast<BuildingDesignation>(squadTemplate->idata["building designation"]));
}

void SquadAutonomyPanel::_clearBuildings(DataPanelLine* line)
{
    if (!SquadAutonomySettings::initialized) return;
    if (!_selectedSquad)
    {
        ou->showPlayerAMessage("No squad selected/invalid squad", true);
        return;
    }
    auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);
    settings->setBuilding(nullptr, true);
    settings->setBuilding(nullptr, false);
    if (settings->isEnabled())
    {
        settings->unassignSquadHome();
    }
    ou->showPlayerAMessage("Clear buildings", true);

    refresh();

}

void SquadAutonomyPanel::_setBuilding(bool home)
{
    if (!SquadAutonomySettings::initialized) return;
    if (!_selectedSquad || !_selectedSquad->activePlatoon)
    {
        ou->showPlayerAMessage("No squad selected/invalid squad", true);
        return;
    }
    if (!gui->selectedObject)
    {
        ou->showPlayerAMessage("No object selected!", true);
        return;
    }
    auto settings = SquadAutonomySettings::getSingletonPtr()->getSquadSettings(_selectedSquad, true);

    Building* building = gui->selectedObject.getBuilding();
    if (!building)
    {
        ou->showPlayerAMessage("Select a building!", true);
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
    std::string report = "Set " + _selectedSquad->activePlatoon->getName();
    if (home) report += " home building: " + building->displayName;
    else report += " work building: " + building->displayName;
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