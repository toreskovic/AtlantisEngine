#include "engine/reflection/reflectionHelpers.h"
#include "helpers.h"
#include "game.h"
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

SRenderer *_renderer = nullptr;

AWorld *World = nullptr;

AResourceHandle bunnyHandle;

extern "C"
{
    void createBunny()
    {

        World->QueueNewObject<AEntity>([](AEntity *e)
                                       {
        CPosition *p = World->NewObject_Internal<CPosition>();
        p->x = (float)(rand() % 640);
        p->y = (float)(rand() % 480);

        Color cols[] = {RED, GREEN, BLUE, PURPLE, YELLOW};

        CColor *c = World->NewObject_Internal<CColor>();
        c->col = cols[rand() % 5];

        CRenderable *r = World->NewObject_Internal<CRenderable>();
        r->textureHandle = bunnyHandle;

        CVelocity *v = World->NewObject_Internal<CVelocity>();
        v->x = GetRandomValue(-250, 250);
        v->y = GetRandomValue(-250, 250);

        e->AddComponent(p);
        e->AddComponent(c);
        e->AddComponent(r);
        e->AddComponent(v); });
    };

    void RegisterSystems()
    {
        _renderer = new SRenderer();
        _renderer->Labels.insert("Render");
        World->RegisterSystem(_renderer, {"EndRender"});

        auto bunnySystem = [](AWorld *world)
        {
            world->ForEntitiesWithComponents<CPosition, CVelocity>([](AEntity *e)
                                                                   {
                CVelocity *vel = e->GetComponentOfType<CVelocity>();
                CPosition *pos = e->GetComponentOfType<CPosition>();

                pos->x += vel->x * GetFrameTime();
                pos->y += vel->y * GetFrameTime();

                if (((pos->x + 16) > GetScreenWidth()) || ((pos->x + 16) < 0))
                {
                    vel->x *= -1;
                }
                if (((pos->y + 16) > GetScreenHeight()) || ((pos->y + 16 - 40) < 0))
                {
                    vel->y *= -1;
                } },
                                                                   true);
            /*const auto& entities = world->GetEntitiesWithComponents<CVelocity, CPosition>();

            #pragma omp parallel for
            for (AEntity* e : entities)
            {
                CVelocity *vel = e->GetComponentOfType<CVelocity>();
                CPosition *pos = e->GetComponentOfType<CPosition>();

                pos->x += vel->x * GetFrameTime();
                pos->y += vel->y * GetFrameTime();

                if (((pos->x + 16) > GetScreenWidth()) ||
                    ((pos->x + 16) < 0)) vel->x *= -1;
                if (((pos->y + 16) > GetScreenHeight()) ||
                    ((pos->y + 16 - 40) < 0)) vel->y *= -1;
            }*/
        };

        World->RegisterSystem(bunnySystem,
                              {"Physics"}, {"BeginRender"});
        /*World->RegisterSystem(bunnySystem,
                              {"Physics"}, {"BeginRender"});
        World->RegisterSystem(bunnySystem,
                              {"Physics"}, {"BeginRender"});
        World->RegisterSystem(bunnySystem,
                              {"Physics"}, {"BeginRender"});
        World->RegisterSystem(bunnySystem,
                              {"Physics"}, {"BeginRender"});*/

        static float fps = 0;

        World->RegisterSystem([](AWorld *world)
                              {
            static auto timer = Timer(1000);
            static float fpsAggregator = 0.0f;
            static int fpsCounter = 0;

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

            // count entities that are alive
            auto entities = world->GetEntitiesWithComponents<CPosition, CRenderable>();
            int count = 0;
            for (AEntity *e : entities)
            {
                if (e->_isAlive)
                {
                    count++;
                }
            }

            auto bunnyStr = fmt::format("Bunnies: {}", count);
            textSize = std::max(textSize, MeasureText(bunnyStr.c_str(), fontSize));

            Color bg = DARKGRAY;
            bg.a = 150;

            DrawRectangle(0, 0, textSize + 30, fontSize * 2 + 30, bg);
            DrawText(fpsStr.c_str(), 10, 10, fontSize, LIGHTGRAY);
            DrawText(bunnyStr.c_str(), 10, 30, fontSize, LIGHTGRAY); },
                              {"DebugInfo"}, {"EndRender"});

        World->RegisterSystem([](AWorld *world)
                              {
            if (fps > 60.0f)
            {
                for (int i = 0; i < 100; i++)
                {
                    createBunny();
                }
            } },
                              {"CreateBunny"}, {"Physics"});

        // register a system that will mark for deletion 10 live bunnies if frametime is more than 1 / 60
        World->RegisterSystem([](AWorld *world)
                              {
            if (fps < 60.0f)
            {
                int count = 0;
                for (AEntity *e : world->GetEntitiesWithComponents<CPosition, CRenderable>())
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
            } },
                              {"DeleteBunny"}, {"Physics"});
    }

    LIB_EXPORT void SetWorld(AWorld *world)
    {
        World = world;
    }

    LIB_EXPORT void Init()
    {
        SetWindowTitle("AtlantisEngine - BunnyMark");
        SetWindowState(FLAG_WINDOW_RESIZABLE);

        bunnyHandle = World->ResourceHolder.GetTexture("Assets/wabbit_alpha.png");

        RegisterSystems();
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
