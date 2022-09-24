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

        json["Name"] = classData.Name.GetName();
        json["Properties"] = nlohmann::json::array({});
        for (const auto &propData : classData.Properties)
        {
            nlohmann::json propJson = {{"Name", propData.Name.GetName()}, {"Type", propData.Type.GetName()}, {"Offset", propData.Offset}};
            const AObject *cdo = Registry::GetCDO<AObject>(classData.Name);

            SERIALIZE_PROP_HELPER(float);
            SERIALIZE_PROP_HELPER(Color);
            SERIALIZE_PROP_HELPER(std::string);
            SERIALIZE_PROP_HELPER(Atlantis::HName);
            SERIALIZE_PROP_HELPER(Atlantis::ResourceHandle);

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
            DESERIALIZE_PROP_HELPER(Atlantis::ResourceHandle);
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

    const std::vector<std::unique_ptr<AObject>> &Registry::GetObjectsByName(const HName &componentName)
    {
        const std::vector<std::unique_ptr<AObject>> &objList = ObjectLists[componentName];

        return objList;
    }

    size_t Registry::GetObjectCountByType(const HName& objectName)
    {
        return GetObjectsByName(objectName).size();
    }

    const std::unordered_set<AEntity *> Registry::GetEntitiesWithComponents(std::vector<HName> componentNames)
    {
        std::unordered_set<AEntity *> intersection;
        const auto& entities = GetObjectsByName("AEntity");
        intersection.reserve(entities.size());

        std::sort(componentNames.begin(), componentNames.end());

        // TODO: iterate only over entities for the componentName with the smallest amount of instances
        for (auto& entityObj : entities)
        {
            AEntity* entity = static_cast<AEntity *>(entityObj.get());
            bool isValid = true;

            isValid = entity->HasComponentsOfType(componentNames);

            if (isValid)
            {
                intersection.insert(entity);
            }
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
