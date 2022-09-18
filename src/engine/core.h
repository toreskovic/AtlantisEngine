#ifndef CORE_H
#define CORE_H

#include <vector>
#include <functional>
#include <map>
#include <memory>

#include "reflection/reflectionHelpers.h"
#include "./generated/core.gen.h"

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
};

struct AEntity;

struct AComponent : public AObject
{
    AEntity* Owner;
};

struct AEntity : public AObject
{
    DEF_CLASS();

    std::vector<AComponent*> Components;

    AComponent* GetComponentOfType(const HName& name)
    {
        for (auto* comp : Components)
        {
            if (HName(comp->GetClassData().Name) == name)
            {
                return comp;
            }
        }

        return nullptr;
    }

    template <typename T>
    T* GetComponentOfType()
    {
        T tempComponent;
        HName name = tempComponent.GetClassData().Name;

        for (auto* comp : Components)
        {
            if (HName(comp->GetClassData().Name) == name)
            {
                return static_cast<T*>(comp);
            }
        }

        return nullptr;
    }

    void AddComponent(AComponent* component)
    {
        component->Owner = this;
        Components.push_back(component);
    }

    AEntity(){};

    AEntity(const AEntity& other)
    {

    }
};

struct Registry
{
    std::map<HName, ClassData, HNameComparer> CData;

    std::map<HName, std::unique_ptr<AObject>, HNameComparer> CDOs;
    std::map<HName, std::vector<std::shared_ptr<AObject>>, HNameComparer> ObjectLists;

    void RegisterClass(AObject *obj)
    {
        ClassData data = obj->GetClassData();
        CData.emplace(data.Name, data);

        CDOs.emplace(data.Name, std::make_unique<AObject>(*obj));
    }

    template <typename T>
    void RegisterDefault()
    {
        T thing;
        RegisterClass(&thing);
    }

    template <typename T>
    const T *GetCDO(const HName &name)
    {
        return dynamic_cast<T *>(CDOs[name].get());
    }

    template <typename T>
    T& NewObject(const HName &name)
    {
        T *CDO = dynamic_cast<T *>(CDOs[name].get());

        T thing = *CDO;

        ObjectLists[name].push_back(std::make_shared<T>(thing));

        auto& vec = ObjectLists[name];

        return dynamic_cast<T&>(*vec[vec.size() - 1].get());
    }

    const std::vector<std::shared_ptr<AObject>>& GetAllComponentsByName(const HName& componentName)
    {
        const std::vector<std::shared_ptr<AObject>>& objList = ObjectLists[componentName];

        return objList;
    }
};

#endif