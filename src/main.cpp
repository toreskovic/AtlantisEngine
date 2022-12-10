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
#include <iostream>
#include <fstream>
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

#include <omp.h>
//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
using namespace Atlantis;

int screenWidth = 800;
int screenHeight = 450;

dylib *LibPtr = nullptr;

std::string LibName = "AtlantisGame";
std::string LibDir;
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
ARegistry _registry;

void RegisterTypes()
{
    _registry.RegisterDefault<AEntity>();
    _registry.RegisterDefault<CPosition>();
    _registry.RegisterDefault<CColor>();
    _registry.RegisterDefault<CVelocity>();
    _registry.RegisterDefault<CRenderable>();
}

void RegisterSystems()
{
    _registry.RegisterSystem([](ARegistry *)
                             {
        BeginDrawing();
        ClearBackground(RAYWHITE); },
                             {"BeginRender"}, {"Render"});

    _registry.RegisterSystem([](ARegistry *registry)
                             { EndDrawing(); },
                             {"EndRender"});
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
    auto cpuThreadCount = omp_get_num_procs();
    if (cpuThreadCount == 0)
    {
        cpuThreadCount = 1;
        std::cout << "CPU multithreading not detected!";
    }
    else
    {
        std::cout << "CPU threads detected: " << cpuThreadCount << std::endl;
        // TODO: find a better way for this, arbitrary
        if (cpuThreadCount > 10)
        {
            cpuThreadCount /= 2;
        }

        std::cout << "Setting used CPU thread count to: " << cpuThreadCount << std::endl;
    }

    omp_set_num_threads(cpuThreadCount);

    std::ifstream projectFile("./project.aeng");
    std::getline(projectFile, LibName);

    LibDir = "./projects/" + LibName;

    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(screenWidth, screenHeight, "AtlantisEngine");

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

    CPosition p;
    AClassData d = p.GetClassData();
    std::cout << d.Properties[0].Offset << std::endl;
    std::cout << d.Properties[1].Offset << std::endl;

    // PreHotReload();
    // PostHotReload();

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
    _registry.ResourceHolder.Resources.clear();
    //--------------------------------------------------------------------------------------
    CloseWindow(); // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    if (LibPtr != nullptr)
    {
        auto onShutdown = LibPtr->get_function<void()>("OnShutdown");
        onShutdown();
    }
    
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
        auto unload = LibPtr->get_function<void()>("Unload");
        unload();

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
    RegisterSystems();

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

bool GameInitialized = false;

void DoDylibTest()
{
    HotReloadTimer = []()
    { return false; };

    if (GameInitialized)
    {
        PreHotReload();
        auto preHotReload = LibPtr->get_function<void()>("PreHotReload");
        preHotReload();
    }

    TryUnloadDylib();

    // copy lib to temp one
    LibTempName = LibName + std::to_string(rand());

    std::filesystem::copy_file(FinalLibName, LibDir + DirSlash + LibTempName);

    // load lib
    LibPtr = new dylib(LibDir, LibTempName, false);

    auto setRegistry = LibPtr->get_function<void(ARegistry *)>("SetRegistry");
    setRegistry(&_registry);

    if (GameInitialized)
    {
        PostHotReload();
        auto postHotReload = LibPtr->get_function<void()>("PostHotReload");
        postHotReload();
    }

    auto printer = LibPtr->get_function<void()>("print_hello");
    printer();

    auto printer2 = LibPtr->get_function<void()>("print_helper");
    printer2();

    if (!GameInitialized)
    {
        GameInitialized = true;

        auto init = LibPtr->get_function<void()>("Init");
        init();
    }
}
