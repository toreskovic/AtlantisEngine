#include <sol/sol.hpp>
#include <iostream>
#include <filesystem>
#include "engine/core.h"
#include "engine/reflection/reflectionHelpers.h"


namespace Atlantis
{
    struct CLuaComponent : public AComponent
    {
        sol::table LuaTable;
    };

    struct ALuaWorld
    {
        AWorld *World = nullptr;
        sol::state *Lua = nullptr;

        void SetState(sol::state *lua)
        {
            Lua = lua;
        }

        void RegisterEntity(const std::string &name)
        {
            World->RegisterDefault<AEntity>(name);
        }

        void RegisterComponent(const std::string &name)
        {
            World->RegisterDefault<CLuaComponent>(name);
        }

        void RegisterSystem(sol::function func, std::vector<AName> labels, std::vector<AName> beforeLabels)
        {
            World->RegisterSystem([func](AWorld *world)
                                  {
                std::function<void(AWorld*)> f = func;
                f(world); },
                                  labels, beforeLabels);
        }

        AEntity *NewEntity()
        {
            return World->NewObject<AEntity>();
        }

        AComponent* NewComponent(const std::string &name)
        {
            AComponent *newComponent = World->NewObject<AComponent>(name);
            return newComponent;
        }

        void AddComponentToEntity(AEntity *entity, AComponent *component)
        {
            entity->AddComponent(component);
        }

        AResourceHandle GetTexture(const std::string &name)
        {
            return World->ResourceHolder.GetTexture(name);
        }

        std::vector<AEntity *> GetEntitiesWithComponents(std::vector<AName> components)
        {
            return World->GetEntitiesWithComponents(components);
        }

        void ForEntitiesWithComponents(std::vector<AName> components, sol::function func)
        {
            World->ForEntitiesWithComponents(components, [func](AEntity *e)
                                             {
                std::function<void(AEntity*)> f = func;
                f(e); });
        }

        float GetDeltaTime()
        {
            return GetFrameTime();
        }
    };

    struct ALuaRuntime
    {
        sol::state Lua;
        ALuaWorld LuaWorld;

        void RunScript(const std::string &file)
        {
            if (std::filesystem::exists(file))
            {
                Lua.script_file(file);
                Lua.script("Init()");
            }
            else
            {
                std::cout << "Lua file " << file << " does not exist! " << std::endl;
            }
        }

        void InitLua()
        {
            // open some common libraries
            Lua.open_libraries(sol::lib::base, sol::lib::package);

            Lua.set_function("RegisterEntity", &ALuaWorld::RegisterEntity, &LuaWorld);
            Lua.set_function("RegisterComponent", &ALuaWorld::RegisterComponent, &LuaWorld);
            Lua.set_function("RegisterSystem", &ALuaWorld::RegisterSystem, &LuaWorld);
            Lua.set_function("NewEntity", &ALuaWorld::NewEntity, &LuaWorld);
            Lua.set_function("NewComponent", &ALuaWorld::NewComponent, &LuaWorld);
            Lua.set_function("AddComponentToEntity", &ALuaWorld::AddComponentToEntity, &LuaWorld);
            Lua.set_function("GetTexture", &ALuaWorld::GetTexture, &LuaWorld);
            Lua.set_function("GetEntitiesWithComponents", &ALuaWorld::GetEntitiesWithComponents, &LuaWorld);
            Lua.set_function("ForEntitiesWithComponents", &ALuaWorld::ForEntitiesWithComponents, &LuaWorld);
            Lua.set_function("GetDeltaTime", &ALuaWorld::GetDeltaTime, &LuaWorld);

            sol::usertype<AName> name_type = Lua.new_usertype<AName>("AName",
                                                                     sol::constructors<AName(),
                                                                                       AName(std::string),
                                                                                       AName(const char *)>());

            const std::vector<AEntity *> (AWorld::*getEntitiesWithComponents)(std::vector<AName>) = &AWorld::GetEntitiesWithComponents;
            sol::usertype<AWorld> world_type = Lua.new_usertype<AWorld>("AWorld");
            world_type["GetEntitiesWithComponents"] = getEntitiesWithComponents;

            AComponent *(AEntity::*getComponentOfType)(const AName &name) = &AEntity::GetComponentOfType;
            sol::usertype<AEntity> entity_type = Lua.new_usertype<AEntity>("AEntity");
            entity_type["AddComponent"] = &AEntity::AddComponent;
            entity_type["GetComponentOfType"] = getComponentOfType;

            sol::usertype<AComponent> component_type = Lua.new_usertype<AComponent>("AComponent",
            "GetPropertyInt", &AComponent::GetProperty<int>,
            "GetPropertyFloat", &AComponent::GetProperty<float>,
            "GetPropertyString", &AComponent::GetProperty<std::string>,
            "GetPropertyBool", &AComponent::GetProperty<bool>,
            "GetPropertyResourceHandle", &AComponent::GetProperty<AResourceHandle>,
            "SetPropertyInt", &AComponent::SetProperty<int>,
            "SetPropertyFloat", &AComponent::SetProperty<float>,
            "SetPropertyString", &AComponent::SetProperty<std::string>,
            "SetPropertyBool", &AComponent::SetProperty<bool>,
            "SetPropertyResourceHandle", &AComponent::SetProperty<AResourceHandle>
            );
        }

        void SetWorld(AWorld *world)
        {
            LuaWorld.World = world;
            LuaWorld.SetState(&Lua);
        }

        void DoLua()
        {
        }

        void UnloadLua()
        {
        }
    };
}