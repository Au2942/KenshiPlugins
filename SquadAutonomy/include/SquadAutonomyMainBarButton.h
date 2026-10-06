#pragma once

#include <mygui/MyGUI_Button.h>

namespace SquadAutonomy
{
    class MainBarButton
    {
    public:
        static MainBarButton& getSingleton();
        ~MainBarButton();

        void createButton(MyGUI::Widget& parent);
        void destroyButton(MyGUI::Widget& parent);
        void updateButton();

    private:
        // Make default constructor private to only allow creation as the singleton.
        MainBarButton();

        // Disable copy and move operations for the singleton by declaring them private.
        MainBarButton(const MainBarButton& other);
        MainBarButton(MainBarButton&& other);
        MainBarButton& operator=(const MainBarButton& other);
        MainBarButton& operator=(MainBarButton&& other);

        static void onPressed(
            MyGUI::Widget* sender,
            int left,
            int top,
            MyGUI::MouseButton id
        );
        static void onDrag(
            MyGUI::Widget* sender,
            int left,
            int top,
            MyGUI::MouseButton id
        );
        static void onReleased(
            MyGUI::Widget* sender,
            int left,
            int top,
            MyGUI::MouseButton id
        );

        MyGUI::Button* _autBtn;

        MyGUI::IntPoint _dragStart;
        MyGUI::IntPoint _windowStart;
        bool _clicked;
        bool _dragged;
    };
}
