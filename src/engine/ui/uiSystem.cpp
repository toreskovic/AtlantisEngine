#include "uiSystem.h"
#include "engine/world.h"

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

    void operator()(Panel& p) { GuiPanel(Rect, Text.c_str()); };

    void operator()(DummyRec& dr) { GuiDummyRec(Rect, Text.c_str()); };
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

void AUiScreen::AnchorElementToElement(UIElement * element, UIElement * anchor)
{
    element->anchorX = anchor->Bounds.x / _targetWidth;
    element->anchorY = anchor->Bounds.y / _targetHeight;
}

void AUiScreen::SetVisible(bool visible)
{
    _visible = visible;
}

bool AUiScreen::GetVisible() const
{
    return _visible;
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

    Vector2 mousePosition = GetMousePosition();
    for (auto& uiElement : _elements)
    {
        if (uiElement.OnPreDraw)
        {
            _needsRedraw = uiElement.OnPreDraw(&uiElement) || _needsRedraw;
        }

        // check if mouse is hovering over element
        if (CheckCollisionPointRec(mousePosition, uiElement.Bounds))
        {
            if (uiElement.OnHover)
            {
                uiElement.OnHover(&uiElement);
            }

            _needsRedraw = true;
        }
        else
        {
            if (uiElement.OnHoverEnd)
            {
                uiElement.OnHoverEnd(&uiElement);
            }
        }
    }

    if (!_needsRedraw)
    {
        return;
    }

    if (_timer)
    {
        if (!_timer())
        {
            return;
        }
        else
        {
            _timer = Timer(1000 / _fps);
        }
    }
    else
    {
        _timer = Timer(1000 / _fps);
    }

    _needsRedraw = false;

    if (_renderTexture.id == 0 || _renderTexture.texture.width != currentWidth || _renderTexture.texture.height != currentHeight)
    {
        if (_renderTexture.id != 0)
        {
            UnloadRenderTexture(_renderTexture);
        }

        _renderTexture = LoadRenderTexture(currentWidth, currentHeight);
    }

    BeginTextureMode(_renderTexture);
    ClearBackground(BLANK);

    for (auto& uiElement : _elements)
    {
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

    EndTextureMode();
}

void SUiSystem::Process(AWorld* world)
{
    for (auto& screen : _screens)
    {
        if (screen._visible == false)
        {
            continue;
        }

        if (screen.World == nullptr)
        {
            screen.World = world;
        }

        screen.Draw();

        // Draw the render texture flipped
        DrawTexturePro(screen._renderTexture.texture,
            Rectangle{ 0, 0, (float)screen._renderTexture.texture.width, (float)-screen._renderTexture.texture.height },
            Rectangle{ 0, 0, (float)screen._renderTexture.texture.width, (float)screen._renderTexture.texture.height },
            Vector2{ 0, 0 }, 0.0f, WHITE);
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
