#pragma once

#include <string>

#include <kenshi/gui/DatapanelGUI.h>
#include <kenshi/gui/DatapanelLine.h>

namespace SquadAutonomy
{
namespace SquadAutonomyPanelUtils
{
    DataPanelLine_Button* addTextButton(DatapanelGUI& panel, int category, const std::string& label);
    DataPanelLine_Button* addToggleButton(DatapanelGUI& panel, int category, const std::string& label, bool selected);
    DataPanelLine_CheckBox* addCheckbox(DatapanelGUI& panel, int category, const std::string& label, bool& value);
    DataPanelLine_SliderEditable* addSlider(DatapanelGUI& panel, int category, const std::string& label, bool showLabel, float& value, float min, float max, int precision);
    DataPanelLine_Text* addText(DatapanelGUI& panel, int category, const std::string& label, bool wordWrap = true, bool textBox = true);

    template<typename T>
    void setCallback(DataPanelLine& target, T* object, void (T::*callback)(DataPanelLine*))
    {
        if (object && callback)
        {
            target.callback = new MyGUI::delegates::CMethodDelegate1<T, DataPanelLine*>(
                MyGUI::delegates::GetDelegateUnlink(object),
                object,
                callback
            );
        }
    }
}
}
