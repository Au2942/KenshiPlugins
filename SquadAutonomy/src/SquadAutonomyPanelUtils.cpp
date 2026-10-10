#include "SquadAutonomyPanelUtils.h"

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>

namespace SquadAutonomy
{
    DataPanelLine_Button* SquadAutonomyPanelUtils::addTextButton(DatapanelGUI& panel, int category, const std::string& label)
    {
        DataPanelLine_Button* button = panel.setLineTextButton("", label, category, 1.0f, "Kenshi_Button2");

        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

        return button;
    }

    DataPanelLine_Button* SquadAutonomyPanelUtils::addToggleButton(DatapanelGUI& panel, int category, const std::string& label, bool selected)
    {
        auto button = panel.setLineToggleButton("", label, category);

        button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
        button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());

        button->button->setStateSelected(selected);

        return button;
    }

    DataPanelLine_CheckBox* SquadAutonomyPanelUtils::addCheckbox(DatapanelGUI& panel, int category, const std::string& label, bool& value)
    {
        auto checkbox = panel.setLineCheckbox(label, &value, category);

        return checkbox;
    }

    DataPanelLine_SliderEditable* SquadAutonomyPanelUtils::addSlider(DatapanelGUI& panel, int category, const std::string& label, bool showLabel, float& value, float min, float max, int precision)
    {
        DataPanelLine_SliderEditable* slider = panel.setLineSliderEditable(label, category, showLabel, min, max, &value);
        slider->setPrecision(precision);

        return slider;
    }

    DataPanelLine_Text* SquadAutonomyPanelUtils::addText(DatapanelGUI& panel, int category, const std::string& label, bool wordWrap, bool textBox)
    {
        DataPanelLine_Text* text = panel.setLineText("", "", category, wordWrap, MyGUI::Align::Left);

        if (textBox)
        {
            text->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
        }

        text->editBox->setCaption(label);

        return text;
    }
}
