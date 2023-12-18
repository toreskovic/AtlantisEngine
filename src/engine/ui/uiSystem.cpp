#include "uiSystem.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

namespace Atlantis
{
    
struct UiVisitor
{
    Rectangle Rect;
    std::string Text;
    std::function<void()> OnClick;

    void operator()(Button& b)
    {
        if (GuiButton(Rect, Text.c_str()))
        {
            if (OnClick)
            {
                OnClick();
            }
        }
    }

    void operator()(Label& l) { GuiLabel(Rect, Text.c_str()); }

    void operator()(LabelButton& lb) { GuiLabelButton(Rect, Text.c_str()); }

    void operator()(Toggle& t) { GuiToggle(Rect, Text.c_str(), &t.active); }

    void operator()(ToggleGroup& tg)
    {
        GuiToggleGroup(Rect, Text.c_str(), &tg.active);
    }

    void operator()(ToggleSlider& ts)
    {
        GuiToggleSlider(Rect, Text.c_str(), &ts.active);
    }

    void operator()(CheckBox& cb)
    {
        GuiCheckBox(Rect, Text.c_str(), &cb.checked);
    }

    void operator()(ComboBox& cb)
    {
        GuiComboBox(Rect, Text.c_str(), &cb.active);
    }
};

UIElement* AUiScreen::AddElement(UIElement element)
{
    _elements.push_back(element);
    UIElement* ptr = &_elements.back();
    return ptr;
}

void AUiScreen::RemoveElement(UIElement element)
{
}

void AUiScreen::Draw()
{
    UiVisitor uiVisitor;

    int currentWidth = GetScreenWidth();
    int currentHeight = GetScreenHeight();

    float scaleX = (float)currentWidth / (float)_targetWidth;
    float scaleY = (float)currentHeight / (float)_targetHeight;

    float scale = 1.0f;

    // scale to fit
    if (scaleX > scaleY)
    {
        scale = scaleY;
    }
    else
    {
        scale = scaleX;
    }

    GuiSetStyle(DEFAULT, TEXT_SIZE, 24 * scale);

    for (auto& uiElement : _elements)
    {
        if (uiElement.OnPreDraw)
        {
            uiElement.OnPreDraw(&uiElement);
        }

        if (uiElement.OnClick)
        {
            uiVisitor.OnClick = [this, &uiElement]()
            { World->QueueSystem([&uiElement]() { uiElement.OnClick(&uiElement); }); };
        }
        else
        {
            uiVisitor.OnClick = nullptr;
        }

        // Apply scaling
        uiVisitor.Rect.width = uiElement.Bounds.width * scale;
        uiVisitor.Rect.height = uiElement.Bounds.height * scale;

        // Apply anchor
        float anchorOffsetX = uiElement.Bounds.x - uiElement.anchorX * _targetWidth;
        float anchorOffsetY = uiElement.Bounds.y - uiElement.anchorY * _targetHeight;

        anchorOffsetX *= scaleX;
        anchorOffsetY *= scaleY;

        uiVisitor.Rect.x = uiElement.anchorX * currentWidth + anchorOffsetX;
        uiVisitor.Rect.y = uiElement.anchorY * currentHeight + anchorOffsetY;

        uiVisitor.Text = uiElement.Text;
        std::visit(uiVisitor, uiElement.Element);
    }
}

void SUiSystem::Process(AWorld* world)
{
    for (auto& screen : _screens)
    {
        if (screen.World == nullptr)
        {
            screen.World = world;
        }

        screen.Draw();
    }
}

AUiScreen* SUiSystem::AddScreen(AUiScreen screen)
{
    _screens.push_back(screen);
    return &_screens.back();
}

void SUiSystem::RemoveScreen(AUiScreen screen)
{

}
}
