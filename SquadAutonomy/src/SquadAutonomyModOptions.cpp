#include "SquadAutonomy.h"
#include "SquadAutonomySettings.h"
#include "SquadAutonomyModOptions.h"
#include "SquadAutonomyLocalization.h"
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
    this->_panel->setCaption(Localization::gettext("Squad Autonomy Mod Options"));
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
    auto checkbox = this->_panel->setLineCheckbox(Localization::gettext("Show button on Mainbar"), &showOnMain, _category);
    if (tooltip) tooltip->setup(checkbox->getTextBox(), Localization::gettext("Show AUT button on the Mainbar"));
    checkbox = this->_panel->setLineCheckbox(Localization::gettext("Lock button position"), &lockPosition, _category);
    if (tooltip) tooltip->setup(checkbox->getTextBox(), Localization::gettext("Lock/Unlock button position (can be dragged around to change position when unlocked)"));
    auto slider = this->_panel->setLineSliderEditable(Localization::gettext("Button Width"), _category, true, 0.0, 100.0, &btnWidth);
    slider->setPrecision(2);
    if (tooltip) tooltip->setup(slider->nameText, Localization::gettext("Set the button width relative to the screen width"));
    slider = this->_panel->setLineSliderEditable(Localization::gettext("Button Height"), _category, true, 0.0, 100.0, &btnHeight);
    slider->setPrecision(2);
    if (tooltip) tooltip->setup(slider->nameText, Localization::gettext("Set the button height relative to the screen height"));
    slider = this->_panel->setLineSliderEditable(Localization::gettext("Button Font Size"), _category, true, 0.0, 100.0, &btnFontSize);
    slider->setPrecision(0);
    if (tooltip) tooltip->setup(slider->nameText, Localization::gettext("Set the button font size"));
    checkbox = this->_panel->setLineCheckbox(Localization::gettext("Show button in Squad screen"), &showInSquad, _category);
    if (tooltip) tooltip->setup(checkbox->getTextBox(), Localization::gettext("Show AUT button in the Squad Management screen"));
    checkbox = this->_panel->setLineCheckbox(Localization::gettext("Enable logging"), &enableLogging, _category);
    if (tooltip) tooltip->setup(checkbox->getTextBox(), Localization::gettext("Write to log. Turn on if experiencing frequent crashes and include the last few lines with your bug report."));
    auto button = this->_panel->setLineButton("", Localization::gettext("Reset to Default"), _category);
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
    std::wifstream cfgFile(settings->getConfigPath(), std::wfstream::in);
    std::wofstream tempFile(L"temp.txt");
    if (!cfgFile.is_open() || !tempFile.is_open())
    {
        DebugLog("Config Save: Cannot open file");
    }
    else
    {
        bool packagesFound = false;
        std::wstring wline = L"";
        tempFile << converter.from_bytes("<Options>") << L'\n';
        tempFile << converter.from_bytes("ShowOnMain: " + Ogre::StringConverter::toString(showOnMain)) << L'\n';
        tempFile << converter.from_bytes("LockPosition: " + Ogre::StringConverter::toString(lockPosition)) << L'\n';
        tempFile << converter.from_bytes("BtnWidth: " + Ogre::StringConverter::toString(btnWidth)) << L'\n';
        tempFile << converter.from_bytes("BtnHeight: " + Ogre::StringConverter::toString(btnHeight)) << L'\n';
        tempFile << converter.from_bytes("BtnLeft: " + Ogre::StringConverter::toString(btnLeft)) << L'\n';
        tempFile << converter.from_bytes("BtnTop: " + Ogre::StringConverter::toString(btnTop)) << L'\n';
        tempFile << converter.from_bytes("BtnFontSize: " + Ogre::StringConverter::toString(btnFontSize)) << L'\n';
        tempFile << converter.from_bytes("ShowInSquad: " + Ogre::StringConverter::toString(showInSquad)) << L'\n';
        tempFile << converter.from_bytes("EnableLogging: " + Ogre::StringConverter::toString(enableLogging)) << L'\n';
        tempFile << converter.from_bytes("</Options>") << L'\n';
        while (std::getline(cfgFile, wline))
        {
            //std::string line = converter.to_bytes(wline);
            if (wline == L"<Packages>")
            {
                tempFile << wline << L'\n';
                while (std::getline(cfgFile, wline))
                {
                    tempFile << wline << L'\n';
                    if (wline == L"</Packages>")
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
            tempFile << L"<Packages>" << L'\n';
            tempFile << L"</Packages>";
        }
        cfgFile.close();
        tempFile.close();

        _wremove(settings->getConfigPath().c_str());
        _wrename(L"temp.txt", settings->getConfigPath().c_str());
    }
}

