#include "helpers.h"
#include "game.h"
#include "engine/core.h"
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

AWorld* World = nullptr;
SRenderer* _renderer = nullptr;

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

        World->RegisterSystem([](AWorld *world)
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

} // extern "C"