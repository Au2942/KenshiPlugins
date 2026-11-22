#include "SquadAutonomyPanelUtils.h"

#include <mygui/MyGUI_Button.h>
#include <mygui/MyGUI_EditBox.h>
#include <mygui/MyGUI_ScrollBar.h>
#include <mygui/MyGUI_ComboBox.h>

namespace SquadAutonomy
{
    DataPanelLine_Button* SquadAutonomyPanelUtils::addTextButton(DatapanelGUI& panel, int category, const std::string& text, const std::string& label, bool textbox)
    {
        DataPanelLine_Button* button = panel.setLineTextButton(text, label, category, 1.0f, "Kenshi_Button2");
        if (!text.empty())
        {
            button->w1->setVisible(true);
            button->button->setRealSize(0.3, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
            button->button->setRealPosition(1.0 - static_cast<float>(button->w1->getLeft())/button->button->getParent()->getWidth() - 0.3, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
            if (textbox)
            {
                button->w1->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
                button->w1->setCaption(text);
                button->w1->setSize(button->button->getLeft() - button->w1->getLeft(), button->w1->getHeight());
            }
        }
        else
        {
            button->button->setRealSize(0.7, static_cast<float>(button->button->getHeight()) / button->button->getParent()->getHeight());
            button->button->setRealPosition(0.15, static_cast<float>(button->button->getTop()) / button->button->getParent()->getHeight());
        }
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

    DataPanelLine_DropBox* SquadAutonomyPanelUtils::addDropBox(DatapanelGUI& panel, int category, const std::string& label, int& indexValue, bool goBox, float width, bool showLabel, bool seperateTextLine, bool textbox)
    {
        if (seperateTextLine)
        {
            addText(panel, category, label, true, textbox, MyGUI::Align::Center);
        }
        auto dropBox = panel.setLineDropBox(label, category, &indexValue, goBox, width);
        if (!showLabel)
        {
            dropBox->w1->setCaption("");
            dropBox->w1->setVisible(false);
            auto comboBox = dropBox->getComboBox();
            comboBox->setSize(comboBox->getParentSize().width-(dropBox->w1->getLeft() * 2), comboBox->getHeight());
            comboBox->setPosition(dropBox->w1->getLeft(), comboBox->getTop());
        }
        return dropBox;
    }

    /*
    DataPanelLine_SliderEditable* SquadAutonomyPanelUtils::addSlider(DatapanelGUI& panel, int category, const std::string& label, bool showLabel, float& value, float min, float max, int precision)
    {
        DataPanelLine_SliderEditable* slider = panel.setLineSliderEditable(label, category, showLabel, min, max, &value);
        slider->setPrecision(precision);

        return slider;
    }
    */

    DataPanelLine_SliderEditable* SquadAutonomyPanelUtils::addSlider(DatapanelGUI& panel, int category, const std::string& label, bool showLabel, float& value, float min, float max, int precision, bool wordWrap, bool seperateTextLine, bool textbox)
    {
        if (seperateTextLine)
        {
            addText(panel, category, label, wordWrap, textbox, MyGUI::Align::Center);
        }
        auto slider = panel.setLineSliderEditable(label, category, showLabel, min, max, &value);
        slider->setPrecision(precision);
        if (showLabel && !seperateTextLine && wordWrap)
        {
            int wrapCount = slider->nameText->getTextSize().width / slider->nameText->getTextRegion().width;
            int margin = slider->nameText->getHeight() - slider->nameText->getTextRegion().height;
            slider->nameText->setSize(slider->nameText->getWidth(), margin + slider->nameText->getTextRegion().height * (wrapCount + 1));
            slider->nameText->setEditWordWrap(true);
            int nameTextMid = slider->nameText->getTop() + slider->nameText->getHeight() * 0.5;
            slider->sliderBar->setPosition(slider->sliderBar->getLeft(), nameTextMid - slider->sliderBar->getHeight() * 0.5);
            slider->valueEditBox->setPosition(slider->valueEditBox->getLeft(), nameTextMid - slider->valueEditBox->getHeight() * 0.5);
        }
        //Utils::setCallback(*slider, this, &OptionsTab::writeToSelectedSquadSettings);
        return slider;
    }

    DataPanelLine_Text* SquadAutonomyPanelUtils::addText(DatapanelGUI& panel, int category, const std::string& label, bool wordWrap, bool textBox, MyGUI::Align align)
    {
        DataPanelLine_Text* text = panel.setLineText("", "", category, false, align);

        if (textBox)
        {
            text->editBox->changeWidgetSkin("Kenshi_GenericTextBoxFlat");
        }

        text->editBox->setCaption(label);
        text->editBox->setTextAlign(align);
        if (wordWrap)
        {
            int wrapCount = text->editBox->getTextSize().width / text->editBox->getTextRegion().width;
            int margin = text->editBox->getHeight() - text->editBox->getTextRegion().height;
            text->editBox->setSize(text->editBox->getWidth(), margin + text->editBox->getTextRegion().height * (wrapCount + 1));
            text->editBox->setEditWordWrap(true);
        }

        return text;
    }

    std::string SquadAutonomyPanelUtils::getTownBiomeName(TownBase* town)
    {
        std::string biomeName = "";
        if (town->biome && town->biome->data) biomeName = town->biome->data->name;
        std::string townName = town->getKnownName();
        if (!biomeName.empty() && biomeName != "NONE") townName += ", " + biomeName;
        return townName;
    }

}
