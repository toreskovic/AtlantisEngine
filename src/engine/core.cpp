#include "core.h"

#define SERIALIZE_PROP_HELPER(type) \
            if (propData.Type == #type) \
            { \
                type prop = GetProperty<type>(propData.Name); \
                type defProp = cdo->GetProperty<type>(propData.Name); \
                propJson["Value"] = prop; \
                propJson["IsDefault"] = Helper_IsEqual(prop, defProp); \
            }

#define DESERIALIZE_PROP_HELPER(type) \
            if (prop["Type"].get<std::string>() == #type) \
            { \
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

    const std::vector<std::unique_ptr<AObject>> &Registry::GetAllComponentsByName(const HName &componentName)
    {
        const std::vector<std::unique_ptr<AObject>> &objList = ObjectLists[componentName];

        return objList;
    }

    void Registry::Clear()
    {
        CData.clear();
        CDOs.clear();
        ObjectLists.clear();
    }
} // namespace Atlantis
