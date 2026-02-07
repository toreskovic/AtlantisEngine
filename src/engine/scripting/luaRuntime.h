#ifndef LUARUNTIME_H
#define LUARUNTIME_H
#include <sol/sol.hpp>
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include "engine/core.h"
#include "engine/world.h"
#include "engine/renderer/renderer.h"
#include "engine/reflection/reflectionHelpers.h"

namespace Atlantis
{
#define DECLARE_PROPERTY_TYPE(name) static const AName _propType_##name = #name;
#define DECLARE_PROPERTY_TYPE_ATLANTIS(name) static const AName _propType_##name = "Atlantis::" #name;
#define GET_PROPERTY_TYPE(name) _propType_##name

    DECLARE_PROPERTY_TYPE(int)
    DECLARE_PROPERTY_TYPE(float)
    DECLARE_PROPERTY_TYPE(bool)
    DECLARE_PROPERTY_TYPE(string)
    DECLARE_PROPERTY_TYPE_ATLANTIS(AResourceHandle)

#define SCRIPTING_EXPOSE_COMPONENT(name)                             \
    sol::usertype<name> type_##name = Lua.new_usertype<name>(#name); \
    entity_type["GetComponent_" #name] = &AEntity::GetComponentOfType<name>;

    template <>
    sol::object AComponent::GetPropertyScripting<sol::object, sol::stack_object, sol::this_state>(sol::stack_object key, sol::this_state L)
    {
        auto maybe_string_key = key.as<sol::optional<std::string>>();
        if (!maybe_string_key)
        {
            return sol::nil;
        }

        static APropertyData propData;
        static std::string tmpStr;
        static std::unordered_map<std::string, APropertyData> memo;

        tmpStr = *maybe_string_key;

        auto it = memo.find(tmpStr);
        if (it != memo.end())
        {
            propData = it->second;
        }
        else
        {
            const AClassData &classData = GetClassData();
            for (const auto &property : classData.Properties)
            {
                if (property.Name == tmpStr)
                {
                    propData = property;
                    break;
                }
            }

            memo[tmpStr] = propData;
        }

        if (propData.Type == GET_PROPERTY_TYPE(int))
        {
            int *val = reinterpret_cast<int *>((size_t)this + propData.Offset);
            return sol::make_object(L, *val);
        }
        else if (propData.Type == GET_PROPERTY_TYPE(float))
        {
            float *val = reinterpret_cast<float *>((size_t)this + propData.Offset);
            return sol::make_object(L, *val);
        }
        else if (propData.Type == GET_PROPERTY_TYPE(bool))
        {
            bool *val = reinterpret_cast<bool *>((size_t)this + propData.Offset);
            return sol::make_object(L, *val);
        }
        else if (propData.Type == GET_PROPERTY_TYPE(string))
        {
            std::string *val = reinterpret_cast<std::string *>((size_t)this + propData.Offset);
            return sol::make_object(L, *val);
        }
        else if (propData.Type == GET_PROPERTY_TYPE(AResourceHandle))
        {
            AResourceHandle *val = reinterpret_cast<AResourceHandle *>((size_t)this + propData.Offset);
            return sol::make_object(L, *val);
        }

        return sol::nil;
    }

    template <>
    void AComponent::SetPropertyScripting<sol::stack_object, sol::stack_object, sol::this_state>(sol::stack_object key, sol::stack_object value, sol::this_state L)
    {
        auto maybe_string_key = key.as<sol::optional<std::string>>();
        if (!maybe_string_key)
        {
            return;
        }

        AName k = *maybe_string_key;
        const AClassData &classData = GetClassData();

        for (const auto &property : classData.Properties)
        {
            if (property.Name == k)
            {
                if (property.Type == GET_PROPERTY_TYPE(int))
                {
                    SetProperty<int>(k, value.as<int>());
                }
                else if (property.Type == GET_PROPERTY_TYPE(float))
                {
                    SetProperty<float>(k, value.as<float>());
                }
                else if (property.Type == GET_PROPERTY_TYPE(bool))
                {
                    SetProperty<bool>(k, value.as<bool>());
                }
                else if (property.Type == GET_PROPERTY_TYPE(string))
                {
                    SetProperty<std::string>(k, value.as<std::string>());
                }
                else if (property.Type == GET_PROPERTY_TYPE(AResourceHandle))
                {
                    SetProperty<AResourceHandle>(k, value.as<AResourceHandle>());
                }
            }
        }
    }

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
            return World->NewObject_Internal<AEntity>();
        }

        AComponent *NewComponent(const std::string &name)
        {
            AComponent *newComponent = World->NewObject_Internal<AComponent>(name);
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
            ComponentBitset componentMask = World->GetComponentMaskForComponents(components);
            return World->GetEntitiesWithComponents(componentMask);
        }

        void ForEntitiesWithComponents(std::vector<AName> components, sol::function func)
        {
            std::function<void(AEntity *)> f = func;
            ComponentBitset componentMask = World->GetComponentMaskForComponents(components);
            World->ForEntitiesWithComponents(componentMask, f);
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
            Lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math);

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

            //const std::vector<AEntity *> (AWorld::*getEntitiesWithComponents)(std::vector<AName>) = &AWorld::GetEntitiesWithComponents;
            sol::usertype<AWorld> world_type = Lua.new_usertype<AWorld>("AWorld");
            //world_type["GetEntitiesWithComponents"] = getEntitiesWithComponents;

            AComponent *(AEntity::*getComponentOfType)(const AName &name) const = &AEntity::GetComponentOfType;
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
                                                                                    "SetPropertyResourceHandle", &AComponent::SetProperty<AResourceHandle>,
                                                                                    sol::meta_function::index,
                                                                                    &AComponent::GetPropertyScripting<sol::object, sol::stack_object, sol::this_state>,
                                                                                    sol::meta_function::new_index,
                                                                                    &AComponent::SetPropertyScripting<sol::stack_object, sol::stack_object, sol::this_state>);

            // TODO: generate something usable for this from the header parser
            /*SCRIPTING_EXPOSE_COMPONENT(CPosition);
            SCRIPTING_EXPOSE_COMPONENT(CVelocity);
            type_CPosition["x"] = &CPosition::x;
            type_CPosition["y"] = &CPosition::y;
            type_CVelocity["x"] = &CVelocity::x;
            type_CVelocity["y"] = &CVelocity::y;*/
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
#endif