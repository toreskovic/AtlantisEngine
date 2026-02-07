#include "helpers.h"
#include "game.h"
#include "engine/core.h"
#include "engine/reflection/reflectionHelpers.h"
#include "engine/world.h"
#include "engine/renderer/renderer.h"
#include "timer.h"
#include "fmt/core.h"

#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

using namespace Atlantis;

AWorld* World = nullptr;
SRenderer* _renderer = nullptr;
SUiSystem* _uiSystem = nullptr;

extern "C" {

    LIB_EXPORT void SetWorld(AWorld *world)
    {
        World = world;
    }

    LIB_EXPORT void Init()
    {
        SetWindowTitle("AtlantisEngine - BunnyMark: Lua Edition");
        SetWindowState(FLAG_WINDOW_RESIZABLE);

        _renderer = new SRenderer();
        _renderer->Labels.insert("Render");
        World->RegisterSystem(_renderer, {"EndRender"});

        _uiSystem = &World->UiSystem;

        World->ResourceHolder.LoadGuiStyle("Assets/styles/cyber/style_cyber.rgs");

        auto* screen = _uiSystem->AddScreen(AUiScreen{});

        auto *panelBg = screen->AddElement(
            {Rectangle{ 64 - 16 - 8, 64 - 16 - 8, 300 + 16, 100 + 16 }, "", DummyRec{}});

        auto *panel = screen->AddElement(
            {Rectangle{ 64 - 16, 64 - 16, 300, 100 }, "", GroupBox{}});
        panel->Text = "Stats";

        auto* btn = screen->AddElement(
            {Rectangle{ 1920 - 300 - 64, 128, 300, 50 }, "Test Button", Button{}});
        btn->anchorX = 1.0f;

        btn->OnClick = [](UIElement* e)
        {
            e->Text = "Clicked!";
        };

        static float fps = 0.0f;

        auto* label = screen->AddElement(
            {Rectangle{ 64, 64, 300, 32 }, "FPS", Label{}});
        
        label->OnPreDraw = [](UIElement* e)
        {
            static auto timer = Timer(100);
            static float fpsAggregator = 0.0f;
            static int fpsCounter = 0;

            fpsAggregator += 1.0f / World->GetDeltaTime();
            fpsCounter++;
            if (timer())
            {
                float currentFps = fpsAggregator / fpsCounter;
                fps = fps * 0.7f + currentFps * 0.3f;

                fpsAggregator = 0.0f;
                fpsCounter = 0;
                timer = Timer(100);
            }

            auto fpsStr = fmt::format("FPS: {:.2f}", fps);

            e->Text = fpsStr;
            return true;
        };

        auto* label2 = screen->AddElement(
            {Rectangle{ 64, 64 + 32, 300, 32 }, "Bunnies", Label{}});
        
        label2->OnPreDraw = [](UIElement* e)
        {
            static const AName entityType = AName("AEntity");
            size_t count = World->GetObjectCountByType(entityType);

            auto bunnyStr = fmt::format("Bunnies: {}", count);

            e->Text = bunnyStr;
            return true;
        };
    }

    LIB_EXPORT void Unload()
    {
    }

    LIB_EXPORT void OnShutdown()
    {
    }

    LIB_EXPORT void PreHotReload()
    {
    }

    LIB_EXPORT void PostHotReload()
    {
    }

    LIB_EXPORT void RegisterTypes()
    {
    }

} // extern "C"