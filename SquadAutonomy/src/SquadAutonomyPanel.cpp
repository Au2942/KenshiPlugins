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
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>

#include <mygui/MyGUI_Button.h>

#include <kenshi/Town.h>
#include <kenshi/Building/Building.h>
#include <kenshi/GameWorld.h>

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
    _panel(nullptr)
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

    _mainTab.reset(new MainTab(*_panel, 0));
    _optionsTab.reset(new OptionsTab(*_panel, 1));

    if (_selectedSquad)
    {
        _mainTab->setSelectedSquad(*_selectedSquad);
        _optionsTab->setSelectedSquad(*_selectedSquad);
    }

    _panel->showTabs(true);
    _panel->addTab(_mainTab->getCategory(), Localization::gettext("Main"), "");
    _panel->addTab(_optionsTab->getCategory(), Localization::gettext("Options"), "");
    _panel->changeCategory(_mainTab->getCategory());

    refresh();

    createModSettingsButton();

    _panel->show(false);
}

void SquadAutonomyPanel::refresh()
{
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
    _panel->show(true);
    MyGUI::LayerManager::getInstancePtr()->upLayerItem(_panel->getWidget());
    refresh();
}

void SquadAutonomyPanel::hide()
{
    _panel->show(false);
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

    auto squadSettings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
    _panel->setLineSpacing(32.0);
    _panel->clearPage(_category);

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

    auto editbox = _panel->setLineTextEditable(Localization::gettext("Search"), "", _category, true, false, MyGUI::Align::Left, 0.95f);
    Utils::setCallback(*editbox, this, &MainTab::changeAIPackageSearchText);

    auto dropbox = _panel->setLineDropBox(Localization::lineAIPackage, _category, &_selectedAIPackageIndex, false, 1.0f);
    dropbox->w1->setSize(dropbox->w1->getWidth(), editbox->editBox->getHeight());

    updateAIPackageList();
    editbox->getEditBox()->setSize(dropbox->listBox->getSize());

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

        //DebugLog("Squad Settings exist: " + Ogre::StringConverter::toString(squadPackages.size()));
        for (auto it = squadPackages.begin(); it != squadPackages.end(); ++it)
        {
            auto packages = it->second;
            //DebugLog(Ogre::StringConverter::toString(it->first) + " Packages: " + Ogre::StringConverter::toString(packages.size()));
            for (int i = 0; i < packages.size(); ++i)
            {
                //DebugLog(Ogre::StringConverter::toString(it->first) + " " + packages[i]->name);
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
    SquadSettingsInfo* settingsInfo = settings.getSquadSettings(_selectedSquad, true);
    if (!settingsInfo)
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

    settingsInfo->addPackage(static_cast<int>(_priority), data);
    //DebugLog("Add " + data->name + " package");
    settingsInfo->updateSquadPackages();

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

    auto dropBox = dynamic_cast<DataPanelLine_DropBox*>(_panel->getLine(Localization::lineAIPackage, _category));
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


SquadAutonomyPanel::OptionsTab::OptionsTab(DatapanelGUI& panel, int category) :
    _panel(&panel),
    _selectedSquad(nullptr),
    _category(category),
    _subCategory(0),
    _basic(new BasicOptions(*this, panel, category)),
    _labour(new LabourOptions(*this, panel, category)),
    _combat(new CombatOptions(*this, panel, category)),
    _guard(new GuardOptions(*this, panel, category))
{
}

void SquadAutonomyPanel::OptionsTab::refresh()
{
    if (!SquadAutonomySettings::getSingleton().initialized || !_panel || !_selectedSquad)
    {
        return;
    }

    auto settings = SquadAutonomySettings::getSingleton().getSquadSettings(_selectedSquad, true);
    if (!settings)
    {
        // Should not happen, since createNew = true
        return;
    }

    _panel->setLineSpacing(24.0);
    _panel->clearPage(_category);

    if (_selectedSquad->activePlatoon)
    {
        _panel->setCaption(Localization::lineSquadAutonomyTitle + ' ' + _selectedSquad->activePlatoon->getName());
    }

    // TODO: make this a floating panel so it'll always be showing even when scrolled down
    auto dropBox = _panel->setLineDropBox(Localization::gettext("Category"), _category, &_subCategory, false, 1.0f);

    if (_subCategory < 0)
    {
        _subCategory = 0;
    }
    else if (_subCategory > 3)
    {
        _subCategory = 3;
    }

    dropBox->addAValue(Localization::gettext("Basic"), 0);
    dropBox->addAValue(Localization::gettext("Labour"), 1);
    dropBox->addAValue(Localization::gettext("Combat"), 2);
    dropBox->addAValue(Localization::gettext("Guard"), 3);
    dropBox->setSelectedValue(_subCategory);

    dropBox->getComboBox()->eventComboAccept += MyGUI::newDelegate(this, &SquadAutonomyPanel::OptionsTab::refreshCallback);
    readFromSquadSettings(*settings);
    switch (_subCategory)
    {
    case 1:
        _labour->refresh(*settings);
        break;
    case 2:
        _combat->refresh(*settings);
        break;
    case 3:
        _guard->refresh(*settings);
        break;
    default:
        _basic->refresh(*settings);
        break;
    }
    if (_panel->getNumLines(_category) == 1)
    {
        Utils::addText(*_panel, _category, *_MainColorCode + Localization::gettext("No settings available for current packages"), false, false);
    }
}

int SquadAutonomy::SquadAutonomyPanel::OptionsTab::getCategory() const
{
    return _category;
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

void SquadAutonomy::SquadAutonomyPanel::OptionsTab::readFromSquadSettings(const SquadSettingsInfo& squadSettings)
{
    _basic->readFromSquadSettings(squadSettings);
    _labour->readFromSquadSettings(squadSettings);
    _combat->readFromSquadSettings(squadSettings);
    _guard->readFromSquadSettings(squadSettings);
}

DataPanelLine_SliderEditable* SquadAutonomyPanel::OptionsTab::createOptionsSlider(std::string key, float* var, int precision, float min, float max, bool seperateTextLine)
{
    if (seperateTextLine)
    {
        Utils::addText(*_panel, _category, key);
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

    slider->callback = new MyGUI::delegates::CMethodDelegate1<OptionsTab, DataPanelLine*>(
        MyGUI::delegates::GetDelegateUnlink(this),
        this,
        &OptionsTab::writeToSelectedSquadSettings
    );

    return slider;
}


SquadAutonomyPanel::OptionsTab::OptionsBase::OptionsBase(OptionsTab &optionsTab, DatapanelGUI &panel, int category) :
    _optionsTab(&optionsTab),
    _panel(&panel),
    _category(category)
{
}


SquadAutonomyPanel::OptionsTab::BasicOptions::BasicOptions(OptionsTab& optionsTab, DatapanelGUI& panel, int category) :
    OptionsBase(optionsTab, panel, category),
    startWorkTime(0.0f),
    endWorkTime(24.0f),
    doSleep(false),
    restUntilHealed(true),
    restThreshold(50.0),
    healedThreshold(90.0),
    usePaidBeds(false)
{
}

void SquadAutonomyPanel::OptionsTab::BasicOptions::refresh(SquadSettingsInfo& settings)
{
    auto startSlider = Utils::addSlider(*_panel, _category, Localization::gettext("Start Work Time"), true, startWorkTime, 0.0f, 24.0f, 0);
    Utils::setCallback(*startSlider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

    auto endSlider = Utils::addSlider(*_panel, _category, Localization::gettext("End Work Time"), true, endWorkTime, 0.0f, 24.0f, 0);
    Utils::setCallback(*endSlider, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

    auto checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Rest Until Healed When Wounded"), restUntilHealed);
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

void SquadAutonomyPanel::OptionsTab::BasicOptions::readFromSquadSettings(const SquadSettingsInfo& squadSettings)
{
    startWorkTime = squadSettings.getStartWorkTime();
    endWorkTime = squadSettings.getEndWorkTime();
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
    squadSettings.setDoSleep(doSleep);
    squadSettings.setRestUntilHealed(restUntilHealed);
    squadSettings.setRestThreshold(restThreshold);
    squadSettings.setHealedThreshold(healedThreshold);
    squadSettings.setUsePaidBeds(usePaidBeds);
}


SquadAutonomyPanel::OptionsTab::LabourOptions::LabourOptions(OptionsTab& optionsTab, DatapanelGUI& panel, int category) :
    OptionsBase(optionsTab, panel, category),
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

void SquadAutonomyPanel::OptionsTab::LabourOptions::refresh(SquadSettingsInfo& settings)
{
    if (!settings.hasTask(AUTO_LABOURING_MINES)) return;
    _panel->setLineSpacing(28.0);

    const std::string statsUsedLabel = *_MainColorCode + Localization::gettext("Stats Used:") + ' ' + Localization::gettext("(bigger number = higher priority, 0 = forbid)");
    Utils::addText(*_panel, _category, statsUsedLabel, false, false);

    const float maxPriority = 10.0;
    _optionsTab->createOptionsSlider(Localization::gettext("Science"), &science, 0, 0.0, maxPriority, false);
    _optionsTab->createOptionsSlider(Localization::gettext("Labouring (Mining)"), &labouring, 0, 0.0, maxPriority, false);
    _optionsTab->createOptionsSlider(Localization::gettext("Farming"), &farming, 0, 0.0, maxPriority, false);
    _optionsTab->createOptionsSlider(Localization::gettext("Cooking"), &cooking, 0, 0.0, maxPriority, false);
    _optionsTab->createOptionsSlider(Localization::gettext("Medic"), &medic, 0, 0.0, maxPriority, false);
    _optionsTab->createOptionsSlider(Localization::gettext("Engineer"), &engineer, 0, 0.0, maxPriority, false);
    _optionsTab->createOptionsSlider(Localization::gettext("Robotics"), &robotics, 0, 0.0, maxPriority, false);
    _optionsTab->createOptionsSlider(Localization::gettext("Weapon Smith"), &weaponSmith, 0, 0.0, maxPriority, false);
    _optionsTab->createOptionsSlider(Localization::gettext("Armour Smith"), &armourSmith, 0, 0.0, maxPriority, false);
    _optionsTab->createOptionsSlider(Localization::gettext("Crossbow Smith"), &crossbowSmith, 0, 0.0, maxPriority, false);
    _optionsTab->createOptionsSlider(Localization::gettext("Automatic Machine"), &automaticMachine, 0, 0.0, maxPriority, false);
    _optionsTab->createOptionsSlider(Localization::gettext("Other"), &other, 0, 0.0, maxPriority, false);

    _panel->addSpace(_category, 0.25);

    auto slider = _panel->setLineSliderEditable(Localization::gettext("Set All"), _category, true, 0.0, maxPriority, &setAll);
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


SquadAutonomyPanel::OptionsTab::CombatOptions::CombatOptions(OptionsTab& optionsTab, DatapanelGUI& panel, int category) :
    OptionsBase(optionsTab, panel, category),
    attackEnemies(true),
    protectAllies(true),
    doMedic(true),
    doRescue(true)
{
}

void SquadAutonomyPanel::OptionsTab::CombatOptions::refresh(SquadSettingsInfo& settings)
{
    auto checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Attack Enemies"), attackEnemies);
    Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

    checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Protect Allies"), protectAllies);
    Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

    checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Do Medic"), doMedic);
    Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

    checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Rescue Downed Allies"), doRescue);
    Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
}

void SquadAutonomyPanel::OptionsTab::CombatOptions::readFromSquadSettings(const SquadSettingsInfo& squadSettings)
{
    attackEnemies = squadSettings.getAttackEnemies();
    protectAllies = squadSettings.getProtectAllies();
    doMedic = squadSettings.getDoMedic();
    doRescue = squadSettings.getDoRescue();
}

void SquadAutonomyPanel::OptionsTab::CombatOptions::writeToSquadSettings(SquadSettingsInfo& squadSettings) const
{
    squadSettings.setAttackEnemies(attackEnemies);
    squadSettings.setProtectAllies(protectAllies);
    squadSettings.setDoMedic(doMedic);
    squadSettings.setDoRescue(doRescue);
}


SquadAutonomyPanel::OptionsTab::GuardOptions::GuardOptions(OptionsTab& optionsTab, DatapanelGUI& panel, int category) :
    OptionsBase(optionsTab, panel, category),
    manTurrets(false),
    stayInsideGate(false),
    closeGate(false)
{
}

void SquadAutonomyPanel::OptionsTab::GuardOptions::refresh(SquadSettingsInfo& settings)
{
    if (settings.hasTask(MAN_A_TURRET) || settings.hasTask(MAN_A_TURRET_ON_BUILDING))
    {
        auto checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Man Turrets"), manTurrets);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
    }
    if (settings.hasTask(MAN_THE_GATE))
    {
        Utils::addText(*_panel, _category, *_MainColorCode + Localization::gettext("Gate Guard:"), false, false);

        auto checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Stay Inside Gate"), stayInsideGate);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);

        checkbox = Utils::addCheckbox(*_panel, _category, Localization::gettext("Close Gate"), closeGate);
        Utils::setCallback(*checkbox, _optionsTab, &OptionsTab::writeToSelectedSquadSettings);
    }
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
