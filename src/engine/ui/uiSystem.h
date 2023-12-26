#ifndef UISYSTEM_H
#define UISYSTEM_H

#include "raylib.h"
#include "uiElements.h"

#include "engine/reflection/reflectionHelpers.h"
#include "engine/core.h"
#include "engine/system.h"
#include "timer.h"
//#include "./generated/uiSystem.gen.h"

namespace Atlantis
{

    class AUiScreen : public AObject
    {
        friend class SUiSystem;
    public:
        UIElement* AddElement(UIElement element);
        void RemoveElement(UIElement element);
        void AnchorElementToElement(UIElement* element, UIElement* anchor);

        void SetVisible(bool visible);
        bool GetVisible() const;

        void Draw();
    private:
        std::vector<UIElement> _elements;
        int _targetWidth = 1920;
        int _targetHeight = 1080;
        RenderTexture2D _renderTexture;
        bool _needsRedraw = true;
        int _fps = 60;
        TimerFn _timer;
        bool _visible = true;
    };

    class SUiSystem : public ASystem
    {
    public:
        SUiSystem()
        {
            Labels.insert("UiSystem");
            IsRenderSystem = true;
        };

        void Process(AWorld* world) override;

        AUiScreen* AddScreen(AUiScreen screen);

        void RemoveScreen(AUiScreen screen);

    private:
        std::vector<AUiScreen> _screens;
    };
}



#endif // UISYSTEM_H