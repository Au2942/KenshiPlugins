#include "SquadAutonomy.h"
#include "SquadAutonomySettings.h"
#include "SquadAutonomyModOptions.h"
#include <Debug.h>
#include <ogre/OgreStringConverter.h>
#include <boost/scoped_ptr.hpp>
#include <kenshi/Globals.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <kenshi/gui/ToolTip.h>


using namespace SquadAutonomy;
bool SquadAutonomyModOptions::initialized = false;
SquadAutonomyModOptions* SquadAutonomyModOptions::getSingletonPtr()
{
    static boost::scoped_ptr<SquadAutonomyModOptions> singleton(new SquadAutonomyModOptions());
    return singleton.get();
}

SquadAutonomyModOptions::SquadAutonomyModOptions() : _category(0), _panel(nullptr), _optionsWindow(nullptr)
{
    create();
    initialized = true;
}

SquadAutonomyModOptions::~SquadAutonomyModOptions() {}

void SquadAutonomyModOptions::setOptionsWindow(OptionsWindow* win)
{
    _optionsWindow = win;
}

void SquadAutonomyModOptions::create()
{
    if (this->_panel != nullptr)
    {
        this->_panel->show(false);
        gui->destroy(this->_panel);
    }
    this->_panel = gui->createDatapanel(0.25f, 0.35f, 0.3f, 0.5f, true, "Window", true);
    this->_panel->setCaption("Squad Autonomy Mod Options");
    this->_panel->setPanelName("SquadAutonomyModOptions");

    refresh();

    this->_panel->show(false);
    this->_panel->setCloseCallback(MyGUI::newDelegate(this, &SquadAutonomyModOptions::close));
}

void SquadAutonomyModOptions::close(MyGUI::Window* sender, const std::string& name)
{
    saveOptionsSettings();
}

void SquadAutonomyModOptions::resetSettingsToDefault(MyGUI::Widget* sender)
{
    showOnMain = true;
    lockPosition = true;
    showInSquad = true;
    enableLogging = false;
    btnWidth = defaultBtnWidth;
    btnHeight = defaultBtnHeight;
    btnFontSize = defaultBtnFontSize;
    btnLeft = defaultBtnLeft;
    btnTop = defaultBtnTop;
    refresh();
}

void SquadAutonomyModOptions::refresh()
{
    if (!_panel) return;
    ToolTip* tooltip = nullptr;
    if (_optionsWindow) tooltip = _optionsWindow->tooltip;
    this->_panel->clearPage(this->_category);
    this->_panel->setLineSpacing(24.0f);
    //auto textbox = this->_panel->setLineText("", *_MainColorCode + "[Squad Autonomy]", _category, true, MyGUI::Align::Left);
    auto checkbox = this->_panel->setLineCheckbox("Show button on Mainbar", &showOnMain, _category);
    if (tooltip) tooltip->setup(checkbox->getTextBox(), "Show AUT button on the Mainbar");
    checkbox = this->_panel->setLineCheckbox("Lock button position", &lockPosition, _category);
    if (tooltip) tooltip->setup(checkbox->getTextBox(), "Lock/Unlock button position (can be dragged around to change position when unlocked)");
    auto slider = this->_panel->setLineSliderEditable("Button Width", _category, true, 0.0, 100.0, &btnWidth);
    slider->setPrecision(2);
    if (tooltip) tooltip->setup(slider->nameText, "Set the button width relative to the screen width");
    slider = this->_panel->setLineSliderEditable("Button Height", _category, true, 0.0, 100.0, &btnHeight);
    slider->setPrecision(2);
    if (tooltip) tooltip->setup(slider->nameText, "Set the button height relative to the screen height");
    slider = this->_panel->setLineSliderEditable("Button Font Size", _category, true, 0.0, 100.0, &btnFontSize);
    slider->setPrecision(0);
    if (tooltip) tooltip->setup(slider->nameText, "Set the button font size");
    checkbox = this->_panel->setLineCheckbox("Show button in Squad screen", &showInSquad, _category);
    if (tooltip) tooltip->setup(checkbox->getTextBox(), "Show AUT button in the Squad Management screen");
    checkbox = this->_panel->setLineCheckbox("Enable logging", &enableLogging, _category);
    if (tooltip) tooltip->setup(checkbox->getTextBox(), "Write to log. Turn on if experiencing frequent crashes and include the last few lines with your bug report.");
    auto button = this->_panel->setLineButton("", "Reset to Default", _category);
    button->button->eventMouseButtonClick += MyGUI::newDelegate(this, &SquadAutonomyModOptions::resetSettingsToDefault);

}

void SquadAutonomyModOptions::show()
{
    this->_panel->show(true);
    MyGUI::LayerManager::getInstancePtr()->upLayerItem(this->_panel->getWidget());
    refresh();
}

void SquadAutonomyModOptions::hide()
{
    this->_panel->show(false);
    saveOptionsSettings();
}

bool SquadAutonomyModOptions::isVisible()
{
    return this->_panel->isVisible();
}

void SquadAutonomyModOptions::saveOptionsSettings()
{
    auto settings = SquadAutonomySettings::getSingletonPtr();
    std::ifstream cfgFile(settings->getConfigPath(), std::fstream::in);
    std::ofstream tempFile("temp.txt");
    if (!cfgFile.is_open() || !tempFile.is_open())
    {
        DebugLog("Config Save: Cannot open file");
    }
    else
    {
        bool packagesFound = false;
        std::string line = "";
        tempFile << "<Options>" << '\n';
        tempFile << "ShowOnMain: " << Ogre::StringConverter::toString(showOnMain) << '\n';
        tempFile << "LockPosition: " << Ogre::StringConverter::toString(lockPosition) << '\n';
        tempFile << "BtnWidth: " << Ogre::StringConverter::toString(btnWidth) << '\n';
        tempFile << "BtnHeight: " << Ogre::StringConverter::toString(btnHeight) << '\n';
        tempFile << "BtnLeft: " << Ogre::StringConverter::toString(btnLeft) << '\n';
        tempFile << "BtnTop: " << Ogre::StringConverter::toString(btnTop) << '\n';
        tempFile << "BtnFontSize: " << Ogre::StringConverter::toString(btnFontSize) << '\n';
        tempFile << "ShowInSquad: " << Ogre::StringConverter::toString(showInSquad) << '\n';
        tempFile << "EnableLogging: " << Ogre::StringConverter::toString(enableLogging) << '\n';
        tempFile << "</Options>" << '\n';
        while (std::getline(cfgFile, line))
        {
            if (line == "<Packages>")
            {
                tempFile << line << '\n';
                while (std::getline(cfgFile, line))
                {
                    tempFile << line << '\n';
                    if (line == "</Packages>")
                    {
                        packagesFound = true;
                        break;
                    }
                }
                break;
            }
        }
        if (!packagesFound)
        {
            tempFile << "<Packages>" << '\n';
            tempFile << "</Packages>";
        }
        cfgFile.close();
        tempFile.close();

        _wremove(settings->getConfigPath().c_str());
        _wrename(L"temp.txt", settings->getConfigPath().c_str());
    }
}

