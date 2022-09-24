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
    std::map<HName, std::unique_ptr<AObject>, HNameComparer> Registry::CDOs{};

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

    nlohmann::json AObject::Serialize()
    {
        nlohmann::json json;
        const auto &classData = GetClassData();

        json["Name"] = classData.Name.Name;
        json["Properties"] = nlohmann::json::array({});
        for (const auto &propData : classData.Properties)
        {
            nlohmann::json propJson = {{"Name", propData.Name.Name}, {"Type", propData.Type.Name}, {"Offset", propData.Offset}};
            const AObject *cdo = Registry::GetCDO<AObject>(classData.Name);

            SERIALIZE_PROP_HELPER(float);
            SERIALIZE_PROP_HELPER(Color);
            SERIALIZE_PROP_HELPER(std::string);
            SERIALIZE_PROP_HELPER(Atlantis::HName);

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
            DESERIALIZE_PROP_HELPER(std::string);
            DESERIALIZE_PROP_HELPER(Atlantis::HName);
        }
    }

    void Registry::RegisterSystem(System *system, const std::vector<HName> &beforeLabels)
    {
        std::unique_ptr<System> systemPtr(system);

        if (beforeLabels.size() > 0)
        {
            for (int i = 0; i < Systems.size(); i++)
            {
                const System *sys = Systems[i].get();

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

    void Registry::RegisterSystem(std::function<void(Registry *)> lambda, const std::vector<HName> &labels, const std::vector<HName> &beforeLabels)
    {
        LambdaSystem* system = new LambdaSystem();
        system->Lambda = lambda;
        system->Labels = {labels.begin(), labels.end()};

        RegisterSystem(system, beforeLabels);
    }

    void Registry::ProcessSystems()
    {
        for (std::unique_ptr<System> &system : Systems)
        {
            system->Process(this);
        }
    }

    const std::vector<std::unique_ptr<AObject>> &Registry::GetComponentsByName(const HName &componentName)
    {
        const std::vector<std::unique_ptr<AObject>> &objList = ObjectLists[componentName];

        return objList;
    }

    const std::unordered_set<AEntity *> Registry::GetEntitiesWithComponents(const std::vector<HName> &componentNames)
    {
        std::vector<std::unordered_set<AEntity *>> entitySets;
        entitySets.reserve(componentNames.size());

        const HName entityName("AEntity");
        auto entityCount = ObjectLists[entityName].size();

        for (const auto &name : componentNames)
        {
            std::unordered_set<AEntity *> compEntities;
            compEntities.reserve(entityCount);
            const std::vector<std::unique_ptr<AObject>> &components = GetComponentsByName(name);

            for (const auto &obj : components)
            {
                AComponent *component = static_cast<AComponent *>(obj.get());

                compEntities.insert(component->Owner);
            }

            entitySets.push_back(compEntities);
        }

        std::unordered_set<AEntity *> tmpSet = entitySets[0];
        std::unordered_set<AEntity *> intersection = tmpSet;
        intersection.reserve(entityCount);
        for (int i = 1; i < entitySets.size(); i++)
        {
            intersection.clear();
            intersection.reserve(entityCount);

            // TODO: iterate over intersection instead of the larger set
            for (AEntity *e : entitySets[i])
            {
                if (tmpSet.count(e) > 0)
                {
                    intersection.insert(e);
                }
            }

            tmpSet = intersection;
        }

        return intersection;
    }

    void Registry::Clear()
    {
        CData.clear();
        CDOs.clear();
        ObjectLists.clear();
    }
} // namespace Atlantis
