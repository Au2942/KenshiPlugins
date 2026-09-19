#include "MoreImmersiveBars.h"
#include "ModOptions.h"
#include <Debug.h>
#include <ogre/OgreStringConverter.h>
#include <boost/scoped_ptr.hpp>
#include <kenshi/Globals.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DataPanelLine.h>
#include <kenshi/gui/ToolTip.h>


using namespace MoreImmersiveBars;
bool ModOptions::initialized = false;
ModOptions* ModOptions::getSingletonPtr()
{
    static boost::scoped_ptr<ModOptions> singleton(new ModOptions());
    return singleton.get();
}

ModOptions::ModOptions() : _category(0), _panel(nullptr), _optionsWindow(nullptr)
{
    loadOptionsSettings();
    create();
    initialized = true;
}

ModOptions::~ModOptions() {}

void ModOptions::setOptionsWindow(OptionsWindow* win)
{
    _optionsWindow = win;
}

void ModOptions::create()
{
    if (this->_panel != nullptr)
    {
        this->_panel->show(false);
        gui->destroy(this->_panel);
    }
    this->_panel = gui->createDatapanel(0.4f, 0.4f, 0.2f, 0.2f, true, "Window", true);
    this->_panel->setCaption("More Immersive Bars Mod Options");
    this->_panel->setPanelName("ModOptions");

    refresh();

    this->_panel->show(false);
    this->_panel->setCloseCallback(MyGUI::newDelegate(this, &ModOptions::close));
}

void ModOptions::close(MyGUI::Window* sender, const std::string& name)
{
    saveOptionsSettings();
}

void ModOptions::refresh()
{
    if (!_panel) return;
    ToolTip* tooltip = nullptr;
    if (_optionsWindow) tooltip = _optionsWindow->tooltip;
    this->_panel->clearPage(this->_category);
    this->_panel->setLineSpacing(24.0f);
    //auto textbox = this->_panel->setLineText("", *_MainColorCode + "[Squad Autonomy]", _category, true, MyGUI::Align::Left);
    auto checkbox = this->_panel->setLineCheckbox("No sleep talking", &noSleepTalk, this->_category);
    if (tooltip) tooltip->setup(checkbox->getTextBox(), "Disable the ability to talk for sleeping NPCs (only those with AI involving bars) for a more immersive experience");

}

void ModOptions::show()
{
    this->_panel->show(true);
    MyGUI::LayerManager::getInstancePtr()->upLayerItem(this->_panel->getWidget());
    refresh();
}

void ModOptions::hide()
{
    this->_panel->show(false);
    saveOptionsSettings();
}

bool ModOptions::isVisible()
{
    return this->_panel->isVisible();
}

void ModOptions::loadOptionsSettings()
{
    std::ifstream cfgFile(cfgPath);
    if (!cfgFile.is_open())
    {
        DebugLog("Loading config: Cannot open file at " + cfgPath);
    }
    else
    {
        std::string line = "";
        while (std::getline(cfgFile, line))
        {
            std::string dataLine = "";
            std::string type = "";
            line.erase(0, line.find_first_not_of(" \t"));
            size_t colon = line.find(':');

            if (colon == std::string::npos)
            {
                continue;
            }
            type = line.substr(0, colon);
            //DebugLog(type);
            //get squad
            if (type == "DialogueWhileSleep")
            {
                dataLine = line.substr(colon + 1);
                dataLine.erase(0, dataLine.find_first_not_of(" \t"));
                //DebugLog(dataLine);
                if (dataLine == "false")
                {
                    noSleepTalk = false;
                }
                continue;
            }
        }
        DebugLog("Finished loading config file.");
        cfgFile.close();
    }
}

void ModOptions::saveOptionsSettings()
{
    std::ofstream cfgFile(cfgPath, std::fstream::out | std::fstream::trunc);
    if (!cfgFile.is_open())
    {
        cfgFile.open(cfgPath, std::fstream::out | std::fstream::app);
        if (!cfgFile.is_open())
        {
            DebugLog("Config Save: Cannot open file");
            return;
        }
    }
    cfgFile << "<Options>" << '\n';
    cfgFile << "DialogueWhileSleep: " << Ogre::StringConverter::toString(noSleepTalk) << '\n';
    cfgFile << "</Options>";
    cfgFile.close();
}

