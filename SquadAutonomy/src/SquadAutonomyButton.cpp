#include "SquadAutonomyButton.h"

#include "SquadAutonomyLocalization.h"

#include <kenshi/Platoon.h>

#include <mygui/MyGUI_Button.h>

#include "SquadAutonomy.h"

namespace SquadAutonomy
{
    SquadAutonomyButton::SquadAutonomyButton(SquadManagementScreen::SquadCellView& squadCellView)
    {
        cellview = &squadCellView;

        MyGUI::Widget* parent = squadCellView.txtName->getParent();
        int left = squadCellView.txtName->getRight();
        int width = squadCellView.txtSquadSize->getLeft() - left;
        int top = squadCellView.txtName->getTop();
        int height = squadCellView.txtName->getHeight();

        button = parent->createWidgetReal<MyGUI::Button>(
            "Kenshi_Button1",
            static_cast<float>(left) / parent->getWidth() + 0.05,
            static_cast<float>(top) / parent->getHeight(),
            static_cast<float>(width) / parent->getWidth() - 0.1,
            static_cast<float>(height) / parent->getHeight(),
            MyGUI::Align::Center,
            "AutonomyButton"
        );

        button->setCaption(Localization::gettext("AUT"));
        button->eventMouseButtonClick += MyGUI::newDelegate(this, &SquadAutonomyButton::onClick);
    }

    SquadAutonomyButton::~SquadAutonomyButton()
    {
    }

    void SquadAutonomyButton::setVisible(bool visible)
    {
        button->setVisible(visible);
    }

    SquadManagementScreen::SquadCellView* SquadAutonomyButton::getSquadCellView() const
    {
        return cellview;
    }

    void SquadAutonomyButton::onClick(MyGUI::WidgetPtr sender)
    {
        OpenSquadAutonomyPanel(cellview->squad->platoon->me);
    }
}
