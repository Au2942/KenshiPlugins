#pragma once

#include <kenshi/gui/SquadManagementScreen.h>

#include <mygui/MyGUI_Widget.h>

namespace SquadAutonomy
{
    // The "AUT" button on each squad in the squad management screen
    class SquadAutonomyButton
    {
    public:
        SquadAutonomyButton(SquadManagementScreen::SquadCellView& squadCellView);
        ~SquadAutonomyButton();

        void setVisible(bool visible);

        SquadManagementScreen::SquadCellView* getSquadCellView() const;

    private:
        // Button delegate stores the object pointer in a delegate.
        // Disabling copy and move to prevent the object pointer from changing.
        // Move operations with resubscribing could be implemented if needed.
        SquadAutonomyButton(const SquadAutonomyButton& other);
        SquadAutonomyButton(SquadAutonomyButton&& other);
        SquadAutonomyButton& operator=(const SquadAutonomyButton& other);
        SquadAutonomyButton& operator=(SquadAutonomyButton&& other);

        void onClick(MyGUI::WidgetPtr sender);

        MyGUI::Button* button;
        SquadManagementScreen::SquadCellView* cellview;
    };
}