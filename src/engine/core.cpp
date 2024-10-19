#include "core.h"
#include "engine/world.h"
#include "raygui.h"
#include "system.h"
#include <iostream>
#include <vector>

#define SERIALIZE_PROP_HELPER(type)                                            \
    if (propData.Type == #type)                                                \
    {                                                                          \
        type prop = GetProperty<type>(propData.Name);                          \
        type defProp = cdo->GetProperty<type>(propData.Name);                  \
        propJson["Value"] = prop;                                              \
        propJson["IsDefault"] = Helper_IsEqual(prop, defProp);                 \
    }

#define DESERIALIZE_PROP_HELPER(type)                                          \
    if (prop["Type"].get<std::string>() == #type)                              \
    {                                                                          \
        SetProperty(prop["Name"].get<std::string>(),                           \
                    prop["Value"].get<type>());                                \
    }

// On Linux, Atlantis types in reflection data are namespaced in the form of
// "Atlantis::type", while on Windows, they appear as just "type"
#if defined(_WIN32)
#define SERIALIZE_PROP_HELPER_ATLANTIS(type) SERIALIZE_PROP_HELPER(type)
#define DESERIALIZE_PROP_HELPER_ATLANTIS(type) DESERIALIZE_PROP_HELPER(type)
#else
#define SERIALIZE_PROP_HELPER_ATLANTIS(type)                                   \
    SERIALIZE_PROP_HELPER(Atlantis::type)
#define DESERIALIZE_PROP_HELPER_ATLANTIS(type)                                 \
    DESERIALIZE_PROP_HELPER(Atlantis::type)
#endif

namespace Atlantis
{
template<typename T>
bool Helper_IsEqual(const T& l, const T& r)
{
    return l == r;
}

template<>
bool Helper_IsEqual(const Color& l, const Color& r)
{
    return l.r == r.r && l.g == r.g && l.b == r.b && l.a == r.a;
}

template<>
bool Helper_IsEqual(const Texture2D& l, const Texture2D& r)
{
    return l.id == r.id;
}

void AObject::MarkObjectDead()
{
    if (World != nullptr)
    {
        World->MarkObjectDead(this);
    }
}

nlohmann::json AObject::Serialize()
{
    nlohmann::json json;
    const auto& classData = GetClassData();

    json["Name"] = classData.Name.GetName();
    json["Properties"] = nlohmann::json::array({});
    for (const auto& propData : classData.Properties)
    {
        nlohmann::json propJson = { { "Name", propData.Name.GetName() },
                                    { "Type", propData.Type.GetName() },
                                    { "Offset", propData.Offset } };
        const AObject* cdo = World->GetCDO<AObject>(classData.Name);

        SERIALIZE_PROP_HELPER(bool);
        SERIALIZE_PROP_HELPER(int);
        SERIALIZE_PROP_HELPER(float);
        SERIALIZE_PROP_HELPER(double);
        SERIALIZE_PROP_HELPER(Color);
        SERIALIZE_PROP_HELPER(Texture2D);
        SERIALIZE_PROP_HELPER(std::string);
        SERIALIZE_PROP_HELPER_ATLANTIS(AName);
        SERIALIZE_PROP_HELPER_ATLANTIS(AResourceHandle);

        json["Properties"].push_back(propJson);
    }

    return json;
}

void AObject::Deserialize(const nlohmann::json& json)
{
    for (auto& prop : json["Properties"])
    {
        if (prop.contains("IsDefault"))
        {
            if (prop["IsDefault"])
            {
                continue;
            }
        }
        else
        {
            std::cout << "AObject::Deserialize | Warning: Property "
                      << prop["Name"].get<std::string>()
                      << " has no IsDefault field" << std::endl;
        }

        DESERIALIZE_PROP_HELPER(bool);
        DESERIALIZE_PROP_HELPER(int);
        DESERIALIZE_PROP_HELPER(float);
        DESERIALIZE_PROP_HELPER(double);
        DESERIALIZE_PROP_HELPER(Color);
        DESERIALIZE_PROP_HELPER(Texture2D);
        DESERIALIZE_PROP_HELPER(std::string);
        DESERIALIZE_PROP_HELPER_ATLANTIS(AName);
        DESERIALIZE_PROP_HELPER_ATLANTIS(AResourceHandle);
    }
}

void AComponent::MarkObjectDead()
{
    AObject::MarkObjectDead();

    if (Owner != nullptr)
    {
        Owner->RemoveComponent(this);
    }
}

void AEntity::MarkObjectDead()
{
    // remove all components from entity
    for (AComponent* component : Components)
    {
        component->OnRemovedFromEntity(this);
    }

    Components.clear();
    ComponentNames.clear();

    _componentMask.reset();

    AObject::MarkObjectDead();
}

uint32_t AEntity::GetStaticComponentIndex(const AName& name) const
{
    return std::distance(World->ComponentNames.begin(),
                         std::find(World->ComponentNames.begin(),
                                   World->ComponentNames.end(),
                                   name));
}

AComponent* AEntity::GetComponentOfType(const AName& name) const
{
    const uint32_t staticIndex = GetStaticComponentIndex(name);

    if (_componentMask[staticIndex] == 0)
    {
        return nullptr;
    }

    const size_t shift = MAX_COMPONENTS - staticIndex;

    return Components[(_componentMask << shift).count()];
}

void AEntity::AddComponent(AComponent* component)
{
    AName componentName = component->GetClassData().Name;
    auto it = std::lower_bound(
        ComponentNames.begin(), ComponentNames.end(), componentName);
    int index = std::distance(ComponentNames.begin(), it);

    Components.insert(Components.begin() + index, component);
    ComponentNames.insert(it, componentName);

    _componentMask = World->GetComponentMaskForComponents(ComponentNames);

    component->OnAddedToEntity(this);
}

void AEntity::RemoveComponent(AComponent* component)
{
    for (int i = 0; i < Components.size(); i++)
    {
        if (Components[i] == component)
        {
            Components.erase(Components.begin() + i);
            ComponentNames.erase(ComponentNames.begin() + i);
            _componentMask =
                World->GetComponentMaskForComponents(ComponentNames);

            component->OnRemovedFromEntity(this);
            break;
        }
    }
}

bool AEntity::HasComponentOfType(const AName& name)
{
    for (const AName& compName : ComponentNames)
    {
        if (compName == name)
        {
            return true;
        }
    }

    return false;
}

bool AEntity::HasComponentsByMask(const ComponentBitset& mask)
{
    return (mask & _componentMask) == mask;
}

bool AEntity::HasComponentsOfType(const std::vector<AName>& names)
{
    return HasComponentsByMask(World->GetComponentMaskForComponents(names));
}

AResourceHandle AResourceHolder::GetTexture(std::string path)
{
    if (World == nullptr)
    {
        std::cout << "AResourceHolder::GetTexture | Error: World is null"
                  << std::endl;
        return AResourceHandle();
    }

    if (Resources.contains(path))
    {
        AResourceHandle ret(Resources.at(path).get());
        return ret;
    }

    if (World->IsMainThread())
    {
        World->QueueRenderThreadCall(
            [this, path]()
            {
                if (Resources.contains(path))
                {
                    return;
                }

                Resources.emplace(
                    path,
                    std::move(std::make_unique<ATextureResource>(LoadTexture(
                        (Helpers::GetProjectDirectory().string() + path)
                            .c_str()))));
            });
    }
    else
    {
        Resources.emplace(
            path,
            std::move(std::make_unique<ATextureResource>(LoadTexture(
                (Helpers::GetProjectDirectory().string() + path).c_str()))));
    }

    AResourceHandle ret(this, path);
    return ret;
}

AResourceHandle AResourceHolder::GetShader(std::string path,
                                           int variant /*= 0*/)
{
    // add a suffix to the path to differentiate between different shader
    // variants use format "[path]__variant[variant]"
    std::string originalPath = path;
    path += "__variant" + std::to_string(variant);

    if (World == nullptr)
    {
        std::cout << "AResourceHolder::GetTexture | Error: World is null"
                  << std::endl;
        return AResourceHandle();
    }

    if (Resources.contains(path))
    {
        AResourceHandle ret(Resources.at(path).get());
        return ret;
    }

    // the only difference is the extension, .vs for vertex shader and .fs for
    // fragment shader
    std::string vertexShaderPath =
        (Helpers::GetProjectDirectory().string() + originalPath + ".vs");
    std::string fragmentShaderPath =
        (Helpers::GetProjectDirectory().string() + originalPath + ".fs");

    if (World->IsMainThread())
    {

        World->QueueRenderThreadCall(
            [this, path, vertexShaderPath, fragmentShaderPath]()
            {
                if (Resources.contains(path))
                {
                    return;
                }

                Resources.emplace(path,
                                  std::move(std::make_unique<AShaderResource>(
                                      LoadShader(vertexShaderPath.c_str(),
                                                 fragmentShaderPath.c_str()))));
            });
    }
    else
    {
        Resources.emplace(
            path,
            std::move(std::make_unique<AShaderResource>(LoadShader(
                vertexShaderPath.c_str(), fragmentShaderPath.c_str()))));
    }

    AResourceHandle ret(this, path);
    return ret;
}

void AResourceHolder::LoadGuiStyle(std::string path)
{
    if (World->IsMainThread())
    {
        World->QueueRenderThreadCall(
            [this, path]() {
                GuiLoadStyle(
                    (Helpers::GetProjectDirectory().string() + path).c_str());
            });
    }
    else
    {
        GuiLoadStyle((Helpers::GetProjectDirectory().string() + path).c_str());
    }
}

void* AResourceHolder::GetResourcePtr(std::string path)
{
    if (Resources.contains(path))
    {
        return Resources.at(path).get();
    }

    return nullptr;
}

void* AObjPtrHelper::GetPtr(AName typeName, size_t uid, AWorld* world)
{
    return world->GetObjectsByName(typeName)[uid].get();
}

} // namespace Atlantis
