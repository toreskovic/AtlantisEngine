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
#include <type_traits>
#include <bitset>

#include <iostream>

#include "reflection/reflectionHelpers.h"
#include "baseTypeSerialization.h"
#include "./generated/core.gen.h"

// TODO: arbitrary number, make it configurable and / or larger by default
typedef std::bitset<200> ComponentBitset;

namespace Atlantis
{
    struct ASystem;

    struct HNameComparer
    {
        bool operator()(const HName &lhs, const HName &rhs) const
        {
            return lhs < rhs;
        }
    };

    struct AWorld;

    struct AObject
    {
        AWorld *World = nullptr;

        // used internally
        // object's index in the objects' array
        // used for keeping references after reallocating memory
        size_t _uid;

        // used internally to know if we need to process the entity / component
        // or ignore it (and potentially reuse it when creating new entities / components)
        bool _isAlive = true;

        virtual void MarkObjectDead();

        virtual const AClassData &GetClassData() const
        {
            static AClassData classData;
            return classData;
        };

        static AClassData &GetClassDataStatic()
        {
            static AClassData classData;
            return classData;
        }

        template <typename T>
        T GetProperty(const HName &name) const
        {
            const AClassData &classData = GetClassData();
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
            const AClassData &classData = GetClassData();
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

        virtual void OnAddedToEntity(AEntity *entity)
        {
            Owner = entity;
        }

        virtual void OnRemovedFromEntity(AEntity *entity)
        {
            Owner = nullptr;
            MarkObjectDead();
        }

        virtual void MarkObjectDead() override;

    };

    struct AEntity : public AObject
    {
        DEF_CLASS();

        std::vector<AComponent *> Components;
        std::vector<HName> ComponentNames;

        // used internally to check quickly for components
        ComponentBitset _componentMask = 0x0;

        virtual void MarkObjectDead() override;

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

        void AddComponent(AComponent *component);

        void RemoveComponent(AComponent *component);

        bool HasComponentOfType(const HName &name);

        bool HasComponentsByMask(ComponentBitset mask);

        bool HasComponentsOfType(const std::vector<HName> &names);
        //{
        // return World->GetComponentMaskForComponents(names) == _componentMask;
        /*for (int i = 0; i < names.size(); i++)
        {
            if (std::find(ComponentNames.begin(), ComponentNames.end(), names[i]) == ComponentNames.end())
            {
                return false;
            }
        }

        return true;*/
        //}

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

    template <typename T>
    struct AObjPtr
    {
        AObjPtr(T* ptr)
        {
            if (ptr == nullptr)
            {
                Clear();
            }
            else
            {
                _isAssigned = true;
                _world = ptr->World;
                _uid = ptr->_uid;
            }
        }

        bool IsValid();

        T *Get(bool validate = true);

        T *Get(const HName& name, bool validate = true);

        T *operator->()
        {
            return Get();
        }

        void Clear()
        {
            _isAssigned = false;
            _world = nullptr;
        }

        // object's ID (currently index)
        size_t _uid;

        // we can only check for ptr validity once assigned
        bool _isAssigned = false;

        // cache world from object after assigning
        AWorld *_world = nullptr;
    };

    struct no_deleter
    {
        void operator()(void *const ptr) const
        {
        }
    };

    struct AWorld
    {
        std::map<HName, AClassData, HNameComparer> CData;

        static std::map<HName, std::unique_ptr<AObject>, HNameComparer> CDOs;
        std::map<HName, std::vector<std::unique_ptr<AObject, no_deleter>>, HNameComparer> ObjectLists;
        std::map<HName, std::vector<AObjPtr<AObject>>> DeadObjects;
        std::vector<std::unique_ptr<ASystem>> Systems;
        AResourceHolder ResourceHolder;

        std::vector<HName> ComponentNames;

        struct AllocatorMemoryHelper
        {
            size_t Start;
            size_t Count;
            size_t Limit;
            size_t Increment;
        };

        std::map<HName, AllocatorMemoryHelper, HNameComparer> AllocatorHelpers;
        // std::map<HName, size_t, HNameComparer> ObjAllocStart;

        /*void RegisterClass(AObject *obj)
        {
            AClassData data = obj->GetClassData();
            CData.emplace(data.Name, data);

            CDOs.emplace(data.Name, std::make_shared<AObject>(*obj));
        }*/

        template <typename T, size_t Amount, size_t Increment = Amount>
        void RegisterDefault()
        {
            T obj;
            AClassData data = obj.GetClassData();
            CData.insert_or_assign(data.Name, data);

            CDOs.insert_or_assign(data.Name, std::make_unique<T>(obj));

            size_t memBlock = (size_t)malloc(Amount * data.Size);

            AllocatorMemoryHelper allocatorHelper;
            allocatorHelper.Start = memBlock;
            allocatorHelper.Count = 0;
            allocatorHelper.Limit = Amount;
            allocatorHelper.Increment = Increment;

            AllocatorHelpers.insert_or_assign(data.Name, allocatorHelper);
            // ObjAllocStart.emplace(data.Name, memBlock);

            T *objPtr = &obj;
            if (dynamic_cast<AComponent *>(objPtr) != nullptr)
            {
                ComponentNames.push_back(data.Name);
            }

            ObjectLists[data.Name].reserve(allocatorHelper.Limit);
            DeadObjects[data.Name].reserve(allocatorHelper.Limit);
        }

        template <typename T>
        void RegisterDefault()
        {
            RegisterDefault<T, 10000>();
        }

        template <typename T>
        static const T *GetCDO(const HName &name)
        {
            return dynamic_cast<T *>(CDOs[name].get());
        }

        template <typename T>
        T *NewObject_Base(const HName &name)
        {
            const T *CDO = GetCDO<T>(name);

            AClassData classData = CDO->GetClassData();

            // reuse dead objects
            if (DeadObjects[name].size() > 0)
            {
                AObjPtr<AObject> objPtr = DeadObjects[name].back();
                DeadObjects[name].pop_back();

                T *obj = static_cast<T *>(objPtr.Get(name, false));
                obj->_isAlive = true;

                return obj;
            }

            // allocate new objects
            AllocatorMemoryHelper &allocatorHelper = AllocatorHelpers.at(classData.Name);
            if (allocatorHelper.Count >= allocatorHelper.Limit)
            {
                std::cout << "Reallocating memory for type " << classData.Name.GetName() << " from " << allocatorHelper.Limit << " to " << allocatorHelper.Limit + allocatorHelper.Increment << std::endl;

                allocatorHelper.Limit += allocatorHelper.Increment;
                allocatorHelper.Start = (size_t)realloc((void *)allocatorHelper.Start, allocatorHelper.Limit * classData.Size);

                ObjectLists[name].clear();
                ObjectLists[name].reserve(allocatorHelper.Limit);
                DeadObjects[name].reserve(allocatorHelper.Limit);

                for (size_t i = 0; i < allocatorHelper.Count; i++)
                {
                    T *objPtr = static_cast<T *>((void *)(allocatorHelper.Start + i * classData.Size));
                    std::unique_ptr<AObject, no_deleter> sPtr(objPtr);
                    ObjectLists[name].push_back(std::move(sPtr));

                    // TODO: Ugly
                    if (AEntity *entity = dynamic_cast<AEntity *>(objPtr))
                    {
                        for (auto *component : entity->Components)
                        {
                            component->Owner = entity;
                        }
                    }
                    else if (AComponent *component = dynamic_cast<AComponent *>(objPtr))
                    {
                        if (component->Owner != nullptr)
                        {
                            for (int j = 0; j < component->Owner->Components.size(); j++)
                            {
                                if (component->Owner->ComponentNames[j] == classData.Name)
                                {
                                    component->Owner->Components[j] = component;
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            void *cpy = (void *)(allocatorHelper.Start + allocatorHelper.Count * classData.Size);

            // void *cpy = malloc(classData.Size);
            memcpy(cpy, (void *)CDO, classData.Size);

            T *cpy_T = static_cast<T *>(cpy);

            cpy_T->_uid = allocatorHelper.Count;
            cpy_T->World = this;

            allocatorHelper.Count++;

            std::unique_ptr<AObject, no_deleter> sPtr(cpy_T);
            ObjectLists[name].push_back(std::move(sPtr));

            auto &vec = ObjectLists[name];

            return static_cast<T *>(vec[vec.size() - 1].get());
        }

        template <typename T>
        T *NewObject(const HName &name)
        {
            T *obj = NewObject_Base<T>(name);

            return obj;
        }

        /*template <typename T,
                  std::enable_if_t<!std::is_base_of_v<AEntity, T>> * = nullptr>
        T *NewObject(const HName &name)
        {
            return NewObject_Base<T>(name);
        }

        template <typename T,
                  std::enable_if_t<std::is_base_of_v<AEntity, T>> * = nullptr>
        T *NewObject(const HName &name)
        {
            T *obj = NewObject_Base<T>(name);
            obj->World = this;

            return obj;
        }*/

        template <typename T>
        T *NewObject()
        {
            return NewObject<T>(T::GetClassDataStatic().Name);
        }

        void MarkObjectDead(AObject *object);

        ~AWorld()
        {
            for (auto thing : AllocatorHelpers)
            {
                free((void *)thing.second.Start);
            }
        }

        void RegisterSystem(ASystem *system, const std::vector<HName> &beforeLabels = {});

        void RegisterSystem(std::function<void(AWorld *)> lambda, const std::vector<HName> &labels = {}, const std::vector<HName> &beforeLabels = {});

        void ProcessSystems();

        const std::vector<std::unique_ptr<AObject, no_deleter>> &GetObjectsByName(const HName &objectName);

        const std::vector<AEntity *> GetEntitiesWithComponents(std::vector<HName> componentsNames);

        ComponentBitset GetComponentMaskForComponents(std::vector<HName> componentsNames);

        size_t GetObjectCountByType(const HName &objectName);

        void Clear();

        template <typename T>
        void GetNamesOfComponents(std::vector<HName> &names)
        {
            static T tmp;
            static HName tmpName = tmp.GetClassData().Name;
            names.push_back(tmpName);
        }

        template <typename T1, typename T2, typename... Types>
        void GetNamesOfComponents(std::vector<HName> &names)
        {
            static HName tmpName = T1::GetClassDataStatic().Name;
            names.push_back(tmpName);

            GetNamesOfComponents<T2, Types...>(names);
        }

        template <typename T, typename... Types>
        const std::vector<AEntity *> GetEntitiesWithComponents()
        {
            static std::vector<HName> names;
            if (names.size() == 0)
            {
                // TODO: make less arbitrary
                names.reserve(8);

                GetNamesOfComponents<T, Types...>(names);
            }

            return GetEntitiesWithComponents(names);
        }
    };

    template<typename T>
    inline bool AObjPtr<T>::IsValid()
    {
        if (!_isAssigned)
        {
            return false;
        }

        T *obj = static_cast<T *>(_world->GetObjectsByName(T::GetClassDataStatic().Name)[_uid].get());
        return obj->_isAlive;
    }

    template<typename T>
    inline T *AObjPtr<T>::Get(bool validate)
    {
        if (validate)
        {
            if (!IsValid())
            {
                return nullptr;
            }
        }

        return static_cast<T *>(_world->GetObjectsByName(T::GetClassDataStatic().Name)[_uid].get());
    }

    template <typename T>
    inline T *AObjPtr<T>::Get(const HName &name, bool validate)
    {
        if (validate)
        {
            if (!IsValid())
            {
                return nullptr;
            }
        }

        return static_cast<T *>(_world->GetObjectsByName(name)[_uid].get());
    }
}

#endif
