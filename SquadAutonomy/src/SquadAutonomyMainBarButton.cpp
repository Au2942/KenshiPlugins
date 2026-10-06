#include "SquadAutonomyMainBarButton.h"

#include <memory>

#include "SquadAutonomy.h"
#include "SquadAutonomyLocalization.h"
#include "SquadAutonomyModSettings.h"

using namespace SquadAutonomy;

MainBarButton &MainBarButton::getSingleton()
{
    static std::unique_ptr<MainBarButton> singleton;
    if (!singleton)
    {
        singleton.reset(new MainBarButton());
    }

    return *singleton.get();
}

MainBarButton::MainBarButton() :
    _autBtn(nullptr),
    _dragStart(0, 0),
    _windowStart(0, 0),
    _clicked(false),
    _dragged(false)
{
}

MainBarButton::~MainBarButton()
{
}

void MainBarButton::createButton(MyGUI::Widget& parent)
{
    if (_autBtn)
    {
        return;
    }

    const auto& settingsValues = ModSettings::getSingleton().getValues();

    _autBtn = parent.createWidgetReal<MyGUI::Button>(
        "Kenshi_Button1",
        settingsValues.btnLeft,
        settingsValues.btnTop,
        settingsValues.btnWidth/100.0,
        settingsValues.btnHeight/100.0,
        MyGUI::Align::Center,
        "AUTBtn"
    );

    _autBtn->setCaption(Localization::gettext("AUT"));
    _autBtn->setFontHeight(settingsValues.btnFontSize);

    _autBtn->eventMouseButtonPressed += MyGUI::newDelegate(onPressed);
    _autBtn->eventMouseDrag += MyGUI::newDelegate(onDrag);
    _autBtn->eventMouseButtonReleased += MyGUI::newDelegate(onReleased);

    _autBtn->setDepth(0);

    _autBtn->setUserData(this);

    if (!settingsValues.showOnMain)
    {
        _autBtn->setVisible(false);
    }
}

void MainBarButton::destroyButton(MyGUI::Widget &parent)
{
    if (!_autBtn)
    {
        return;
    }

    parent._destroyChildWidget(_autBtn);
    _autBtn = nullptr;
}

void MainBarButton::updateButton()
{
    if (!_autBtn)
    {
        return;
    }

    const auto& settingsValues = ModSettings::getSingleton().getValues();

    if (settingsValues.showOnMain)
    {
        _autBtn->setVisible(true);
        _autBtn->setRealPosition(settingsValues.btnLeft, settingsValues.btnTop);
        _autBtn->setRealSize(settingsValues.btnWidth / 100.0, settingsValues.btnHeight / 100.0);
        _autBtn->setFontHeight(settingsValues.btnFontSize);
        _autBtn->setDepth(0);
    }
    else
    {
        _autBtn->setVisible(false);
    }
}

void MainBarButton::onPressed(
    MyGUI::Widget* sender,
    int left,
    int top,
    MyGUI::MouseButton id)
{
    if (!sender || id != MyGUI::MouseButton::Left)
    {
        return;
    }

    MainBarButton** button = sender->getUserData<MainBarButton*>(false);
    if (!button || !*button)
    {
        return;
    }

    MainBarButton& buttonRef = **button;

    buttonRef._clicked = true;

    buttonRef._dragStart = MyGUI::IntPoint(left, top);
    buttonRef._windowStart = buttonRef._autBtn->getPosition();
    sender->_setRootMouseFocus(true);
}

void MainBarButton::onDrag(
    MyGUI::Widget* sender,
    int left,
    int top,
    MyGUI::MouseButton id)
{
    if (!sender || id != MyGUI::MouseButton::Left)
    {
        return;
    }

    MainBarButton** button = sender->getUserData<MainBarButton*>(false);
    if (!button || !*button)
    {
        return;
    }

    MainBarButton& buttonRef = **button;

    auto& settingsValuesMutable = ModSettings::getSingleton().getValuesMutable();

    if (settingsValuesMutable.lockPosition || !buttonRef._clicked)
    {
        return;
    }

    const int dx = left - buttonRef._dragStart.left;
    const int dy = top - buttonRef._dragStart.top;
    if (dx != 0 || dy != 0)
    {
        buttonRef._dragged = true;
        settingsValuesMutable.btnLeft = static_cast<float>(buttonRef._windowStart.left + dx) / buttonRef._autBtn->getParentSize().width;
        settingsValuesMutable.btnTop = static_cast<float>(buttonRef._windowStart.top + dy) / buttonRef._autBtn->getParentSize().height;
        buttonRef._autBtn->setRealPosition(
            settingsValuesMutable.btnLeft,
            settingsValuesMutable.btnTop
        );
        //btnLeft = static_cast<float>(autBtn->getLeft()) / autBtn->getParentSize().width;
        //btnTop = static_cast<float>(autBtn->getTop()) / autBtn->getParentSize().height;
    }
}

void MainBarButton::onReleased(
    MyGUI::Widget* sender,
    int left,
    int top,
    MyGUI::MouseButton id)
{
    if (!sender || id != MyGUI::MouseButton::Left)
    {
        return;
    }

    MainBarButton** button = sender->getUserData<MainBarButton*>(false);
    if (!button || !*button)
    {
        return;
    }

    MainBarButton& buttonRef = **button;

    if (buttonRef._clicked && !buttonRef._dragged)
    {
        OpenSquadAutonomyPanelMainBar();
    }
    else if (buttonRef._dragged)
    {
        ModSettings::getSingleton().saveToFile();
    }

    buttonRef._clicked = false;
    buttonRef._dragged = false;
    sender->_setRootMouseFocus(false);
}
