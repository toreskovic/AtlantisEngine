#ifndef UIELEMENTS_H
#define UIELEMENTS_H

#include "raylib.h"
#include <string>
#include <variant>
#include <vector>
#include <functional>

namespace Atlantis
{

struct Label
{
    // Label specific properties
};

struct Button
{
    // Button specific properties
};

struct LabelButton
{
    // LabelButton specific properties
};

struct Toggle
{
    bool active;
};

struct ToggleGroup
{
    int active;
};

struct ToggleSlider
{
    int active;
};

struct CheckBox
{
    bool checked;
};

struct ComboBox
{
    int active;
};

struct UIElement
{
    Rectangle Bounds;
    std::string Text;
    std::variant<Label,
                 Button,
                 LabelButton,
                 Toggle,
                 ToggleGroup,
                 ToggleSlider,
                 CheckBox,
                 ComboBox>
        Element;
    std::vector<UIElement> Children;
    std::function<void(UIElement*)> OnPreDraw;
    std::function<void(UIElement*)> OnClick;
    std::function<void(UIElement*)> OnHover;
    std::function<void(UIElement*)> OnHoverEnd;

    float anchorX = 0.0f;
    float anchorY = 0.0f;

    UIElement(Rectangle bounds, std::string text,
              std::variant<Label,
                           Button,
                           LabelButton,
                           Toggle,
                           ToggleGroup,
                           ToggleSlider,
                           CheckBox,
                           ComboBox>
                  element
                  )
        : Bounds(bounds), Text(text), Element(element)
    {
    }
};
}
#endif // UIELEMENTS_H