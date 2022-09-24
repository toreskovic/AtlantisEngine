/*******************************************************************************************
 *
 *   raylib [core] example - Basic window (adapted for HTML5 platform)
 *
 *   This example is prepared to compile for PLATFORM_WEB, PLATFORM_DESKTOP and PLATFORM_RPI
 *   As you will notice, code structure is slightly diferent to the other examples...
 *   To compile it for PLATFORM_WEB just uncomment #define PLATFORM_WEB at beginning
 *
 *   This example has been created using raylib 1.3 (www.raylib.com)
 *   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
 *
 *   Copyright (c) 2015 Ramon Santamaria (@raysan5)
 *
 ********************************************************************************************/

#include <filesystem>
#include "raylib.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#include "helpers.h"
#include "dylib.hpp"
#include "third-party/FileWatch.h"
#include "timer.h"
#include "engine/core.h"
#include "engine/renderer/renderer.h"
#include "nlohmann/json.hpp"
#include "fmt/core.h"
//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
using namespace Atlantis;

int screenWidth = 800;
int screenHeight = 450;

dylib *LibPtr = nullptr;

std::string LibName = "AtlantisGame";
std::string LibDir = "./" + LibName;
std::string FinalLibName = "";
std::string DirSlash = "/";

std::function<bool()> HotReloadTimer = []()
{ return false; };

std::string LibTempName = "";

// dylib loading
void InitDylibTest();
void DoDylibTest();
void TryUnloadDylib();

void PreHotReload();
void PostHotReload();

// engine modules
Registry _registry;
Renderer *_renderer = new Renderer();

void RegisterTypes()
{
    _registry.RegisterDefault<AEntity>();
    _registry.RegisterDefault<position>();
    _registry.RegisterDefault<color>();
    _registry.RegisterDefault<velocity>();
    _registry.RegisterDefault<renderable>();
}

void RegisterSystems()
{
    _renderer->Labels.insert("Render");
    _registry.RegisterSystem(_renderer, {"EndRender"});

    _registry.RegisterSystem([](Registry *)
                             {
        BeginDrawing();
        ClearBackground(RAYWHITE); },
                             {"BeginRender"}, {"Render"});

    _registry.RegisterSystem([](Registry *registry)
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
        DrawText(bunnyStr.c_str(), 10, 30, fontSize, LIGHTGRAY);
        EndDrawing(); },
                             {"EndRender"});

    _registry.RegisterSystem([](Registry *registry)
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
}

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
void UpdateDrawFrame(void); // Update and Draw one frame

//----------------------------------------------------------------------------------
// Main Enry Point
//----------------------------------------------------------------------------------
int main()
{
    RegisterTypes();
    RegisterSystems();

    FinalLibName = LibDir + DirSlash + dylib::filename_components::prefix + LibName + dylib::filename_components::suffix;

    InitDylibTest();
    std::cout << FinalLibName << std::endl;
    filewatch::FileWatch<std::string> watch(
        FinalLibName,
        [](const std::string &path, const filewatch::Event change_type)
        {
            HotReloadTimer = Timer(500);
        });

    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

    Texture2D bunnyTex = LoadTexture("_deps/raylib-src/examples/textures/resources/wabbit_alpha.png");

    Registry *_reg = &_registry;

    auto createBunny = [&bunnyTex, _reg]()
    {
        Color cols[] = {RED, GREEN, BLUE, PURPLE, YELLOW};

        AEntity *e = _reg->NewObject<AEntity>(HName("AEntity"));

        position *p = _reg->NewObject<position>(HName("position"));
        p->x = (float)(rand() % screenWidth);
        p->y = (float)(rand() % screenHeight);

        color *c = _registry.NewObject<color>(HName("color"));
        c->col = cols[rand() % 5];

        renderable *r = _reg->NewObject<renderable>(HName("renderable"));
        // r->renderType = GetRandomValue(0, 1) ? "Rectangle" : "Circle";
        r->textureHandle = {(size_t)&bunnyTex};

        velocity *v = _reg->NewObject<velocity>("velocity");
        v->x = GetRandomValue(-250, 250);
        v->y = GetRandomValue(-250, 250);

        e->AddComponent(p);
        e->AddComponent(c);
        e->AddComponent(r);
        e->AddComponent(v);
    };

    for (int i = 0; i < 1; i++)
    {
        createBunny();
    }

    /*auto &comps = _registry.GetAllComponentsByName(HName("renderable"));
    std::cout << "-----------" << std::endl;
    for (auto &comp : comps)
    {
        renderable *ren = static_cast<renderable *>(comp.get());
        std::cout << ren->renderType << std::endl;
    }*/

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    // SetTargetFPS(60); // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    position p;
    ClassData d = p.GetClassData();
    std::cout << d.Properties[0].Offset << std::endl;
    std::cout << d.Properties[1].Offset << std::endl;

    PreHotReload();
    PostHotReload();

    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        if (HotReloadTimer())
        {
            DoDylibTest();
        }

        UpdateDrawFrame();

        if (GetFrameTime() < 1.0f / 60.0f)
        {
            for (int i = 0; i < 100; i++)
            {
                createBunny();
            }
        }
    }
#endif

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(bunnyTex);
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    TryUnloadDylib();
    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
void UpdateDrawFrame(void)
{
    // Update
    //----------------------------------------------------------------------------------
    // TODO: Update your variables here
    //----------------------------------------------------------------------------------

    // Draw
    //----------------------------------------------------------------------------------
    _registry.ProcessSystems();
    //----------------------------------------------------------------------------------
}

void InitDylibTest()
{
    srand(time(NULL));
    DoDylibTest();
}

void TryUnloadDylib()
{
    if (!LibTempName.empty() && LibPtr != nullptr)
    {
        std::cout << "Unloading old lib" << std::endl;
        std::filesystem::remove(LibDir + DirSlash + LibTempName);
        // unload lib
        delete LibPtr;
        LibPtr = nullptr;
    }
}

nlohmann::json _serialized;

void PreHotReload()
{
    const auto &entities = _registry.GetObjectsByName("AEntity");

    nlohmann::json serialized;
    serialized["Entities"] = {};
    for (auto &entity : entities)
    {
        serialized["Entities"].push_back(entity->Serialize());
    }

    // std::cout << std::setw(4) << serialized << std::endl;
    _serialized = serialized;
}

void PostHotReload()
{
    _registry.Clear();
    RegisterTypes();

    nlohmann::json serialized = _serialized;
    // std::cout << std::setw(4) << serialized << std::endl;
    for (auto &ent : serialized["Entities"])
    {
        AEntity *e = _registry.NewObject<AEntity>(HName("AEntity"));

        for (auto &comp : ent["Components"])
        {
            AComponent *component = _registry.NewObject<AComponent>(comp["Name"].get<std::string>());
            component->Deserialize(comp);

            e->AddComponent(component);
        }
    }

    std::cout << "posthotreload" << std::endl;
}

void DoDylibTest()
{
    HotReloadTimer = []()
    { return false; };

    PreHotReload();

    TryUnloadDylib();

    // copy lib to temp one
    LibTempName = LibName + std::to_string(rand());

    std::filesystem::copy_file(FinalLibName, LibDir + DirSlash + LibTempName);

    // load lib
    LibPtr = new dylib(LibDir, LibTempName, false);

    PostHotReload();

    auto printer = LibPtr->get_function<void()>("print_hello");
    printer();

    auto printer2 = LibPtr->get_function<void()>("print_helper");
    printer2();
}
