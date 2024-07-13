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
#include <mutex>
#include <thread>
#include <atomic>
#include <iostream>

#include <sol/sol.hpp>

#include "reflection/reflectionHelpers.h"
#include "baseTypeSerialization.h"
#include "helpers.h"
#include "engine/system.h"
#include "engine/inputHandler.h"
#include "engine/profiling.h"
#include "./generated/core.gen.h"

// TODO: arbitrary number, make it configurable and / or larger by default
constexpr std::size_t MAX_COMPONENTS = 128;
typedef std::bitset<MAX_COMPONENTS> ComponentBitset;

namespace Atlantis
{
    struct ASystem;

    struct ANameComparer
    {
        bool operator()(const AName &lhs, const AName &rhs) const
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
        T GetProperty(const AName &name) const
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
        void SetProperty(const AName &name, T value)
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
        AEntity *Owner = nullptr;

        bool _shouldBlockRenderThread = false;

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

        template <typename R, typename... Types>
        R GetPropertyScripting(Types... args)
        {
            std::cout << "Error: Unimplemented GetPropertyScripting" << std::endl;
        }

        template <typename... Types>
        void SetPropertyScripting(Types... args)
        {
            std::cout << "Error: Unimplemented SetPropertyScripting" << std::endl;
        }
    };

    struct AEntity : public AObject
    {
        DEF_CLASS();

        std::vector<AComponent *> Components;
        std::vector<AName> ComponentNames;

        // used internally to check quickly for components
        ComponentBitset _componentMask = 0x0;

        virtual void MarkObjectDead() override;

        uint32_t GetStaticComponentIndex(const AName &name) const;

        AComponent *GetComponentOfType(const AName &name) const;

        template <typename T>
        T *GetComponentOfType() const
        {
            static const AName &name = T::GetClassDataStatic().Name;
            static const uint32_t staticIndex = GetStaticComponentIndex(name);
            
            if (_componentMask[staticIndex] == 0)
            {
                return nullptr;
            }

            static const size_t shift = MAX_COMPONENTS - staticIndex;

            return static_cast<T*>(Components[(_componentMask << shift).count()]);
        }

        void AddComponent(AComponent *component);

        void RemoveComponent(AComponent *component);

        template <typename T>
        void RemoveComponentOfType()
        {
            RemoveComponent(GetComponentOfType<T>());
        }

        bool HasComponentOfType(const AName &name);

        bool HasComponentsByMask(const ComponentBitset &mask);

        bool HasComponentsOfType(const std::vector<AName> &names);

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
        AWorld *World = nullptr;
        std::unordered_map<std::string, std::unique_ptr<AResource>> Resources;

        AResourceHolder(AWorld *world)
        {
            World = world;
        }

        AResourceHandle GetTexture(std::string path);

        AResourceHandle GetShader(std::string path, int variant = 0);

        void LoadGuiStyle(std::string path);

        void* GetResourcePtr(std::string path);
    };

    class AObjPtrHelper
    {
    public:
        static void* GetPtr(AName typeName, size_t uid, AWorld* world);
    };

    template <typename T>
    struct AObjPtr
    {
        AObjPtr(T *ptr)
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
                _typeName = ptr->GetClassData().Name;
            }
        }

        AObjPtr(const AObjPtr<T> &other)
        {
            _uid = other._uid;
            _typeName = other._typeName;
            _isAssigned = other._isAssigned;
            _world = other._world;
        }

        bool IsValid() const;

        T *Get(bool validate = true) const;

        T *Get(const AName &name, bool validate = true) const;

        T *operator->() const
        {
            return Get();
        }

        bool operator ==(const AObjPtr<T> &other) const
        {
            return _uid == other._uid && _typeName == other._typeName;
        }

        void operator =(const AObjPtr<T> &other)
        {
            _uid = other._uid;
            _typeName = other._typeName;
            _isAssigned = other._isAssigned;
            _world = other._world;
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

        // cache object type name for validation
        AName _typeName;

        // cache world from object after assigning
        AWorld *_world = nullptr;
    };

    template <typename T, uint32_t size>
    struct SmallArray
    {
        T Data[size];
        uint32_t Count;

        std::vector<T> BigArray;

        SmallArray() { Count = 0; }

        void Add(T item)
        {
            if (Count < size)
            {
                Data[Count] = item;
                Count++;
            }
            else
            {
                BigArray.push_back(item);
            }
        }

        void Remove(T item)
        {
            for (uint32_t i = 0; i < Count; i++)
            {
                if (Data[i] == item)
                {
                    Data[i] = Data[Count - 1];
                    Count--;
                    return;
                }
            }

            for (uint32_t i = 0; i < BigArray.size(); i++)
            {
                if (BigArray[i] == item)
                {
                    BigArray[i] = BigArray[BigArray.size() - 1];
                    BigArray.pop_back();
                    return;
                }
            }
        }

        void Clear()
        {
            Count = 0;
            BigArray.clear();
        }

        T& operator[](uint32_t index)
        {
            if (index < Count)
            {
                return Data[index];
            }
            else
            {
                return BigArray[index - Count];
            }
        }

        const T& operator[](uint32_t index) const
        {
            if (index < Count)
            {
                return Data[index];
            }
            else
            {
                return BigArray[index - Count];
            }
        }

        uint32_t Size() const { return Count + BigArray.size(); }
    };

    template <typename T>
    inline bool AObjPtr<T>::IsValid() const
    {
        if (!_isAssigned)
        {
            return false;
        }

        if (_typeName == AName::None())
        {
            return false;
        }

        T *obj = static_cast<T *>(AObjPtrHelper::GetPtr(_typeName, _uid, _world));
        return obj->_isAlive;
    }

    template <typename T>
    inline T *AObjPtr<T>::Get(bool validate) const
    {
        if (validate)
        {
            if (!IsValid())
            {
                return nullptr;
            }
        }

        if (_typeName == AName::None())
        {
            return nullptr;
        }

        return static_cast<T *>(AObjPtrHelper::GetPtr(_typeName, _uid, _world));
    }

    template <typename T>
    inline T *AObjPtr<T>::Get(const AName &name, bool validate) const
    {
        if (validate)
        {
            if (!IsValid())
            {
                return nullptr;
            }
        }

        return static_cast<T *>(AObjPtrHelper::GetPtr(name, _uid, _world));
    }
}

#endif
