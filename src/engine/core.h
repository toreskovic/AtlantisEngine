#ifndef ACORE_H
#define ACORE_H

#include <vector>
#include <functional>
#include <map>
#include <memory>
#include "nlohmann/json.hpp"

#include "reflection/reflectionHelpers.h"
#include "baseTypeSerialization.h"
#include "./generated/core.gen.h"

namespace Atlantis
{
    struct HNameComparer
    {
        constexpr bool operator()(const HName &lhs, const HName &rhs) const
        {
            return lhs < rhs;
        }
    };

    struct AObject
    {
        virtual const ClassData &GetClassData()
        {
            static ClassData __classData;
            return __classData;
        };

        template <typename T>
        T GetProperty(const HName &name)
        {
            const ClassData &classData = GetClassData();
            for (auto &prop : classData.Properties)
            {
                if (prop.Name == name)
                {
                    T *val = reinterpret_cast<T *>((size_t)this + prop.Offset);
                    return *val;
                }
            }

            T tmp;
            return tmp;
        }

        template <typename T>
        void SetProperty(const HName &name, T value)
        {
            const ClassData &classData = GetClassData();
            for (auto &prop : classData.Properties)
            {
                if (prop.Name == name)
                {
                    T *val = reinterpret_cast<T *>((size_t)this + prop.Offset);
                    *val = value;
                    break;
                }
            }
        }

        virtual nlohmann::json Serialize()
        {
            nlohmann::json json;
            const auto &classData = GetClassData();

            json["Name"] = classData.Name.Name;
            json["Properties"] = nlohmann::json::array({});
            for (const auto &propData : classData.Properties)
            {
                if (propData.Type == "float")
                {
                    json["Properties"].push_back({{"Name", propData.Name.Name}, {"Type", propData.Type.Name}, {"Offset", propData.Offset}, {"Value", GetProperty<float>(propData.Name)}});
                }
                else if (propData.Type == "Color")
                {
                    json["Properties"].push_back({{"Name", propData.Name.Name}, {"Type", propData.Type.Name}, {"Offset", propData.Offset}, {"Value", GetProperty<Color>(propData.Name)}});
                }
                else
                {
                    json["Properties"].push_back({{"Name", propData.Name.Name}, {"Type", propData.Type.Name}, {"Offset", propData.Offset}});
                }
            }

            return json;
        }

        virtual void Deserialize(nlohmann::json &json)
        {
            const ClassData &classData = GetClassData();
            for (auto &prop : json["Properties"])
            {
                if (prop["Type"].get<std::string>() == "float")
                {
                    SetProperty(prop["Name"].get<std::string>(), prop["Value"].get<float>());
                }
                if (prop["Type"].get<std::string>() == "Color")
                {
                    SetProperty(prop["Name"].get<std::string>(), prop["Value"].get<Color>());
                }
            }
        }
    };

    struct AEntity;

    struct AComponent : public AObject
    {
        AEntity *Owner;
    };

    struct AEntity : public AObject
    {
        DEF_CLASS();

        std::vector<AComponent *> Components;

        AComponent *GetComponentOfType(const HName &name)
        {
            for (auto *comp : Components)
            {
                if (HName(comp->GetClassData().Name) == name)
                {
                    return comp;
                }
            }

            return nullptr;
        }

        template <typename T>
        T *GetComponentOfType()
        {
            T tempComponent;
            HName name = tempComponent.GetClassData().Name;

            for (auto *comp : Components)
            {
                if (HName(comp->GetClassData().Name) == name)
                {
                    return static_cast<T *>(comp);
                }
            }

            return nullptr;
        }

        void AddComponent(AComponent *component)
        {
            component->Owner = this;
            Components.push_back(component);
        }

        AEntity(){};

        AEntity(const AEntity &other)
        {
        }

        virtual nlohmann::json Serialize() override
        {
            nlohmann::json json = AObject::Serialize();
            json["Components"] = nlohmann::json::array({});

            for (AComponent *comp : Components)
            {
                nlohmann::json j = comp->Serialize();
                json["Components"].push_back(j);
            }

            return json;
        }

        virtual void Deserialize(nlohmann::json &json)
        {
            for (auto &comp : json["Components"])
            {
                AComponent *component = GetComponentOfType(comp["Name"].get<std::string>());
                component->Deserialize(comp);
            }
        }
    };

    struct Registry
    {
        std::map<HName, ClassData, HNameComparer> CData;

        std::map<HName, std::unique_ptr<AObject>, HNameComparer> CDOs;
        std::map<HName, std::vector<std::unique_ptr<AObject>>, HNameComparer> ObjectLists;

        /*void RegisterClass(AObject *obj)
        {
            ClassData data = obj->GetClassData();
            CData.emplace(data.Name, data);

            CDOs.emplace(data.Name, std::make_shared<AObject>(*obj));
        }*/

        template <typename T>
        void RegisterDefault()
        {
            T obj;
            ClassData data = obj.GetClassData();
            CData.emplace(data.Name, data);

            CDOs.emplace(data.Name, std::make_unique<T>(obj));
        }

        template <typename T>
        const T *GetCDO(const HName &name)
        {
            return dynamic_cast<T *>(CDOs[name].get());
        }

        template <typename T>
        T *NewObject(const HName &name)
        {
            T *CDO = dynamic_cast<T *>(CDOs[name].get());

            ClassData classData = CDO->GetClassData();
            T *cpy = (T *)malloc(classData.Size);
            memcpy(cpy, CDO, classData.Size);

            std::unique_ptr<AObject> sPtr(cpy);
            ObjectLists[name].push_back(std::move(sPtr));

            /*T thing = *CDO;

            ObjectLists[name].push_back(std::make_shared<T>(thing));*/

            auto &vec = ObjectLists[name];

            return static_cast<T *>(vec[vec.size() - 1].get());
        }

        const std::vector<std::unique_ptr<AObject>> &GetAllComponentsByName(const HName &componentName)
        {
            const std::vector<std::unique_ptr<AObject>> &objList = ObjectLists[componentName];

            return objList;
        }

        void Clear()
        {
            CData.clear();
            CDOs.clear();
            ObjectLists.clear();
        }
    };
}

#endif