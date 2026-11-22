#pragma once

#include <string>

#include <extern/AreaBiomeGroup.h>
#include <kenshi/Town.h>
#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DatapanelLine.h>

namespace SquadAutonomy
{
namespace SquadAutonomyPanelUtils
{
    DataPanelLine_Button* addTextButton(DatapanelGUI& panel, int category, const std::string& text, const std::string& label, bool textbox = false);
    DataPanelLine_Button* addToggleButton(DatapanelGUI& panel, int category, const std::string& label, bool selected);
    //DataPanelLine_TextEditable* addTextEdit(DatapanelGUI& panel, int cateogry, const std::string& label);
    DataPanelLine_CheckBox* addCheckbox(DatapanelGUI& panel, int category, const std::string& label, bool& value);
    DataPanelLine_DropBox* addDropBox(DatapanelGUI& panel, int category, const std::string& label, int& indexValue, bool goBox = false, float width = 1.0, bool showLabel = true, bool seperateTextLine = false, bool textbox = false);
    DataPanelLine_SliderEditable* addSlider(DatapanelGUI& panel, int category, const std::string& label, bool showLabel, float& value, float min, float max, int precision, bool wordWrap = true, bool seperateTextLine = false, bool textbox = false);
    DataPanelLine_Text* addText(DatapanelGUI& panel, int category, const std::string& label, bool wordWrap = true, bool textBox = true, MyGUI::Align align = MyGUI::Align::Left);

    template<typename T>
    void setCallback(DataPanelLine& target, T* object, void (T::*callback)(DataPanelLine*))
    {
        if (object && callback)
        {
            target.callback = MyGUI::newDelegate(object, callback);
        }
    }

    std::string getTownBiomeName(TownBase* town);
}
}
