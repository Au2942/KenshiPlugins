#include "SquadAutonomy.h"
#include "SquadAutonomyPanel.h"
#include "SquadAutonomyLog.h"
#include "SquadAutonomyLocalization.h"
#include "SquadAutonomyPanelUtils.h"

#include <Debug.h>

#include <ogre/OgreStringConverter.h>
#include <ogre/OgrePrerequisites.h>
#include <kenshi/util/OgreUnordered.h>

#include <kenshi/Globals.h>
#include <kenshi/SharedKing.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>

#include <mygui/MyGUI_Button.h>

#include <kenshi/Town.h>
#include <kenshi/Building/Building.h>
#include <kenshi/GameWorld.h>
//#include <kenshi/ZoneManager.h>

using namespace SquadAutonomy;

namespace Utils = SquadAutonomyPanelUtils;

SquadAutonomyPanel& SquadAutonomyPanel::getSingleton()
{
    static std::unique_ptr<SquadAutonomyPanel> singleton;
    if (!singleton)
    {
        singleton.reset(new SquadAutonomyPanel());
    }

    return *singleton.get();
}

SquadAutonomyPanel::SquadAutonomyPanel() :
    _selectedSquad(nullptr),
    _panel(nullptr),
    _popupPanel(nullptr)
{
    create();
}

SquadAutonomyPanel::~SquadAutonomyPanel()
{
}

void SquadAutonomyPanel::create()
{
    if (_panel != nullptr)
    {
        _panel->show(false);
        gui->destroy(_panel);
    }

    _panel = gui->createDatapanel(0.25f, 0.375f, 0.25f, 0.5f, true, "Window", true);
    _panel->setCaption(Localization::lineSquadAutonomy);
    _panel->setPanelName("SquadAutonomy");
    
    createPopup();

    _mainTab.reset(new MainTab(*_panel, 0));
    _optionsTab.reset(new OptionsTab(*_panel, *_popupPanel, 1));

    if (_selectedSquad)
    {
        _mainTab->setSelectedSquad(*_selectedSquad);
        _optionsTab->setSelectedSquad(*_selectedSquad);
    }

    _panel->showTabs(true);
    _panel->addTab(_mainTab->getCategory(), Localization::gettext("Main"), "");
    _panel->addTab(_optionsTab->getCategory(), Localization::gettext("Options"), "");


    _panel->changeCategory(_mainTab->getCategory());
    _panel->tabEvent += MyGUI::newDelegate(this, &SquadAutonomyPanel::onTabChanged);
    MyGUI::Window* panelWindow = _panel->win->castType<MyGUI::Window>();
    panelWindow->eventWindowButtonPressed += MyGUI::newDelegate(this, &SquadAutonomyPanel::onPanelClosed);

    refresh();

    createModSettingsButton();

    _panel->show(false);

    
}

void SquadAutonomyPanel::createPopup()
{
    // create popup panel
    if (_popupPanel != nullptr)
    {
        _popupPanel->show(false);
        gui->destroy(_popupPanel);
    }

    _popupPanel = gui->createDatapanel(0.25f, 0.125f, 0.25f, 0.3f, true, "Window", true);
    //_popupPanel->setCaption(Localization::lineSquadAutonomyPopup);
    _popupPanel->setPanelName("SquadAutonomyPopup");
    _popupPanel->show(false);

}

void SquadAutonomyPanel::onTabChanged(DatapanelGUI* panel, int category)
{
    refresh();
    _popupPanel->show(false);
    if (_optionsTab)
    {
        _optionsTab->updateFloatingPanel(category);
    }
}

void SquadAutonomyPanel::onPanelClosed(MyGUI::Widget* sender, const std::string& name)
{
    hide();
}

void SquadAutonomyPanel::OptionsTab::updateFloatingPanel(int category)
{
    if (!_floatingPanel) return;
    _floatingPanel->setVisible(category == 1);
}

void SquadAutonomyPanel::refresh()
{
    /*
    for (int i = 0; i < _panel->tabs.size(); ++i)
    {
        _panel->clearPage(i);
    }
    */
    _mainTab->refresh();
    _optionsTab->refresh();
}

void SquadAutonomyPanel::show()
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_selectedSquad)
    {
        return;
    }

    _panel->changeCategory(_mainTab->getCategory());
    if (_optionsTab) _optionsTab->updateFloatingPanel(_panel->getCurrentCategory());
    _panel->show(true);
    MyGUI::LayerManager::getInstancePtr()->upLayerItem(_panel->getWidget());
    refresh();
}

void SquadAutonomyPanel::hide()
{
    _panel->show(false);
    _popupPanel->show(false);
}

bool SquadAutonomyPanel::isVisible()
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_panel) return false;
    return _panel->isVisible();
}

void SquadAutonomyPanel::selectSquad(Platoon& selectedSquad)
{
    if (!SquadAutonomySettings::getSingleton().initialized)
    {
        return;
    }

    _selectedSquad = &selectedSquad;
    _mainTab->setSelectedSquad(selectedSquad);
    _optionsTab->setSelectedSquad(selectedSquad);
}

void SquadAutonomyPanel::createModSettingsButton()
{
    MyGUI::Window* panelWindow = _panel->win->castType<MyGUI::Window>();
    MyGUI::TextBox* captionWidget = panelWindow->getCaptionWidget();
    MyGUI::Widget* header = captionWidget->getParent();
    MyGUI::Button* closeBtn = header->getChildAt(0)->castType<MyGUI::Button>();

    int left = closeBtn->getLeft();
    int top = closeBtn->getTop();
    int width = closeBtn->getWidth();
    int newWidth = width;
    int height = closeBtn->getHeight();
    auto btn = header->createWidget<MyGUI::Button>("Kenshi_Button1", left - newWidth, top, newWidth, height, MyGUI::Align::Left | MyGUI::Align::Top, "SquadAutonomySettingsBtn");
    btn->setCaption(Localization::gettext("Settings"));
    int margin = newWidth - btn->getTextRegion().width;
    newWidth = btn->getTextSize().width + margin;
    btn->setSize(newWidth, height);
    btn->setPosition(left - newWidth, top);
    captionWidget->setSize(captionWidget->getWidth() - newWidth, captionWidget->getHeight());
    btn->eventMouseButtonClick += MyGUI::newDelegate(ShowModSettings);
}


SquadAutonomyPanel::MainTab::MainTab(DatapanelGUI& panel, int tabId) :
    _panel(&panel),
    _selectedSquad(nullptr),
    _category(tabId),
    _priority(0.0f),
    _selectedAIPackageIndex(0)
{
}

void SquadAutonomyPanel::MainTab::refresh()
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_panel || !_selectedSquad)
    {
        return;
    }
    _panel->clearPage(_category);
    if (_panel->currentCategory != _category) return;

    auto squadSettings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);

    _panel->setLineSpacing(32.0);

    if (_selectedSquad->activePlatoon)
    {
        _panel->setCaption(Localization::lineSquadAutonomyTitle + ' ' + _selectedSquad->activePlatoon->getName());
    }

    auto toggleButton = Utils::addToggleButton(*_panel, _category, Localization::gettext("Enable Autonomy"), squadSettings ? squadSettings->isEnabled() : false);
    Utils::setCallback(*toggleButton, this, &MainTab::toggleAI);

    _panel->addSpace(_category, 0.25f);

    auto homeButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Set Squad Home Building"));
    Utils::setCallback(*homeButton, this, &MainTab::setHome);

    auto workButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Set Squad Work Building"));
    Utils::setCallback(*workButton, this, &MainTab::setWork);

    //auto barButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Make Building a Bar"));
    //Utils::setCallback(*barButton, this, &MainTab::setBar);

    auto clearButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Clear Buildings"));
    Utils::setCallback(*clearButton, this, &MainTab::clearBuildings);

    _panel->addSpace(_category, 0.25f);
    _panel->setLineSpacing(24.0);

    auto textBox = Utils::addText(*_panel, _category, Localization::gettext("AI Package"), true, true, MyGUI::Align::Center);

    auto editBox = _panel->setLineTextEditable(Localization::gettext("Search"), "", _category, true, false, MyGUI::Align::Left, 0.95f);
    Utils::setCallback(*editBox, this, &MainTab::changeAIPackageSearchText);

    auto dropBox = Utils::addDropBox(*_panel, _category, Localization::lineAIPackageList, _selectedAIPackageIndex, false, 1.0f, false, false);
    //auto dropBox = _panel->setLineDropBox(Localization::lineAIPackageList, _category, &_selectedAIPackageIndex, false, 1.0f);
    //dropBox->w1->setSize(dropBox->w1->getWidth(), editBox->editBox->getHeight());

    updateAIPackageList();
    //editBox->getEditBox()->setSize(dropBox->listBox->getSize());

    _panel->addSpace(_category, 0.25f);

    Utils::addSlider(*_panel, _category, Localization::gettext("Priority"), true, _priority, 0.0f, 5.0f, 0);

    _panel->addSpace(_category, 0.25f);
    _panel->setLineSpacing(32.0);

    auto addAIButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Add AI Package"));
    Utils::setCallback(*addAIButton, this, &MainTab::addAI);

    auto clearAIButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Clear AI Packages"));
    Utils::setCallback(*clearAIButton, this, &MainTab::clearAI);

    _panel->addSpace(_category, 0.25f);
    _panel->setLineSpacing(24.0);

    if (squadSettings)
    {
        if (const Building* homeBuilding = squadSettings->getBuilding(true))
        {
            Utils::addText(*_panel, _category, Localization::gettext("Home:") + ' ' + getBuildingName(*homeBuilding));
        }
        if (const Building* workBuilding = squadSettings->getBuilding(false))
        {
            Utils::addText(*_panel, _category, Localization::gettext("Work:") + ' ' + getBuildingName(*workBuilding));
        }

        const auto& squadPackages = squadSettings->getSquadPackages();

        for (auto it = squadPackages.begin(); it != squadPackages.end(); ++it)
        {
            auto packages = it->second;
            for (int i = 0; i < packages.size(); ++i)
            {
                Utils::addText(*_panel, _category, Ogre::StringConverter::toString(it->first) + " " + packages[i]->name);
            }
        }
    }
    //textbox = this->_panel->setLineText("", "", this->_category, true, MyGUI::Align::Left); //buffer text for scrolling
    //this->_panel->addSpace(this->_category, 6.0f);
}

int SquadAutonomyPanel::MainTab::getCategory() const
{
    return _category;
}

void SquadAutonomyPanel::MainTab::setSelectedSquad(Platoon& selectedSquad)
{
    _selectedSquad = &selectedSquad;
}

void SquadAutonomyPanel::MainTab::toggleAI(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized)
    {
        return;
    }

    if (!_selectedSquad || !_selectedSquad->activePlatoon)
    {
        ou->showPlayerAMessage(Localization::lineInvalidSquad, true);
        return;
    }

    if (!line || line->classType != DataPanelLine::DPL_BUTTON)
    {
        return;
    }

    auto settings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
    auto button = dynamic_cast<DataPanelLine_Button*>(line)->button;

    if (settings && settings->enableAutonomy(!button->getStateSelected()))
    {
        refresh();

        const char* enableDisableText = settings->isEnabled()
            ? ": Autonomy enable"
            : ": Autonomy disable";

        ou->showPlayerAMessage(_selectedSquad->activePlatoon->getName() + Localization::gettext(enableDisableText), true);
    }
}

void SquadAutonomyPanel::MainTab::addAI(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized)
    {
        return;
    }

    if (!_selectedSquad)
    {
        ou->showPlayerAMessage(Localization::lineInvalidSquad, true);
        return;
    }

    SquadAutonomySettings& settings = SquadAutonomySettings::getSingleton();
    SquadSettingsInfo* squadSettings = settings.getSquadSettings(_selectedSquad, true);
    if (!squadSettings)
    {
        return;
    }

    GameData* data = nullptr;
    if (_selectedAIPackageIndex < settings.getAIPackageList()->size())
    {
        data = settings.getAIPackageList()->at(_selectedAIPackageIndex);
    }

    if (!data)
    {
        return;
    }

    squadSettings->addPackage(static_cast<int>(_priority), data);
    //DebugLog("Add " + data->name + " package");
    squadSettings->updateSquadPackages();

    refresh();
}

void SquadAutonomyPanel::MainTab::clearAI(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized)
    {
        return;
    }

    if (!_selectedSquad)
    {
        ou->showPlayerAMessage(Localization::lineInvalidSquad, true);
        return;
    }

    auto settings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
    if (!settings)
    {
        return;
    }

    settings->clearPackages();
    settings->updateSquadPackages();

    refresh();
}

void SquadAutonomyPanel::MainTab::setHome(DataPanelLine* line)
{
    setBuilding(true);
}

void SquadAutonomyPanel::MainTab::setWork(DataPanelLine* line)
{
    setBuilding(false);
}

void SquadAutonomyPanel::MainTab::setBar(DataPanelLine* line)
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

void SquadAutonomyPanel::MainTab::clearBuildings(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized)
    {
        return;
    }

    if (!_selectedSquad)
    {
        ou->showPlayerAMessage(Localization::lineInvalidSquad, true);
        return;
    }

    auto settings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
    settings->setBuilding(nullptr, true);
    settings->setBuilding(nullptr, false);
    settings->unassignSquadHome();
    ou->showPlayerAMessage(Localization::gettext("Buildings cleared"), true);

    refresh();
}

void SquadAutonomyPanel::MainTab::changeAIPackageSearchText(DataPanelLine* line)
{
    if (!line || line->classType != DataPanelLine::DPL_TEXT_EDIT)
    {
        return;
    }

    std::string keyword = dynamic_cast<DataPanelLine_TextEditable*>(line)->editBox->getOnlyText();

    updateAIPackageList(keyword);
}

void SquadAutonomyPanel::MainTab::setBuilding(bool home)
{
    if (!SquadAutonomySettings::getSingleton().initialized)
    {
        return;
    }

    if (!_selectedSquad || !_selectedSquad->activePlatoon)
    {
        ou->showPlayerAMessage(Localization::lineInvalidSquad, true);
        return;
    }

    if (!gui->selectedObject)
    {
        ou->showPlayerAMessage(Localization::gettext("No object selected!"), true);
        return;
    }

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

    auto settings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
    //_selectedSquad->me->getOwnerships()->setHomeBuilding(building, _selectedSquad->me->getSquadType());
    settings->setBuilding(building, home);

    // TODO: combine "set" and the rest of the localized line; not guaranteed current parts will combine well in all languages
    std::string report = Localization::gettext("Set") + ' ' + _selectedSquad->activePlatoon->getName();
    if (home)
    {
        report += ' ' + Localization::gettext("home building:") + ' ' + building->displayName;
    }
    else
    {
        report += ' ' + Localization::gettext("work building:") + ' ' + building->displayName;
    }

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

void SquadAutonomyPanel::MainTab::updateAIPackageList(const std::string& keyword)
{
    if (!SquadAutonomySettings::getSingleton().initialized)
    {
        return;
    }

    auto dropBox = dynamic_cast<DataPanelLine_DropBox*>(_panel->getLine(Localization::lineAIPackageList, _category));
    if (!dropBox)
    {
        return;
    }

    int currentSelected = _selectedAIPackageIndex;
    if (currentSelected < 0)
    {
        currentSelected = 0;
    }

    int selectVal = -1;
    dropBox->clearValues();

    SquadAutonomySettings& settings = SquadAutonomySettings::getSingleton();

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
        // TODO: can these names be non-ascii? if so, upper case conversion will fail

        std::string keywordUpper = keyword;
        std::transform(keywordUpper.begin(), keywordUpper.end(), keywordUpper.begin(), [](char c) { return std::toupper(c); });

        for (uint32_t i = 0; i < settings.getAIPackageList()->size(); ++i)
        {
            const std::string& name = settings.getAIPackageList()->at(i)->name;
            std::string nameUpper = name;
            std::transform(nameUpper.begin(), nameUpper.end(), nameUpper.begin(), [](char c) { return std::toupper(c); });

            if (nameUpper.find(keywordUpper) != std::string::npos)
            {
                dropBox->addAValue(name, i);
                if (selectVal == -1 || i == currentSelected)
                {
                    selectVal = i;
                }
            }
        }
    }

    dropBox->setSelectedValue(selectVal);
}

std::string SquadAutonomyPanel::MainTab::getBuildingName(const Building& building)
{
    std::string name = building.displayName;

    TownBase* town = building.getTown();
    if (town)
    {
        name += ", " + town->getKnownName();
    }

    return name;
}


SquadAutonomyPanel::OptionsTab::OptionsTab(DatapanelGUI& panel, DatapanelGUI& popupPanel, int category) :
    _panel(&panel),
    _floatingPanel(nullptr),
    _selectedSquad(nullptr),
    _category(category),
    _subCategory(0),
    _basic(new BasicOptions(*this, panel, popupPanel, category)),
    _labour(new LabourOptions(*this, panel, popupPanel, category)),
    _combat(new CombatOptions(*this, panel, popupPanel, category)),
    _guard(new GuardOptions(*this, panel, popupPanel, category)),
    _shopping(new ShoppingOptions(*this, panel, popupPanel, category)),
    _inventory(new InventoryOptions(*this, panel, popupPanel, category)),
    _travel(new TravelOptions(*this, panel, popupPanel, category))
{
}

// TODO: Fix scrolling bar staying after changing subcategory
// The scrollbar is gone after switching back to main tab so look there
void SquadAutonomyPanel::OptionsTab::refresh()
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_panel || !_selectedSquad)
    {
        return;
    }

    _panel->clearPage(_category);

    if (_panel->currentCategory != _category) return;

    auto settings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
    if (!settings)
    {
        // Should not happen, since createNew = true
        return;
    }

    _panel->setLineSpacing(24.0);
    

    if (_selectedSquad->activePlatoon)
    {
        _panel->setCaption(Localization::lineSquadAutonomyTitle + ' ' + _selectedSquad->activePlatoon->getName());
    }

    if (!_floatingPanel)
    {
        _floatingPanel = _panel->win->createWidgetReal<MyGUI::Widget>("Kenshi_GenericWindowSkin", 0.0, float(_panel->tabWin->getBottom()) / _panel->getHeight(), 1.0, 0.0, MyGUI::Align::Top | MyGUI::Align::Left, "");
        _floatingPanel->setWidgetStyle(MyGUI::WidgetStyle::Overlapped);
        _floatingPanel->setVisible(false);
    }
    
    auto dropBox = _panel->setLineDropBox(Localization::gettext("Category"), _category, &_subCategory, false, 1.0f);
    _panel->addSpace(_category, 0.25);
    _floatingPanel->setSize(_floatingPanel->getWidth(), _panel->getContentHeight(_category));

    for (int i = 0; i < dropBox->getNumWidgets(); ++i)
    {
        MyGUI::Widget* widget = dropBox->getWidget(i);
        float left = float(widget->getLeft()) / widget->getParent()->getWidth();
        float top = 0.5 - float(widget->getHeight()) / _floatingPanel->getHeight() * 0.5;
        widget->attachToWidget(_floatingPanel);
        widget->setRealPosition(left, top);
    }


    if (_subCategory < 0)
    {
        _subCategory = 0;
    }
    else if (_subCategory > 6)
    {
        _subCategory = 6;
    }

    dropBox->addAValue(Localization::gettext("Basic"), 0);
    dropBox->addAValue(Localization::gettext("Labour"), 1);
    dropBox->addAValue(Localization::gettext("Combat"), 2);
    dropBox->addAValue(Localization::gettext("Guard"), 3);
    dropBox->addAValue(Localization::gettext("Shopping"), 4);
    dropBox->addAValue(Localization::gettext("Inventory"), 5);
    dropBox->addAValue(Localization::gettext("Travel"), 6);
    dropBox->setSelectedValue(_subCategory);

    dropBox->getComboBox()->eventComboAccept += MyGUI::newDelegate(this, &SquadAutonomyPanel::OptionsTab::refreshCallback);
    readFromSquadSettings(*settings);
    bool hasSettings = false;
    switch (_subCategory)
    {
    case 1:
        hasSettings = _labour->refresh(*settings);
        break;
    case 2:
        hasSettings = _combat->refresh(*settings);
        break;
    case 3:
        hasSettings = _guard->refresh(*settings);
        break;
    case 4:
        hasSettings = _shopping->refresh(*settings);
        break;
    case 5:
        hasSettings = _inventory->refresh(*settings);
        break;
    case 6:
        hasSettings = _travel->refresh(*settings);
        break;
    default:
        hasSettings = _basic->refresh(*settings);
        break;
    }
    if (!hasSettings)
    {
        Utils::addText(*_panel, _category, *_MainColorCode + Localization::gettext("No settings available for current packages"), false, false);
    }
}

int SquadAutonomyPanel::OptionsTab::getCategory() const
{
    return _category;
}

MyGUI::Widget* SquadAutonomyPanel::OptionsTab::getFloatingPanelPtr()
{
    return _floatingPanel;
}

void SquadAutonomyPanel::OptionsTab::setFloatingPanelPtr(MyGUI::Widget* ptr)
{
    _floatingPanel = ptr;
}

void SquadAutonomyPanel::OptionsTab::setSelectedSquad(Platoon& selectedSquad)
{
    _selectedSquad = &selectedSquad;
}

void SquadAutonomyPanel::OptionsTab::refreshCallback(MyGUI::ComboBox* sender, size_t index)
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_selectedSquad)
    {
        return;
    }
    _popupPanel->show(false);
    refresh();
}

void SquadAutonomyPanel::OptionsTab::writeToSelectedSquadSettings(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_selectedSquad)
    {
        return;
    }

    auto squadSettings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);

    _basic->writeToSquadSettings(*squadSettings);
    _labour->writeToSquadSettings(*squadSettings);
    _combat->writeToSquadSettings(*squadSettings);
    _guard->writeToSquadSettings(*squadSettings);
    _shopping->writeToSquadSettings(*squadSettings);
    _inventory->writeToSquadSettings(*squadSettings);
    _travel->writeToSquadSettings(*squadSettings);
}

void SquadAutonomyPanel::OptionsTab::writeToSelectedSquadSettingsAndRefresh(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_selectedSquad)
    {
        return;
    }

    writeToSelectedSquadSettings(line);
    refresh();
}

void SquadAutonomyPanel::OptionsTab::readFromSquadSettings(const SquadSettingsInfo& squadSettings)
{
    _basic->readFromSquadSettings(squadSettings);
    _labour->readFromSquadSettings(squadSettings);
    _combat->readFromSquadSettings(squadSettings);
    _guard->readFromSquadSettings(squadSettings);
    _shopping->readFromSquadSettings(squadSettings);
    _inventory->readFromSquadSettings(squadSettings);
    _travel->readFromSquadSettings(squadSettings);
}


SquadAutonomyPanel::OptionsTab::OptionsBase::OptionsBase(OptionsTab &optionsTab, DatapanelGUI &panel, DatapanelGUI& popupPanel, int category) :
    _optionsTab(&optionsTab),
    _panel(&panel),
    _popupPanel(&popupPanel),
    _category(category)
{
}


SquadAutonomyPanel::OptionsTab::BasicOptions::BasicOptions(OptionsTab& optionsTab, DatapanelGUI& panel, DatapanelGUI& popupPanel, int category) :
    OptionsBase(optionsTab, panel, popupPanel, category),
    startWorkTime(0.0f),
    endWorkTime(24.0f),
    autoMovementSpeed(false),
    doSleep(false),
    restUntilHealed(true),
    restThreshold(50.0),
    healedThreshold(90.0),
    usePaidBeds(false)
{
}

bool SquadAutonomyPanel::OptionsTab::BasicOptions::refresh(SquadSettingsInfo& settings)
{
    auto startSlider = Utils::addSlider(*_panel, _category, Localization::gettext("Start Work Time"), true, startWorkTime, 0.0f, 24.0f, 0);
    Utils::setCallback(*startSlider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

    auto endSlider = Utils::addSlider(*_panel, _category, Localization::gettext("End Work Time"), true, endWorkTime, 0.0f, 24.0f, 0);
    Utils::setCallback(*endSlider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

    auto checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Auto Movement Speed"), autoMovementSpeed);
    Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

    if (settings.hasTask(GO_HOME_AND_GO_TO_BED) || settings.hasTask(GO_HOME_AND_GO_TO_BED_SECURE))
    {
        checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Rest Until Healed When Wounded"), restUntilHealed);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettingsAndRefresh);

        if (restUntilHealed)
        {
            Utils::addText(*_panel, _category, Localization::gettext("Rest Health Threshold"));

            auto restThresholdSlider = Utils::addSlider(*_panel, _category, Localization::gettext("Rest Health Threshold"), false, restThreshold, 0.0f, 100.0f, 0);
            Utils::setCallback(*restThresholdSlider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

            Utils::addText(*_panel, _category, Localization::gettext("Healed Health Threshold"));

            auto healthThresholdSlider = Utils::addSlider(*_panel, _category, Localization::gettext("Healed Health Threshold"), false, healedThreshold, 0.0f, 100.0f, 0);
            Utils::setCallback(*healthThresholdSlider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        }

        checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Allow Using Paid Beds"), usePaidBeds);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

        checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Allow Sleep Outside Work Hours"), doSleep);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
    }
    return true;
}

void SquadAutonomyPanel::OptionsTab::BasicOptions::readFromSquadSettings(const SquadSettingsInfo& squadSettings)
{
    startWorkTime = squadSettings.getStartWorkTime();
    endWorkTime = squadSettings.getEndWorkTime();
    autoMovementSpeed = squadSettings.getAutoMovementSpeed();
    doSleep = squadSettings.getDoSleep();
    restUntilHealed = squadSettings.getRestUntilHealed();
    restThreshold = squadSettings.getRestThreshold();
    healedThreshold = squadSettings.getHealedThreshold();
    usePaidBeds = squadSettings.getUsePaidBeds();
}

void SquadAutonomyPanel::OptionsTab::BasicOptions::writeToSquadSettings(SquadSettingsInfo& squadSettings) const
{
    squadSettings.setStartWorkTime(startWorkTime);
    squadSettings.setEndWorkTime(endWorkTime);
    squadSettings.setAutoMovementSpeed(autoMovementSpeed);
    squadSettings.setDoSleep(doSleep);
    squadSettings.setRestUntilHealed(restUntilHealed);
    squadSettings.setRestThreshold(restThreshold);
    squadSettings.setHealedThreshold(healedThreshold);
    squadSettings.setUsePaidBeds(usePaidBeds);
}


SquadAutonomyPanel::OptionsTab::LabourOptions::LabourOptions(OptionsTab& optionsTab, DatapanelGUI& panel, DatapanelGUI& popupPanel, int category) :
    OptionsBase(optionsTab, panel, popupPanel, category),
    setAll(0.0),
    science(1.0),
    labouring(1.0),
    farming(1.0),
    medic(1.0),
    cooking(1.0),
    engineer(1.0),
    robotics(1.0),
    weaponSmith(1.0),
    armourSmith(1.0),
    crossbowSmith(1.0),
    automaticMachine(1.0),
    other(1.0)
{
}

bool SquadAutonomyPanel::OptionsTab::LabourOptions::refresh(SquadSettingsInfo& settings)
{
    bool hasTask = false;
    if (settings.hasTask(AUTO_LABOURING_MINES))
    {
        hasTask = true;

        _panel->setLineSpacing(28.0);

        const std::string statsUsedLabel = *_MainColorCode + Localization::gettext("Stats Used:") + ' ' + Localization::gettext("(bigger number = higher priority, 0 = forbid)");
        Utils::addText(*_panel, _category, statsUsedLabel, false, false);

        const float maxPriority = 10.0;
        auto slider = Utils::addSlider(*_panel, _category, Localization::gettextMain("Science"), true, science, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        slider = Utils::addSlider(*_panel, _category, Localization::gettextMain("Labouring")+ " " + Localization::gettextMain("Mining"), true, labouring, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        slider = Utils::addSlider(*_panel, _category, Localization::gettextMain("Farming"), true, farming, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        slider = Utils::addSlider(*_panel, _category, Localization::gettextMain("Cooking"), true, cooking, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        slider = Utils::addSlider(*_panel, _category, Localization::gettextMain("Medic"), true, medic, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        slider = Utils::addSlider(*_panel, _category, Localization::gettextMain("Engineer"), true, engineer, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        slider = Utils::addSlider(*_panel, _category, Localization::gettextMain("Robotics"), true, robotics, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        slider = Utils::addSlider(*_panel, _category, Localization::gettextMain("Weapon Smithing"), true, weaponSmith, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        slider = Utils::addSlider(*_panel, _category, Localization::gettextMain("Armour Smithing"), true, armourSmith, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        slider = Utils::addSlider(*_panel, _category, Localization::gettextMain("Crossbow Smithing"), true, crossbowSmith, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        slider = Utils::addSlider(*_panel, _category, Localization::gettext("Automatic Machine"), true, automaticMachine, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        slider = Utils::addSlider(*_panel, _category, Localization::gettext("Other"), true, other, 0.0, maxPriority, 0, true);
        Utils::setCallback(*slider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

        _panel->addSpace(_category, 0.25);

        // set all button
        slider = _panel->setLineSliderEditable(Localization::gettext("Set All"), _category, true, 0.0, maxPriority, &setAll); 
        slider->setPrecision(0);
        auto editBox = slider->valueEditBox;
        auto valueEditPos = editBox->getPosition();
        auto button = _panel->setLineButton("", Localization::gettext("Set"), _category);
        button->button->setPosition(valueEditPos.left, valueEditPos.top + editBox->getHeight());
        button->button->setSize(editBox->getWidth(), editBox->getHeight());
        slider->sliderBar->setPosition(slider->sliderBar->getLeft(), (editBox->getTop() + editBox->getHeight()) - slider->sliderBar->getHeight() * 0.5);
        slider->nameText->setPosition(slider->nameText->getLeft(), (editBox->getTop() + editBox->getHeight()) - slider->nameText->getHeight() * 0.5);
        //button->button->setCaption(Localization::gettext("Set"));
        button->button->setUserData(_optionsTab);
        button->button->eventMouseButtonClick += MyGUI::newDelegate(this, &LabourOptions::setAllTo);
        _panel->addSpace(_category, 1.0);
    }
    return hasTask;
}

void SquadAutonomyPanel::OptionsTab::LabourOptions::setAllTo(MyGUI::Widget* sender)
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

    auto optionsTab = *(sender->getUserData<OptionsTab*>());
    if (optionsTab)
    {
        optionsTab->writeToSelectedSquadSettingsAndRefresh(nullptr);
    }
}

void SquadAutonomyPanel::OptionsTab::LabourOptions::readFromSquadSettings(const SquadSettingsInfo& squadSettings)
{
    science = squadSettings.getLabourScience();
    labouring = squadSettings.getLabourLabouring();
    farming = squadSettings.getLabourFarming();
    cooking = squadSettings.getLabourCooking();
    medic = squadSettings.getLabourMedic();
    engineer = squadSettings.getLabourEngineer();
    robotics = squadSettings.getLabourRobotics();
    weaponSmith = squadSettings.getLabourWeaponSmith();
    armourSmith = squadSettings.getLabourArmourSmith();
    crossbowSmith = squadSettings.getLabourCrossbowSmith();
    automaticMachine = squadSettings.getLabourAutomaticMachine();
    other = squadSettings.getLabourOther();
}

void SquadAutonomyPanel::OptionsTab::LabourOptions::writeToSquadSettings(SquadSettingsInfo& squadSettings) const
{
    squadSettings.setLabourScience(science);
    squadSettings.setLabourLabouring(labouring);
    squadSettings.setLabourFarming(farming);
    squadSettings.setLabourCooking(cooking);
    squadSettings.setLabourMedic(medic);
    squadSettings.setLabourEngineer(engineer);
    squadSettings.setLabourRobotics(robotics);
    squadSettings.setLabourWeaponSmith(weaponSmith);
    squadSettings.setLabourArmourSmith(armourSmith);
    squadSettings.setLabourCrossbowSmith(crossbowSmith);
    squadSettings.setLabourAutomaticMachine(automaticMachine);
    squadSettings.setLabourOther(other);
}


SquadAutonomyPanel::OptionsTab::CombatOptions::CombatOptions(OptionsTab& optionsTab, DatapanelGUI& panel, DatapanelGUI& popupPanel, int category) :
    OptionsBase(optionsTab, panel, popupPanel, category),
    attackEnemies(true),
    protectAllies(true),
    doMedic(true),
    doRescue(true),
    rescueOwnSquadOnly(false)
{
}

bool SquadAutonomyPanel::OptionsTab::CombatOptions::refresh(SquadSettingsInfo& settings)
{
    bool hasTask = false;
    DataPanelLine_CheckBox* checkbox; 
    if (settings.hasTask(TERRITORIAL_AGGRESSION_BUT_DONT_LEAVE_HOME) || settings.hasTask(ATTACK_ENEMIES))
    {
        checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Attack Enemies"), attackEnemies);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        hasTask = true;
    }
    if (settings.hasTask(PROTECT_ALLIES) || settings.hasTask(PROTECT_ALLIES_STAY_IN_TOWN) || settings.hasTask(PROTECT_OWN_SQUAD))
    {
        checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Protect Allies"), protectAllies);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        hasTask = true;
    }
    if (settings.hasTask(JOB_MEDIC) || settings.hasTask(JOB_REPAIR_ROBOT))
    {
        checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Do Medic"), doMedic);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        hasTask = true;
    }
    if (settings.hasTask(FIND_AND_RESCUE) || settings.hasTask(FIND_AND_RESCUE_LEADER) || settings.hasTask(FIND_AND_RESCUE_IF_THERES_BEDS))
    {
        checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Rescue Downed Allies"), doRescue);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettingsAndRefresh);
        if (doRescue)
        {
            checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Rescue Own Squad Only"), rescueOwnSquadOnly);
            Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        }
        hasTask = true;
    }
    return hasTask;
}

void SquadAutonomyPanel::OptionsTab::CombatOptions::readFromSquadSettings(const SquadSettingsInfo& squadSettings)
{
    attackEnemies = squadSettings.getAttackEnemies();
    protectAllies = squadSettings.getProtectAllies();
    doMedic = squadSettings.getDoMedic();
    doRescue = squadSettings.getDoRescue();
    rescueOwnSquadOnly = squadSettings.getRescueOwnSquadOnly();
}

void SquadAutonomyPanel::OptionsTab::CombatOptions::writeToSquadSettings(SquadSettingsInfo& squadSettings) const
{
    squadSettings.setAttackEnemies(attackEnemies);
    squadSettings.setProtectAllies(protectAllies);
    squadSettings.setDoMedic(doMedic);
    squadSettings.setDoRescue(doRescue);
    squadSettings.setRescueOwnSquadOnly(rescueOwnSquadOnly);
}


SquadAutonomyPanel::OptionsTab::GuardOptions::GuardOptions(OptionsTab& optionsTab, DatapanelGUI& panel, DatapanelGUI& popupPanel, int category) :
    OptionsBase(optionsTab, panel, popupPanel, category),
    manTurrets(false),
    stayInsideGate(false),
    closeGate(false)
{
}

bool SquadAutonomyPanel::OptionsTab::GuardOptions::refresh(SquadSettingsInfo& settings)
{
    bool hasTask = false;
    if (settings.hasTask(MAN_A_TURRET) || settings.hasTask(MAN_A_TURRET_ON_BUILDING))
    {
        auto checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Man Turrets"), manTurrets);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        hasTask = true;
    }
    if (settings.hasTask(MAN_THE_GATE))
    {
        Utils::addText(*_panel, _category, *_MainColorCode + Localization::gettext("Gate Guard:"), false, false);

        auto checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Stay Inside Gate"), stayInsideGate);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

        checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Close Gate"), closeGate);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
        hasTask = true;
    }
    return hasTask;
}

void SquadAutonomyPanel::OptionsTab::GuardOptions::readFromSquadSettings(const SquadSettingsInfo& squadSettings)
{
    manTurrets = squadSettings.getManTurrets();
    stayInsideGate = squadSettings.getStayInsideGate();
    closeGate = squadSettings.getCloseGate();
}

void SquadAutonomyPanel::OptionsTab::GuardOptions::writeToSquadSettings(SquadSettingsInfo& squadSettings) const
{
    squadSettings.setManTurrets(manTurrets);
    squadSettings.setStayInsideGate(stayInsideGate);
    squadSettings.setCloseGate(closeGate);
    squadSettings.updateSquadPackages();
}

const ItemFunction itemFunctions[] =
{
    ITEM_NO_FUNCTION,
    ITEM_FIRSTAID,
    ITEM_MEDRIGGING,
    ITEM_FOOD,
    ITEM_CONTAINER,
    ITEM_WEAPON,
    ITEM_CLOTHING,
    // skip
    ITEM____, 
    ITEM_NARCOTIC,
    ITEM_TOOL,
    ITEM_ANYTHING,
    ITEM_BLUEPRINT,
    ITEM_ROBOTREPAIR,
    ITEM_BOOK,
    // skip
    ITEM_MONEY, 
    ITEM_FOOD_RESTRICTED,
    ITEM_AMMO,
    ITEM_SEVERED_LIMB
};

SquadAutonomyPanel::OptionsTab::ShoppingOptions::ShoppingOptions(OptionsTab& optionsTab, DatapanelGUI& panel, DatapanelGUI& popupPanel, int category):
    OptionsBase(optionsTab, panel, popupPanel, category),
    itemFunctionListIndex(ITEM_FIRSTAID),
    amount(0)
{
}

std::string getItemFunctionName(ItemFunction itemFunc)
{
    std::string name = "";
    switch (itemFunc)
    {
    /*
    case ITEM_ANYTHING:
        name = Localization::gettext("Anything");
        break;
    */
    case ITEM_FIRSTAID:
        name = Localization::gettext("First Aid");
        break;
    case ITEM_ROBOTREPAIR:
        name = Localization::gettext("Skeleton Repair Kit");
        break;
    case ITEM_MEDRIGGING:
        name = Localization::gettext("Splinting");
        break;
    case ITEM_CONTAINER:
        name = Localization::gettext("Backpack");
        break;
    case ITEM_FOOD:
        name = Localization::gettext("Food");
        break;
    case ITEM_WEAPON:
        name = Localization::gettext("Weapon");
        break;
    case ITEM_AMMO:
        name = Localization::gettext("Ammunition");
        break;
    case ITEM_CLOTHING:
        name = Localization::gettext("Clothing");
        break;
    case ITEM_NARCOTIC:
        name = Localization::gettext("Narcotic");
        break;
    case ITEM_BLUEPRINT:
        name = Localization::gettext("Blueprint");
        break;
    case ITEM_BOOK:
        name = Localization::gettext("Book");
        break;
    case ITEM_NO_FUNCTION:
        name = Localization::gettext("Material");
        break;
    }
    return name;
}

void updateItemFunctionList(DataPanelLine_DropBox* dropBox)
{
    //dropBox->addAValue(Localization::gettext("Anything"), ITEM_ANYTHING);
    dropBox->addAValue(getItemFunctionName(ITEM_FIRSTAID), ITEM_FIRSTAID);
    dropBox->addAValue(getItemFunctionName(ITEM_ROBOTREPAIR), ITEM_ROBOTREPAIR);
    dropBox->addAValue(getItemFunctionName(ITEM_MEDRIGGING), ITEM_MEDRIGGING);
    dropBox->addAValue(getItemFunctionName(ITEM_CONTAINER), ITEM_CONTAINER);
    dropBox->addAValue(getItemFunctionName(ITEM_FOOD), ITEM_FOOD);
    dropBox->addAValue(getItemFunctionName(ITEM_WEAPON), ITEM_WEAPON);
    dropBox->addAValue(getItemFunctionName(ITEM_AMMO), ITEM_AMMO);
    dropBox->addAValue(getItemFunctionName(ITEM_CLOTHING), ITEM_CLOTHING);
    dropBox->addAValue(getItemFunctionName(ITEM_NARCOTIC), ITEM_NARCOTIC);
    dropBox->addAValue(getItemFunctionName(ITEM_BLUEPRINT), ITEM_BLUEPRINT);
    dropBox->addAValue(getItemFunctionName(ITEM_BOOK), ITEM_BOOK);
    dropBox->addAValue(getItemFunctionName(ITEM_NO_FUNCTION), ITEM_NO_FUNCTION);
}

bool SquadAutonomyPanel::OptionsTab::ShoppingOptions::refresh(SquadSettingsInfo& settings)
{
    bool hasTask = false;
    if (settings.hasTask(SHOPPING))
    {
        hasTask = true;
        auto shoppingBox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Do Shopping"), doShopping);
        Utils::setCallback(*shoppingBox, _optionsTab, &OptionsTab::writeToSelectedSquadSettingsAndRefresh);
        if (doShopping)
        {
            auto dropBox = Utils::addDropBox(*_panel, _category, Localization::lineItemCategory, itemFunctionListIndex, false, 1.0f, false, true);
            updateItemFunctionList(dropBox);
            dropBox->setSelectedValue(itemFunctionListIndex);
            Utils::addSlider(*_panel, _category, Localization::gettext("Min Amount"), true, amount, 0.0f, 50.0f, 0);
            auto addBuyItemButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Add Buy Item"));
            Utils::setCallback(*addBuyItemButton, this, &OptionsTab::ShoppingOptions::addBuyItem);
            auto addSellItemButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Add Sell Item"));
            Utils::setCallback(*addSellItemButton, this, &OptionsTab::ShoppingOptions::addSellItem);

            auto buyItems = settings.getBuyItems();
            Utils::addText(*_panel, _category, *_MainColorCode + Localization::gettext("Buying:"), false, false);
            auto clearBuyItemButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Clear Buy Items"));
            Utils::setCallback(*clearBuyItemButton, this, &OptionsTab::ShoppingOptions::clearBuyItem);

            for (auto iter = buyItems.begin(); iter != buyItems.end(); ++iter)
            {
                //Utils::addText(*_panel, _category, Localization::gettext("Buy") + " " + getItemFunctionName(iter->first) + Localization::gettext(": until >=") + " " + Ogre::StringConverter::toString(iter->second));
                auto button = Utils::addTextButton(*_panel, _category, Localization::gettext("Buy") + " " + getItemFunctionName(iter->first) + Localization::gettext(": until >=") + " " + Ogre::StringConverter::toString(iter->second.quantity));
                button->button->setUserData(*iter);
                button->button->eventMouseButtonClick += MyGUI::newDelegate(this, &OptionsTab::ShoppingOptions::openShopItemDetail);
            }
            auto sellItems = settings.getSellItems();
            Utils::addText(*_panel, _category, *_MainColorCode + Localization::gettext("Selling:"), false, false);
            auto clearSellItemButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Clear Sell Item"));
            Utils::setCallback(*clearSellItemButton, this, &OptionsTab::ShoppingOptions::clearSellItem);
            for (auto iter = sellItems.begin(); iter != sellItems.end(); ++iter)
            {
                //Utils::addText(*_panel, _category, Localization::gettext("Sell") + " " + getItemFunctionName(iter->first) + Localization::gettext(": until <=") + " " + Ogre::StringConverter::toString(iter->second));
                auto button = Utils::addTextButton(*_panel, _category, Localization::gettext("Sell") + " " + getItemFunctionName(iter->first) + Localization::gettext(": until <=") + " " + Ogre::StringConverter::toString(iter->second.quantity));
                button->button->setUserData(*iter);
                button->button->eventMouseButtonClick += MyGUI::newDelegate(this, &OptionsTab::ShoppingOptions::openShopItemDetail);
            }
        }
    }
    return hasTask;
}

void SquadAutonomyPanel::OptionsTab::ShoppingOptions::addBuyItem(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_optionsTab->_selectedSquad)
    {
        return;
    }
    auto dropBox = dynamic_cast<DataPanelLine_DropBox*>(_panel->getLine(Localization::lineItemCategory, _category));
    if (!dropBox)
    {
        return;
    }
    auto squadSettings = SquadAutonomySettings::getSingleton().getSquadSettings(_optionsTab->_selectedSquad, true);

    if (squadSettings)
    {
        if (amount < 1)
        {
            ou->showPlayerAMessage(Localization::gettext("Buy amount must be higher than 0"), false);
            return;
        }
        ItemFunction itemFunction = static_cast<ItemFunction>(dropBox->getSelectedValue());
        auto sellItems = squadSettings->getSellItems();
        int buyAmount = (int)amount;
        if (sellItems.find(itemFunction) != sellItems.end())
        {
            int oldSellAmount = sellItems[itemFunction].quantity;
            squadSettings->addSellItem(itemFunction, std::max(buyAmount, oldSellAmount));
        }
        squadSettings->addBuyItem(itemFunction, buyAmount);
    }
    
    _optionsTab->refresh();
}

void SquadAutonomyPanel::OptionsTab::ShoppingOptions::addSellItem(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_optionsTab->_selectedSquad)
    {
        return;
    }
    auto dropBox = dynamic_cast<DataPanelLine_DropBox*>(_panel->getLine(Localization::lineItemCategory, _category));
    if (!dropBox)
    {
        return;
    }
    auto squadSettings = SquadAutonomySettings::getSingleton().getSquadSettings(_optionsTab->_selectedSquad, true);

    if (squadSettings)
    {
        ItemFunction itemFunction = static_cast<ItemFunction>(dropBox->getSelectedValue());
        auto buyItems = squadSettings->getBuyItems();
        int sellAmount = (int)amount;
        if (buyItems.find(itemFunction) != buyItems.end())
        {
            sellAmount = std::max(sellAmount, buyItems[itemFunction].quantity);
        }
        squadSettings->addSellItem(itemFunction, sellAmount);
    }

    _optionsTab->refresh();
}

void SquadAutonomyPanel::OptionsTab::ShoppingOptions::clearBuyItem(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_optionsTab->_selectedSquad)
    {
        return;
    }

    auto squadSettings = SquadAutonomySettings::getSingleton().getSquadSettings(_optionsTab->_selectedSquad, true);

    if (squadSettings)
    {
        auto tradeItems = squadSettings->getBuyItemsPtr();
        tradeItems->clear();
    }

    _optionsTab->refresh();
}

void SquadAutonomyPanel::OptionsTab::ShoppingOptions::clearSellItem(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_optionsTab->_selectedSquad)
    {
        return;
    }

    auto squadSettings = SquadAutonomySettings::getSingleton().getSquadSettings(_optionsTab->_selectedSquad, true);

    if (squadSettings)
    {
        auto tradeItems = squadSettings->getSellItemsPtr();
        tradeItems->clear();
    }

    _optionsTab->refresh();
}

void SquadAutonomyPanel::OptionsTab::ShoppingOptions::openShopItemDetail(MyGUI::Widget* sender)
{
    if (_popupPanel)
    {
        _popupPanel->show(true);
        std::pair<const ItemFunction, int> item = *sender->getUserData<std::pair<const ItemFunction, int>>();
        _popupPanel->setCaption(getItemFunctionName(item.first));
        _popupPanel->clear();
    }
}

void SquadAutonomyPanel::OptionsTab::ShoppingOptions::readFromSquadSettings(const SquadSettingsInfo& squadSettings)
{
    doShopping = squadSettings.getDoShopping();
}

void SquadAutonomyPanel::OptionsTab::ShoppingOptions::writeToSquadSettings(SquadSettingsInfo& squadSettings) const
{
    squadSettings.setDoShopping(doShopping);
}

SquadAutonomyPanel::OptionsTab::InventoryOptions::InventoryOptions(OptionsTab& optionsTab, DatapanelGUI& panel, DatapanelGUI& popupPanel, int category) :
    OptionsBase(optionsTab, panel, popupPanel, category),
    doLooting(true),
    putLootsInStorage(true),
    ignoreEquipped(true),
    itemFunctionListIndex(ITEM_NO_FUNCTION)
{
}

bool SquadAutonomyPanel::OptionsTab::InventoryOptions::refresh(SquadSettingsInfo& settings)
{
    bool hasTask = false;
    if (settings.hasTask(LOOT_THE_DEAD))
    {
        hasTask = true;
        auto lootingBox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Do Looting"), doLooting);
        Utils::setCallback(*lootingBox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
    }
    if (settings.hasTask(PUT_LOOT_IN_STORAGE))
    {
        hasTask = true;
        auto putLootsInStorageBox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Put loots in storage"), putLootsInStorage);
        Utils::setCallback(*putLootsInStorageBox, _optionsTab, &OptionsTab::writeToSelectedSquadSettingsAndRefresh);
        if (putLootsInStorage)
        {
            auto ignoreEquippedBox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Ignore equipped"), ignoreEquipped);
            Utils::setCallback(*ignoreEquippedBox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

            //auto dropBox = _panel->setLineDropBox(Localization::lineItemCategory, _category, &itemFunctionListIndex, false, 1.0f);
            auto dropBox = Utils::addDropBox(*_panel, _category, Localization::lineItemCategory, itemFunctionListIndex, false, 1.0f, false, true);
            updateItemFunctionList(dropBox);
            dropBox->setSelectedValue(itemFunctionListIndex);
            auto addForbidItemButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Add Forbid Item"));
            Utils::setCallback(*addForbidItemButton, this, &OptionsTab::InventoryOptions::addForbidItem);
            auto clearForbidItemsButton = Utils::addTextButton(*_panel, _category, Localization::gettext("Clear Forbid Items"));
            Utils::setCallback(*clearForbidItemsButton, this, &OptionsTab::InventoryOptions::clearForbidItems);
            auto forbidItems = settings.getForbidItems();
            for (auto iter = forbidItems.begin(); iter != forbidItems.end(); ++iter)
            {
                Utils::addText(*_panel, _category, getItemFunctionName(*iter), true, true, MyGUI::Align::Center);
            }
        }
    }
    return hasTask;
}

void SquadAutonomyPanel::OptionsTab::InventoryOptions::addForbidItem(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_optionsTab->_selectedSquad)
    {
        return;
    }
    auto dropBox = dynamic_cast<DataPanelLine_DropBox*>(_panel->getLine(Localization::lineItemCategory, _category));
    if (!dropBox)
    {
        return;
    }
    auto squadSettings = SquadAutonomySettings::getSingleton().getSquadSettings(_optionsTab->_selectedSquad, true);

    if (squadSettings)
    {
        ItemFunction itemFunction = static_cast<ItemFunction>(dropBox->getSelectedValue());
        squadSettings->addForbidItem(itemFunction);
    }

    _optionsTab->refresh();
}

void SquadAutonomyPanel::OptionsTab::InventoryOptions::clearForbidItems(DataPanelLine* line)
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_optionsTab->_selectedSquad)
    {
        return;
    }
    auto squadSettings = SquadAutonomySettings::getSingleton().getSquadSettings(_optionsTab->_selectedSquad, true);

    if (squadSettings)
    {
        squadSettings->clearForbidItems();
    }

    _optionsTab->refresh();
}

void SquadAutonomyPanel::OptionsTab::InventoryOptions::readFromSquadSettings(const SquadSettingsInfo& squadSettings)
{
    doLooting = squadSettings.getDoLooting();
    putLootsInStorage = squadSettings.getPutLootsInStorage();
    ignoreEquipped = squadSettings.getIgnoreEquipped();
}

void SquadAutonomyPanel::OptionsTab::InventoryOptions::writeToSquadSettings(SquadSettingsInfo& squadSettings) const
{
    squadSettings.setDoLooting(doLooting);
    squadSettings.setPutLootsInStorage(putLootsInStorage);
    squadSettings.setIgnoreEquipped(ignoreEquipped);
}

SquadAutonomyPanel::OptionsTab::TravelOptions::TravelOptions(OptionsTab& optionsTab, DatapanelGUI& panel, DatapanelGUI& popupPanel, int category):
    OptionsBase(optionsTab, panel, popupPanel, category),
    townListIndex(0)
{
}

void SquadAutonomy::SquadAutonomyPanel::OptionsTab::TravelOptions::changeTownSearchText(DataPanelLine* line)
{
    if (!line || line->classType != DataPanelLine::DPL_TEXT_EDIT)
    {
        return;
    }

    std::string keyword = dynamic_cast<DataPanelLine_TextEditable*>(line)->editBox->getOnlyText();

    updateTownList(keyword);
}

void SquadAutonomyPanel::OptionsTab::TravelOptions::updateTownList(const std::string& keyword)
{
    if (!SquadAutonomySettings::getSingleton().initialized)
    {
        return;
    }
    auto dropBox = dynamic_cast<DataPanelLine_DropBox*>(_panel->getLine(Localization::lineTownList, _category));
    if (!dropBox)
    {
        return;
    }
    // sync townlist in settings
    SquadAutonomySettings& settings = SquadAutonomySettings::getSingleton();

    int currentSelected = townListIndex;
    if (currentSelected < 0)
    {
        currentSelected = 0;
    }

    int selectVal = -1;
    dropBox->clearValues();

    auto settingsTowns = settings.getTownBaseList();
    settingsTowns->clear();
    lektor<RootObject*> townObjects = shou->townList->getAllTowns();

    std::vector<TownBase*> towns;
    for (auto iter = townObjects.begin(); iter != townObjects.end(); ++iter)
    {
        TownBase* town = dynamic_cast<TownBase*>(*iter);
        if (!town) continue;
        if (!town->isDiscovered()) continue;
        towns.push_back(town);
    }
    //sort alphabetically
    std::sort(towns.begin(), towns.end(), [](TownBase* a, TownBase* b)
    {
        return a->getKnownName() < b->getKnownName();
    });

    townObjects.clear();

    int count = 0;

    if (keyword.empty())
    {
        selectVal = currentSelected;
        for (auto iter = towns.begin(); iter != towns.end(); ++iter)
        {
            TownBase* town = dynamic_cast<TownBase*>(*iter);
            dropBox->addAValue(town->getKnownName(), count);
            settingsTowns->push_back(town);
            count++;
        }
    }
    else
    {
        std::string keywordUpper = keyword;
        std::transform(keywordUpper.begin(), keywordUpper.end(), keywordUpper.begin(), [](char c) { return std::toupper(c); });
        for (auto iter = towns.begin(); iter != towns.end(); ++iter)
        {
            TownBase* town = dynamic_cast<TownBase*>(*iter);
            const std::string& name = town->getKnownName();
            std::string nameUpper = name;
            std::transform(nameUpper.begin(), nameUpper.end(), nameUpper.begin(), [](char c) { return std::toupper(c); });
            if (nameUpper.find(keywordUpper) != std::string::npos)
            {
                dropBox->addAValue(name, count);
                settingsTowns->push_back(town);
                if (selectVal == -1 || count == currentSelected)
                {
                    selectVal = count;
                }
                count++;
            }
        }
    }

    dropBox->setSelectedValue(selectVal);
}

bool SquadAutonomy::SquadAutonomyPanel::OptionsTab::TravelOptions::refresh(SquadSettingsInfo& settings)
{
    bool hasSignal = false;
    if (settings.hasSignal(SIG_WANDERING_TOWN_TO_TOWN))
    {
        hasSignal = true;
        auto editBox = _panel->setLineTextEditable(Localization::gettext("Search"), "", _category, true, false, MyGUI::Align::Left, 0.95f);
        Utils::setCallback(*editBox, this, &TravelOptions::changeTownSearchText);
        auto dropBox = _panel->setLineDropBox(Localization::lineTownList, _category, &townListIndex, false, 1.0f);
        updateTownList();
    }
    return hasSignal;
}

void SquadAutonomy::SquadAutonomyPanel::OptionsTab::TravelOptions::readFromSquadSettings(const SquadSettingsInfo& squadSettings)
{
}

void SquadAutonomy::SquadAutonomyPanel::OptionsTab::TravelOptions::writeToSquadSettings(SquadSettingsInfo& squadSettings) const
{
}
