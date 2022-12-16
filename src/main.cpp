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

#include "engine/scripting/luaRuntime.h"

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

bool GameLibInitialized = false;

// dylib loading
void InitDylibTest();
void LoadGameLib();
void TryUnloadGameLib();

void PreHotReload();
void PostHotReload();

// engine modules
AWorld World;

ALuaRuntime LuaRuntime;

void RegisterTypes()
{
    World.RegisterDefault<AEntity>();
    World.RegisterDefault<CPosition>();
    World.RegisterDefault<CColor>();
    World.RegisterDefault<CVelocity>();
    World.RegisterDefault<CRenderable>();
}

void RegisterSystems()
{
    World.RegisterSystem([](AWorld *world)
                         {
        BeginDrawing();
        ClearBackground(RAYWHITE); },
                         {"BeginRender"}, {"Render"});

    World.RegisterSystem([](AWorld *world)
                         { EndDrawing(); },
                         {"EndRender"});
}

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

    // Raylib Initialization
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
    

    LuaRuntime.InitLua();
    LuaRuntime.SetWorld(&World);
    LuaRuntime.RunScript(LibDir + DirSlash + "lua" + DirSlash + "main.lua");

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else

    // Main game loop
    //--------------------------------------------------------------------------------------
    while (!WindowShouldClose())
    {
        if (HotReloadTimer())
        {
            LoadGameLib();
        }

        World.ProcessSystems();
    }
#endif

    // De-Initialization
    World.ResourceHolder.Resources.clear();
    LuaRuntime.UnloadLua();
    CloseWindow();
    //--------------------------------------------------------------------------------------

    if (LibPtr != nullptr)
    {
        auto onShutdown = LibPtr->get_function<void()>("OnShutdown");
        onShutdown();
    }

    TryUnloadGameLib();
    return 0;
}

void InitDylibTest()
{
    srand(time(NULL));
    LoadGameLib();
}

void TryUnloadGameLib()
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
    const auto &entities = World.GetObjectsByName("AEntity");

    nlohmann::json serialized;
    serialized["Entities"] = {};
    for (auto &entity : entities)
    {
        serialized["Entities"].push_back(entity->Serialize());
    }

    _serialized = serialized;
}

void PostHotReload()
{
    World.Clear();
    RegisterTypes();
    RegisterSystems();

    nlohmann::json serialized = _serialized;
    for (auto &ent : serialized["Entities"])
    {
        AEntity *e = World.NewObject<AEntity>(AName("AEntity"));

        for (auto &comp : ent["Components"])
        {
            AComponent *component = World.NewObject<AComponent>(comp["Name"].get<std::string>());
            component->Deserialize(comp);

            e->AddComponent(component);
        }
    }

    std::cout << "posthotreload" << std::endl;
}

void LoadGameLib()
{
    HotReloadTimer = []()
    { return false; };

    if (GameLibInitialized)
    {
        PreHotReload();
        auto preHotReload = LibPtr->get_function<void()>("PreHotReload");
        preHotReload();
    }

    TryUnloadGameLib();

    // copy lib to temp one
    LibTempName = LibName + std::to_string(rand());

    std::filesystem::copy_file(FinalLibName, LibDir + DirSlash + LibTempName);

    // load lib
    LibPtr = new dylib(LibDir, LibTempName, false);

    auto setWorld = LibPtr->get_function<void(AWorld *)>("SetWorld");
    setWorld(&World);

    if (GameLibInitialized)
    {
        PostHotReload();
        auto postHotReload = LibPtr->get_function<void()>("PostHotReload");
        postHotReload();
    }

    if (!GameLibInitialized)
    {
        GameLibInitialized = true;

        auto init = LibPtr->get_function<void()>("Init");
        init();
    }
}
