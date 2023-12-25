#ifndef UIELEMENTS_H
#define UIELEMENTS_H

#include "raylib.h"
#include <string>
#include <variant>
#include <vector>
#include <functional>

namespace Atlantis
{

//////////////////////////////////////////////
// Panels
struct WindowBox
{
    // WindowBox specific properties
};

struct GroupBox
{
    // GroupBox specific properties
};

struct Line
{
    // Line specific properties
};

struct Panel
{
    // Panel specific properties
};

struct TabBar
{
    std::vector<std::string> text;
    int count;
    int* active;
};

struct ScrollPanel
{
    std::string text;
    Rectangle content;
    Vector2* scroll;
    Rectangle* view;
};
//////////////////////////////////////////////
// Basic controls
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

struct DropdownBox
{
    int* active;
    bool editMode;
};

struct Spinner
{
    int* value;
    int minValue;
    int maxValue;
    bool editMode;
};

struct ValueBox
{
    int* value;
    int minValue;
    int maxValue;
    bool editMode;
};

struct TextBox
{
    int textSize;
    bool editMode;
};

struct Slider
{
    std::string textLeft;
    std::string textRight;
    float* value;
    float minValue;
    float maxValue;
};

struct SliderBar
{
    std::string textLeft;
    std::string textRight;
    float* value;
    float minValue;
    float maxValue;
};

struct ProgressBar
{
    std::string textLeft;
    std::string textRight;
    float* value;
    float minValue;
    float maxValue;
};

struct StatusBar
{
};

struct DummyRec
{
};

struct Grid
{
    float spacing;
    int subdivs;
    Vector2* mouseCell;
};

//////////////////////////////////////////////
// Advanced controls
struct ListView
{
    int* scrollIndex;
    int* active;
};

struct ListViewEx
{
    std::vector<std::string> text;
    int count;
    int* scrollIndex;
    int* active;
    int* focus;
};

struct MessageBox
{
    std::string message;
    std::string buttons;
};

struct TextInputBox
{
    std::string message;
    std::string buttons;
    char* text;
    int textMaxSize;
    bool* secretViewActive;
};

struct ColorPicker
{
    Color* color;
};

struct ColorPanel
{
    Color* color;
};

struct ColorBarAlpha
{
    float* alpha;
};

struct ColorBarHue
{
    float* value;
};

struct ColorPickerHSV
{
    Vector3* colorHsv;
};

struct ColorPanelHSV
{
    Vector3* colorHsv;
};

//////////////////////////////////////////////
typedef std::variant<
//    WindowBox,
//    GroupBox,
//    Line,
    Panel,
//    TabBar,
//    ScrollPanel,
//    DropdownBox,
//    Spinner,
//    ValueBox,
//    TextBox,
//    Slider,
//    SliderBar,
//    ProgressBar,
//    StatusBar,
    DummyRec,
//    Grid,
//    ListView,
//    ListViewEx,
//    MessageBox,
//    TextInputBox,
//    ColorPicker,
//    ColorPanel,
//    ColorBarAlpha,
//    ColorBarHue,
//    ColorPickerHSV,
//    ColorPanelHSV,
    Label,
    Button,
    LabelButton,
    Toggle,
    ToggleGroup,
    ToggleSlider,
    CheckBox,
    ComboBox
> UiControlVariant;


struct UIElement
{
    Rectangle Bounds;
    std::string Text;
    UiControlVariant Element;
    std::vector<UIElement> Children;
    std::function<bool(UIElement*)> OnPreDraw;
    std::function<void(UIElement*)> OnClick;
    std::function<void(UIElement*)> OnHover;
    std::function<void(UIElement*)> OnHoverEnd;

    float anchorX = 0.0f;
    float anchorY = 0.0f;

    UIElement(Rectangle bounds, std::string text, UiControlVariant element)
        : Bounds(bounds), Text(text), Element(element)
    {
    }
};
}
#endif // UIELEMENTS_H