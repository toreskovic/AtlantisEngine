#include "core.h"
#include "system.h"
#include <vector>
#include <iostream>

#define SERIALIZE_PROP_HELPER(type)                            \
    if (propData.Type == #type)                                \
    {                                                          \
        type prop = GetProperty<type>(propData.Name);          \
        type defProp = cdo->GetProperty<type>(propData.Name);  \
        propJson["Value"] = prop;                              \
        propJson["IsDefault"] = Helper_IsEqual(prop, defProp); \
    }

#define DESERIALIZE_PROP_HELPER(type)                                            \
    if (prop["Type"].get<std::string>() == #type)                                \
    {                                                                            \
        SetProperty(prop["Name"].get<std::string>(), prop["Value"].get<type>()); \
    }

namespace Atlantis
{
    std::map<HName, std::unique_ptr<AObject>, HNameComparer> AWorld::CDOs{};

    template <typename T>
    bool Helper_IsEqual(const T &l, const T &r)
    {
        return l == r;
    }

    template <>
    bool Helper_IsEqual(const Color &l, const Color &r)
    {
        return l.r == r.r && l.g == r.g && l.b == r.b && l.a == r.a;
    }

    template <>
    bool Helper_IsEqual(const Texture2D &l, const Texture2D &r)
    {
        return l.id == r.id;
    }

    nlohmann::json AObject::Serialize()
    {
        nlohmann::json json;
        const auto &classData = GetClassData();

        json["Name"] = classData.Name.GetName();
        json["Properties"] = nlohmann::json::array({});
        for (const auto &propData : classData.Properties)
        {
            nlohmann::json propJson = {{"Name", propData.Name.GetName()}, {"Type", propData.Type.GetName()}, {"Offset", propData.Offset}};
            const AObject *cdo = AWorld::GetCDO<AObject>(classData.Name);

            SERIALIZE_PROP_HELPER(float);
            SERIALIZE_PROP_HELPER(Color);
            SERIALIZE_PROP_HELPER(Texture2D);
            SERIALIZE_PROP_HELPER(std::string);
            SERIALIZE_PROP_HELPER(Atlantis::HName);
            SERIALIZE_PROP_HELPER(Atlantis::AResourceHandle);

            json["Properties"].push_back(propJson);
        }

        return json;
    }

    void AObject::Deserialize(const nlohmann::json &json)
    {
        for (auto &prop : json["Properties"])
        {
            if (prop["IsDefault"])
            {
                continue;
            }

            DESERIALIZE_PROP_HELPER(float);
            DESERIALIZE_PROP_HELPER(Color);
            DESERIALIZE_PROP_HELPER(Texture2D);
            DESERIALIZE_PROP_HELPER(std::string);
            DESERIALIZE_PROP_HELPER(Atlantis::HName);
            DESERIALIZE_PROP_HELPER(Atlantis::AResourceHandle);
        }
    }

    void AEntity::AddComponent(AComponent *component)
    {
        component->Owner = this;
        Components.push_back(component);

        ComponentNames.push_back(component->GetClassData().Name);
        std::sort(ComponentNames.begin(), ComponentNames.end());

        __componentMask = World->GetComponentMaskForComponents(ComponentNames);
    }

    bool AEntity::HasComponentOfType(const HName &name)
    {
        for (const HName &compName : ComponentNames)
        {
            if (compName == name)
            {
                return true;
            }
        }

        return false;
    }

    bool AEntity::HasComponentsByMask(ComponentBitset mask)
    {
        return (mask & __componentMask) == mask;
    }

    bool AEntity::HasComponentsOfType(const std::vector<HName> &names)
    {
        return HasComponentsByMask(World->GetComponentMaskForComponents(names));
    }

    void AWorld::RegisterSystem(ASystem *system, const std::vector<HName> &beforeLabels)
    {
        std::unique_ptr<ASystem> systemPtr(system);

        if (beforeLabels.size() > 0)
        {
            for (int i = 0; i < Systems.size(); i++)
            {
                const ASystem *sys = Systems[i].get();

                for (const HName &label : beforeLabels)
                {
                    if (sys->Labels.count(label))
                    {
                        Systems.insert(Systems.begin() + i, std::move(systemPtr));
                        return;
                    }
                }
            }
        }

        Systems.push_back(std::move(systemPtr));
    }

    void AWorld::RegisterSystem(std::function<void(AWorld *)> lambda, const std::vector<HName> &labels, const std::vector<HName> &beforeLabels)
    {
        ALambdaSystem *system = new ALambdaSystem();
        system->Lambda = lambda;
        system->Labels = {labels.begin(), labels.end()};

        RegisterSystem(system, beforeLabels);
    }

    void AWorld::ProcessSystems()
    {
        for (std::unique_ptr<ASystem> &system : Systems)
        {
            system->Process(this);
        }
    }

    const std::vector<std::unique_ptr<AObject, free_deleter>> &AWorld::GetObjectsByName(const HName &componentName)
    {
        const std::vector<std::unique_ptr<AObject, free_deleter>> &objList = ObjectLists[componentName];

        return objList;
    }

    size_t AWorld::GetObjectCountByType(const HName &objectName)
    {
        return GetObjectsByName(objectName).size();
    }

    const std::vector<AEntity *> AWorld::GetEntitiesWithComponents(std::vector<HName> componentNames)
    {
        std::vector<AEntity *> intersection;
        const auto &entities = GetObjectsByName("AEntity");
        intersection.reserve(entities.size());

        // std::sort(componentNames.begin(), componentNames.end());
        ComponentBitset componentMask = GetComponentMaskForComponents(componentNames);

        // TODO: iterate only over entities for the componentName with the smallest amount of instances
        for (auto &entityObj : entities)
        {
            AEntity *entity = static_cast<AEntity *>(entityObj.get());

            bool isValid = entity->__isAlive && entity->HasComponentsByMask(componentMask);

            if (isValid)
            {
                intersection.push_back(entity);
            }
        }

        return intersection;
    }

    ComponentBitset AWorld::GetComponentMaskForComponents(std::vector<HName> componentsNames)
    {
        ComponentBitset ret = 0x0;

        for (uint i = 0; i < componentsNames.size(); i++)
        {
            auto it = std::find(ComponentNames.begin(), ComponentNames.end(), componentsNames[i]);
            if (it != ComponentNames.end())
            {
                ret.set(i, true);
            }
        }

        return ret;
    }

    void AWorld::Clear()
    {
        CData.clear();
        CDOs.clear();
        ObjectLists.clear();
        Systems.clear();

        for (auto thing : ObjAllocStuff)
        {
            free((void *)thing.second.Start);
        }
    }
} // namespace Atlantis
