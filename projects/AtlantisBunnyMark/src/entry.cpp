#include "engine/core.h"
#include "engine/world.h"
#include "engine/reflection/reflectionHelpers.h"
#include "engine/renderer/renderer.h"
#include "engine/ui/uiSystem.h"
#include "fmt/core.h"
#include "game.h"
#include "helpers.h"
#include "timer.h"

#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

using namespace Atlantis;

SRenderer* _renderer = nullptr;
SUiSystem* _uiSystem = nullptr;

AWorld* World = nullptr;

AResourceHandle bunnyHandle;

extern "C"
{
    void createBunny()
    {
        World->QueueNewObject<AEntity>(
            [](AObjPtr<AEntity> e)
            {
                CPosition* p = World->NewObject_Internal<CPosition>();
                p->x = (float)(rand() % 640);
                p->y = (float)(rand() % 480);

                Color cols[] = { RED, GREEN, BLUE, PURPLE, YELLOW };

                CColor* c = World->NewObject_Internal<CColor>();
                c->col = cols[rand() % 5];

                CRenderable* r = World->NewObject_Internal<CRenderable>();
                r->textureHandle = bunnyHandle;

                CVelocity* v = World->NewObject_Internal<CVelocity>();
                v->x = GetRandomValue(-250, 250);
                v->y = GetRandomValue(-250, 250);

                e->AddComponent(p);
                e->AddComponent(c);
                e->AddComponent(r);
                e->AddComponent(v);
            });
    };

    void RegisterSystems()
    {
        _renderer = new SRenderer();
        _renderer->Labels.insert("Render");
        World->RegisterSystem(_renderer, { "EndRender" });

        World->RegisterSystemView<CPosition, CRenderable, CColor>();
        World->RegisterSystemView<CPosition, CVelocity>();

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
        
        // panel is not valid here
        //screen->AnchorElementToElement(label, panel);

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
        
        // panel is not valid here
        //screen->AnchorElementToElement(label2, panel);
        
        label2->OnPreDraw = [](UIElement* e)
        {
            static const AName entityType = AName("AEntity");
            size_t count = World->GetObjectCountByType(entityType);

            auto bunnyStr = fmt::format("Bunnies: {}", count);

            e->Text = bunnyStr;
            return true;
        };

        auto bunnySystem = [](AWorld* world)
        {
            world->ForEntitiesWithComponentsParallel(
                [world](AEntity* e, CPosition* pos, CVelocity* vel)
                {
                    pos->x += vel->x * world->GetDeltaTime();
                    pos->y += vel->y * world->GetDeltaTime();

                    if (((pos->x + 16) > 1920 + 64) ||
                        ((pos->x + 16) < 0 -64))
                    {
                        vel->x *= -1;
                    }
                    if (((pos->y + 16) > 1080 + 64) ||
                        ((pos->y + 16 - 40) < 0 - 64))
                    {
                        vel->y *= -1;
                    }
                });
        };

        World->RegisterSystem(bunnySystem, { "Physics" }, { "BeginRender" });

        World->RegisterSystem(
            [](AWorld* world)
            {
                if (fps > 60.0f)
                {
                    for (int i = 0; i < 200; i++)
                    {
                        createBunny();
                    }
                }
            },
            { "CreateBunny" },
            { "Physics" });

        World->RegisterSystem(
            [](AWorld* world)
            {
                if (fps < 60.0f)
                {
                    int count = 0;
                    for (AEntity* e :
                         world->GetEntitiesWithComponents<CPosition,
                                                          CRenderable>())
                    {
                        if (count >= 50)
                        {
                            break;
                        }

                        if (!e->_isAlive)
                        {
                            continue;
                        }

                        world->QueueObjectDeletion(e);
                        count++;
                    }
                }
            },
            { "DeleteBunny" },
            { "Physics" });
    }

    LIB_EXPORT void SetWorld(AWorld* world)
    {
        World = world;
    }

    LIB_EXPORT void Init()
    {
        World->QueueRenderThreadCall([]()
            {
                SetWindowTitle("AtlantisEngine - BunnyMark");
                SetWindowState(FLAG_WINDOW_RESIZABLE);
            });

        bunnyHandle =
            World->ResourceHolder.GetTexture("Assets/wabbit_alpha.png");

        RegisterSystems();
    }

    LIB_EXPORT void Unload() {}

    LIB_EXPORT void OnShutdown() {}

    LIB_EXPORT void PreHotReload() {}

    LIB_EXPORT void PostHotReload()
    {
        RegisterSystems();
    }

    LIB_EXPORT void RegisterTypes() {}

} // extern "C"
