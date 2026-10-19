#include "SquadAutonomyModSettingsUI.h"

#include <Debug.h>
#include <kenshi/Globals.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <kenshi/gui/ToolTip.h>
#include "SquadAutonomyLocalization.h"
#include "SquadAutonomyModSettings.h"

using namespace SquadAutonomy;

ModSettingsUI& ModSettingsUI::getSingleton()
{
    static std::unique_ptr<ModSettingsUI> singleton;
    if (!singleton)
    {
        singleton.reset(new ModSettingsUI());
    }

    return *singleton.get();
}

ModSettingsUI::ModSettingsUI() : _category(0), _panel(nullptr), _optionsWindow(nullptr), _logSeverityIndex(0), _logCategoryIndex(0)
{
    create();
}

ModSettingsUI::~ModSettingsUI()
{
    // TODO: destroy?
    //gui->destroy(_panel);
}

void ModSettingsUI::setOptionsWindow(OptionsWindow* win)
{
    _optionsWindow = win;
}

void ModSettingsUI::create()
{
    if (_panel)
    {
        _panel->show(false);
        gui->destroy(_panel);
    }
    Logger::log("Creating Mod Settings Panel", Logger::Info, false);
    _panel = gui->createDatapanel(0.25f, 0.35f, 0.3f, 0.5f, true, "Window", true);
    _panel->setCaption(Localization::gettext("Squad Autonomy Mod Options"));
    _panel->setPanelName("SquadAutonomyModOptions");

    refresh();

    _panel->show(false);
    _panel->setCloseCallback(MyGUI::newDelegate(this, &ModSettingsUI::close));
}

void ModSettingsUI::close(MyGUI::Window* sender, const std::string& name)
{
    saveSettings();
}

void ModSettingsUI::resetSettingsToDefault(MyGUI::Widget* sender)
{
    ModSettings::getSingleton().resetToDefault();
    refresh();
}

void ModSettingsUI::updateLogSeverity(MyGUI::ComboBox* sender, size_t index)
{
    if (!ModSettings::getSingleton().getValues().enableLogging)
    {
        sender->setIndexSelected(_logSeverityIndex);
        return;
    }
    else _logSeverityIndex = index;
    auto& settingsValuesMutable = ModSettings::getSingleton().getValuesMutable();
    if (_logSeverityIndex < 0 || _logSeverityIndex >= sizeof(Logger::severities) / sizeof(Logger::severities[0])) return;
    else settingsValuesMutable.logSeverity = Logger::severities[_logSeverityIndex];
    refresh();
    /*switch (_logSeverityIndex)
    {
    case static_cast<int>(Logger::None):
    case static_cast<int>(Logger::Info):
    case static_cast<int>(Logger::Warning):
    case static_cast<int>(Logger::Error):
    case static_cast<int>(Logger::Debug):
        settingsValuesMutable.logSeverity = static_cast<Logger::Severity>(_logSeverityIndex);
        break;
    default:
        settingsValuesMutable.logSeverity = Logger::None;
    }
    sender->setIndexSelected(_logSeverityIndex);*/
}

void ModSettingsUI::updateLogCategory(MyGUI::ComboBox* sender, size_t index)
{
    if (ModSettings::getSingleton().getValues().logSeverity < Logger::Debug)
    {
        sender->setIndexSelected(_logCategoryIndex);
        return;
    }
    else _logCategoryIndex = index;
    auto& settingsValuesMutable = ModSettings::getSingleton().getValuesMutable();
    if (_logCategoryIndex < 0 || _logCategoryIndex >= sizeof(Logger::categories) / sizeof(Logger::categories[0])) return;
    else settingsValuesMutable.logCategory = Logger::categories[_logCategoryIndex];
    refresh();
    /*switch (_logCategoryIndex)
    {
    case static_cast<uint32_t>(Logger::Any):
    case static_cast<uint32_t>(Logger::AI_SCORING):
    case static_cast<uint32_t>(Logger::AI_TARGETFINDING):
    case static_cast<uint32_t>(Logger::LABOUR):
    case static_cast<uint32_t>(Logger::GUARD):
    case static_cast<uint32_t>(Logger::SHOPPING):
        settingsValuesMutable.logCategory = static_cast<Logger::Category>(_logCategoryIndex);
        break;
    default:
        settingsValuesMutable.logCategory = Logger::Any;
    }
    sender->setIndexSelected(_logCategoryIndex);*/
}

void ModSettingsUI::updatePanel(DataPanelLine* line)
{
    refresh();
}

void ModSettingsUI::refresh()
{
    if (!_panel)
    {
        return;
    }

    _panel->clearPage(_category);
    _panel->setLineSpacing(24.0f);
    //auto textbox = _panel->setLineText("", *_MainColorCode + "[Squad Autonomy]", _category, true, MyGUI::Align::Left);

    auto& settingsValuesMutable = ModSettings::getSingleton().getValuesMutable();

    addCheckbox(
        Localization::gettext("Show button on Mainbar"),
        Localization::gettext("Show AUT button on the Mainbar"),
        settingsValuesMutable.showOnMain
    );
    addCheckbox(
        Localization::gettext("Lock button position"),
        Localization::gettext("Lock/Unlock button position (can be dragged around to change position when unlocked)"),
        settingsValuesMutable.lockPosition
    );
    addSlider(
        Localization::gettext("Button Width"),
        Localization::gettext("Set the button width relative to the screen width"),
        settingsValuesMutable.btnWidth,
        0.0f,
        100.0f,
        2
    );
    addSlider(
        Localization::gettext("Button Height"),
        Localization::gettext("Set the button height relative to the screen height"),
        settingsValuesMutable.btnHeight,
        0.0f,
        100.0f,
        2
    );
    addSlider(
        Localization::gettext("Button Font Size"),
        Localization::gettext("Set the button font size"),
        settingsValuesMutable.btnFontSize,
        0.0f,
        100.0f,
        0
    );
    addCheckbox(
        Localization::gettext("Show button in Squad screen"),
        Localization::gettext("Show AUT button in the Squad Management screen"),
        settingsValuesMutable.showInSquad
    );
    auto checkbox = addCheckbox(
        Localization::gettext("Enable logging"),
        Localization::gettext("Write to log. Turn on if experiencing frequent crashes and include the last few lines with your bug report."),
        settingsValuesMutable.enableLogging
    );
    checkbox->callback = new MyGUI::delegates::CMethodDelegate1<ModSettingsUI, DataPanelLine*>(MyGUI::delegates::GetDelegateUnlink(this),
        this, &ModSettingsUI::updatePanel);

    std::string logGreyed;
    _logSeverityIndex = static_cast<int>(settingsValuesMutable.logSeverity);
    if (!ModSettings::getSingleton().getValues().enableLogging) logGreyed = *_GreyedColorCode;
    // severity
    int severityListIndex = _logSeverityIndex;
    auto dropbox = _panel->setLineDropBox(logGreyed+Localization::gettext("Log Severity"), _category, &severityListIndex, false, 1.0);
    dropbox->addAValue(logGreyed+Localization::gettext("None"), 0);
    dropbox->addAValue(logGreyed+Localization::gettext("Error"), 1);
    dropbox->addAValue(logGreyed+Localization::gettext("Warning"), 2);
    dropbox->addAValue(logGreyed+Localization::gettext("Info"), 3);
    dropbox->addAValue(logGreyed+Localization::gettext("Debug"), 4);
    dropbox->setSelectedValue(_logSeverityIndex);
    dropbox->getComboBox()->eventComboAccept += MyGUI::newDelegate(this, &ModSettingsUI::updateLogSeverity);
    if (_optionsWindow && _optionsWindow->tooltip)
    {
        std::string dropboxTooltip = Localization::gettext("Select log severity level. Higher severity includes all lower ones.");
        _optionsWindow->tooltip->setup(dropbox->getComboBox(),
            dropboxTooltip);
        _optionsWindow->tooltip->setup(dropbox->w1,
            dropboxTooltip);
    }
    // category
    std::string logDebugGreyed;
    _logSeverityIndex = static_cast<int>(settingsValuesMutable.logCategory);
    int categoryListIndex = _logCategoryIndex;
    if (ModSettings::getSingleton().getValues().logSeverity < Logger::Debug) logDebugGreyed = *_GreyedColorCode;
    dropbox = _panel->setLineDropBox(logDebugGreyed + Localization::gettext("Log Debug Category"), _category, &categoryListIndex, false, 1.0);
    dropbox->addAValue(logDebugGreyed + "Any", 0);
    dropbox->addAValue(logDebugGreyed + "AI_SCORING", 1);
    dropbox->addAValue(logDebugGreyed + "AI_TARGETFINDING", 2);
    dropbox->addAValue(logDebugGreyed + "LABOUR", 3);
    dropbox->addAValue(logDebugGreyed + "GUARD", 4);
    dropbox->addAValue(logDebugGreyed + "SHOPPING", 5);
    dropbox->setSelectedValue(_logCategoryIndex);
    dropbox->getComboBox()->eventComboAccept += MyGUI::newDelegate(this, &ModSettingsUI::updateLogCategory);
    if (_optionsWindow && _optionsWindow->tooltip)
    {
        std::string dropboxTooltip = Localization::gettext("Select log debug category to be logged.");
        _optionsWindow->tooltip->setup(dropbox->getComboBox(),
            dropboxTooltip);
        _optionsWindow->tooltip->setup(dropbox->w1,
            dropboxTooltip);
    }


    checkbox = addCheckbox(
        logGreyed + Localization::gettext("Allow Spam"),
        Localization::gettext("Allow logging spam."),
        settingsValuesMutable.logSpam
    );
    checkbox->setEnabled(ModSettings::getSingleton().getValues().enableLogging);



    auto button = _panel->setLineButton("", Localization::gettext("Reset to Default"), _category);
    button->button->eventMouseButtonClick += MyGUI::newDelegate(this, &ModSettingsUI::resetSettingsToDefault);
}

void ModSettingsUI::show()
{
    if (!_panel) return;
    _panel->show(true);
    MyGUI::LayerManager::getInstancePtr()->upLayerItem(_panel->getWidget());
    refresh();
}

void ModSettingsUI::hide()
{
    if (!_panel) return;
    _panel->show(false);
    saveSettings();
}

bool ModSettingsUI::isVisible()
{
    if (!_panel) return false;
    return _panel->isVisible();
}

void ModSettingsUI::saveSettings()
{
    ModSettings::getSingleton().saveToFile();
}

DataPanelLine_CheckBox* ModSettingsUI::addCheckbox(const std::string &label, const std::string &tooltip, bool& value)
{
    if (!_panel)
    {
        return nullptr;
    }

    DataPanelLine_CheckBox* checkbox = _panel->setLineCheckbox(label, &value, _category);

    if (_optionsWindow && _optionsWindow->tooltip && !tooltip.empty())
    {
        _optionsWindow->tooltip->setup(checkbox->getTextBox(), tooltip);
    }

    return checkbox;
}

DataPanelLine_SliderEditable* ModSettingsUI::addSlider(const std::string &label, const std::string &tooltip, float& value, float min, float max, int precision)
{
    if (!_panel)
    {
        return nullptr;
    }

    DataPanelLine_SliderEditable* slider = _panel->setLineSliderEditable(label, _category, true, min, max, &value);
    slider->setPrecision(precision);

    if (_optionsWindow && _optionsWindow->tooltip && !tooltip.empty())
    {
        _optionsWindow->tooltip->setup(slider->nameText, tooltip);
    }

    return slider;
}
