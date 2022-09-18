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
//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
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
Renderer _renderer(_registry);

void RegisterTypes()
{
    _registry.RegisterDefault<AEntity>();
    _registry.RegisterDefault<position>();
    _registry.RegisterDefault<color>();
    _registry.RegisterDefault<velocity>();
    _registry.RegisterDefault<renderable>();
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

    FinalLibName = LibDir + DirSlash + dylib::filename_components::prefix + LibName + dylib::filename_components::suffix;

    InitDylibTest();
    std::cout << FinalLibName << std::endl;
    filewatch::FileWatch<std::string> watch(
        FinalLibName,
        [](const std::string &path, const filewatch::Event change_type)
        {
            std::cout << path << " : ";
            switch (change_type)
            {
            case filewatch::Event::added:
                std::cout << "The file was added to the directory." << '\n';
                break;
            case filewatch::Event::removed:
                std::cout << "The file was removed from the directory." << '\n';
                break;
            case filewatch::Event::modified:
                std::cout << "The file was modified. This can be a change in the time stamp or attributes." << '\n';
                break;
            case filewatch::Event::renamed_old:
                std::cout << "The file was renamed and this is the old name." << '\n';
                break;
            case filewatch::Event::renamed_new:
                std::cout << "The file was renamed and this is the new name." << '\n';
                break;
            };

            HotReloadTimer = Timer(500);
        });

    for (int i = 0; i < 100; i++)
    {
        Color cols[] = {RED, GREEN, BLUE, PURPLE, YELLOW};

        AEntity* e = _registry.NewObject<AEntity>(HName("AEntity"));

        position* p = _registry.NewObject<position>(HName("position"));
        p->x = (float)(rand() % screenWidth);
        p->y = (float)(rand() % screenHeight);

        color* c = _registry.NewObject<color>(HName("color"));
        c->col = cols[rand() % 5];

        renderable* r = _registry.NewObject<renderable>(HName("renderable"));
        r->renderType = rand() % 2 ? "Rectangle" : "Circle";

        e->AddComponent(p);
        e->AddComponent(c);
        e->AddComponent(r);
    }

    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60); // Set our game to run at 60 frames-per-second
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
    }
#endif

    // De-Initialization
    //--------------------------------------------------------------------------------------
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
    BeginDrawing();

    ClearBackground(RAYWHITE);

    _renderer.Render(_registry);

    DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);

    EndDrawing();
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
    const auto& entities = _registry.GetAllComponentsByName("AEntity");

    nlohmann::json serialized;
    serialized["Entities"] = {};
    for (auto& entity : entities)
    {
        serialized["Entities"].push_back(entity->Serialize());
    }
    
    //std::cout << std::setw(4) << serialized << std::endl;
    _serialized = serialized;
}

void PostHotReload()
{
    _registry.Clear();
    RegisterTypes();

    nlohmann::json serialized = _serialized;
    for (auto& ent : serialized["Entities"])
    {
        AEntity* e = _registry.NewObject<AEntity>(HName("AEntity"));

        for (auto& comp : ent["Components"])
        {
            AComponent* component = _registry.NewObject<AComponent>(comp["Name"].get<std::string>());
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
