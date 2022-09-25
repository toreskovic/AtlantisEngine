#include "helpers.h"
#include "game.h"
#include "engine/reflection/reflectionHelpers.h"
#include "engine/core.h"
#include "engine/renderer/renderer.h"
#include "timer.h"
#include "fmt/core.h"

#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

using namespace Atlantis;

Renderer *_renderer = nullptr;

Registry *ObjRegistry = nullptr;

AResourceHandle bunnyHandle;

extern "C"
{

    LIB_EXPORT double pi_value = 3.14159;
    LIB_EXPORT void *ptr = (void *)1;

    LIB_EXPORT void print_hello()
    {
        std::cout << "Hello World" << std::endl;
    }

    LIB_EXPORT void print_helper()
    {
        print_test();
    }

    void createBunny()
    {
        Color cols[] = {RED, GREEN, BLUE, PURPLE, YELLOW};

        AEntity *e = ObjRegistry->NewObject<AEntity>(HName("AEntity"));

        position *p = ObjRegistry->NewObject<position>(HName("position"));
        p->x = (float)(rand() % 640);
        p->y = (float)(rand() % 480);

        color *c = ObjRegistry->NewObject<color>(HName("color"));
        c->col = cols[rand() % 5];

        renderable *r = ObjRegistry->NewObject<renderable>(HName("renderable"));
        r->textureHandle = bunnyHandle;

        velocity *v = ObjRegistry->NewObject<velocity>("velocity");
        v->x = GetRandomValue(-250, 250);
        v->y = GetRandomValue(-250, 250);

        e->AddComponent(p);
        e->AddComponent(c);
        e->AddComponent(r);
        e->AddComponent(v);
    };

    void RegisterSystems()
    {
        _renderer = new Renderer();
        _renderer->Labels.insert("Render");
        ObjRegistry->RegisterSystem(_renderer, {"EndRender"});

        ObjRegistry->RegisterSystem([](Registry *registry)
                                    {
        const auto& entities = registry->GetEntitiesWithComponents({"velocity", "position"});

        for (auto it = entities.begin(); it != entities.end(); it++)
        {
            AEntity* e = *it;
            velocity *vel = e->GetComponentOfType<velocity>();
            position *pos = e->GetComponentOfType<position>();

            pos->x += vel->x * GetFrameTime();
            pos->y += vel->y * GetFrameTime();

            if (((pos->x + 16) > GetScreenWidth()) ||
                ((pos->x + 16) < 0)) vel->x *= -1;
            if (((pos->y + 16) > GetScreenHeight()) ||
                ((pos->y + 16 - 40) < 0)) vel->y *= -1;
        } },
                                    {"Physics"}, {"BeginRender"});

        ObjRegistry->RegisterSystem([](Registry *registry)
                                    {
            static auto timer = Timer(1000);
            static float fpsAggregator = 0.0f;
            static int fpsCounter = 0;
            static float fps = 0;

            fpsAggregator += 1.0f / GetFrameTime();
            fpsCounter++;
            if (timer())
            {
                fps = fpsAggregator / fpsCounter;

                fpsAggregator = 0.0f;
                fpsCounter = 0;
                timer = Timer(1000);
            }

            auto fpsStr = fmt::format("FPS: {:.2f}", fps);
            int fontSize = 20;
            int textSize = MeasureText(fpsStr.c_str(), fontSize);

            auto bunnyStr = fmt::format("Bunnies: {}", registry->GetObjectCountByType("AEntity"));
            textSize = std::max(textSize, MeasureText(bunnyStr.c_str(), fontSize));

            Color bg = DARKGRAY;
            bg.a = 150;

            DrawRectangle(0, 0, textSize + 30, fontSize * 2 + 30, bg);
            DrawText(fpsStr.c_str(), 10, 10, fontSize, LIGHTGRAY);
            DrawText(bunnyStr.c_str(), 10, 30, fontSize, LIGHTGRAY); },
                                    {"DebugInfo"}, {"EndRender"});

        ObjRegistry->RegisterSystem([](Registry *registry)
                                    {
            if (GetFrameTime() < 1.0f / 60.0f)
            {
                for (int i = 0; i < 100; i++)
                {
                    createBunny();
                }
            } },
                                    {"CreateBunny"}, {"Physics"});
    }

    LIB_EXPORT void SetRegistry(Registry *registry)
    {
        ObjRegistry = registry;
    }

    LIB_EXPORT void Init()
    {
        SetWindowTitle("AtlantisEngine - BunnyMark");
        SetWindowState(FLAG_WINDOW_RESIZABLE);

        bunnyHandle = ObjRegistry->ResourceHolder.GetTexture("_deps/raylib-src/examples/textures/resources/wabbit_alpha.png");

        RegisterSystems();

        for (int i = 0; i < 1; i++)
        {
            createBunny();
        }
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
        RegisterSystems();
    }

} // extern "C"