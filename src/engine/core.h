#ifndef ACORE_H
#define ACORE_H

#include <vector>
#include <functional>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include "nlohmann/json.hpp"
#include <algorithm>
#include <string>

#include "reflection/reflectionHelpers.h"
#include "baseTypeSerialization.h"
#include "./generated/core.gen.h"

namespace Atlantis
{
    struct System;

    struct HNameComparer
    {
        bool operator()(const HName &lhs, const HName &rhs) const
        {
            return lhs < rhs;
        }
    };

    struct AObject
    {
        virtual const ClassData &GetClassData() const
        {
            static ClassData __classData;
            return __classData;
        };

        template <typename T>
        T GetProperty(const HName &name) const
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

        virtual nlohmann::json Serialize();

        virtual void Deserialize(const nlohmann::json &json);
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
        std::vector<HName> ComponentNames;

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
                if (comp->GetClassData().Name == name)
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

            ComponentNames.push_back(component->GetClassData().Name);
            std::sort(ComponentNames.begin(), ComponentNames.end());
        }

        bool HasComponentOfType(const HName &name)
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

        bool HasComponentsOfType(const std::vector<HName> &names)
        {
            for (int i = 0; i < names.size(); i++)
            {
                if (std::find(ComponentNames.begin(), ComponentNames.end(), names[i]) == ComponentNames.end())
                {
                    return false;
                }
            }

            return true;
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

        virtual void Deserialize(const nlohmann::json &json) override
        {
            for (auto &comp : json["Components"])
            {
                AComponent *component = GetComponentOfType(comp["Name"].get<std::string>());
                component->Deserialize(comp);
            }
        }
    };

    struct AResourceHolder
    {
        std::unordered_map<std::string, std::unique_ptr<AResource>> Resources;

        AResourceHandle GetTexture(std::string path)
        {
            if (Resources.contains(path))
            {
                AResourceHandle ret(Resources.at(path).get());
                return ret;
            }

            Resources.emplace(path, std::move(std::make_unique<ATextureResource>(LoadTexture(path.c_str()))));

            AResourceHandle ret(Resources.at(path).get());
            return ret;
        }
    };

    struct free_deleter
    {
        void operator()(void *const ptr) const
        {
            //free(ptr);
        }
    };

    struct Registry
    {
        std::map<HName, ClassData, HNameComparer> CData;

        static std::map<HName, std::unique_ptr<AObject>, HNameComparer> CDOs;
        std::map<HName, std::vector<std::unique_ptr<AObject, free_deleter>>, HNameComparer> ObjectLists;
        std::vector<std::unique_ptr<System>> Systems;
        AResourceHolder ResourceHolder;

        std::map<HName, size_t, HNameComparer> ObjAllocStuff;
        std::vector<size_t> ObjAllocStart;

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

            size_t memBlock = (size_t)malloc(100000 * data.Size);
            ObjAllocStuff.emplace(data.Name, memBlock);
            ObjAllocStart.push_back(memBlock);
        }

        template <typename T>
        static const T *GetCDO(const HName &name)
        {
            return dynamic_cast<T *>(CDOs[name].get());
        }

        template <typename T>
        T *NewObject(const HName &name)
        {
            const T *CDO = GetCDO<T>(name);

            ClassData classData = CDO->GetClassData();

            void *cpy = (void *)ObjAllocStuff.at(classData.Name);
            ObjAllocStuff.insert_or_assign(classData.Name, (size_t)cpy + classData.Size);

            // void *cpy = malloc(classData.Size);
            memcpy(cpy, (void *)CDO, classData.Size);

            T *cpy_T = static_cast<T *>(cpy);

            std::unique_ptr<AObject, free_deleter> sPtr(cpy_T);
            ObjectLists[name].push_back(std::move(sPtr));

            auto &vec = ObjectLists[name];

            return static_cast<T *>(vec[vec.size() - 1].get());
        }

        ~Registry()
        {
            for (size_t thing : ObjAllocStart)
            {
                free((void *)thing);
            }
        }

        void RegisterSystem(System *system, const std::vector<HName> &beforeLabels = {});

        void RegisterSystem(std::function<void(Registry *)> lambda, const std::vector<HName> &labels = {}, const std::vector<HName> &beforeLabels = {});

        void ProcessSystems();

        const std::vector<std::unique_ptr<AObject, free_deleter>> &GetObjectsByName(const HName &componentName);

        const std::unordered_set<AEntity *> GetEntitiesWithComponents(std::vector<HName> componentsNames);

        size_t GetObjectCountByType(const HName &objectName);

        void Clear();
    };
}

#endif
