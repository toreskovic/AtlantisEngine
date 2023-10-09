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

        AComponent *GetComponentOfType(const AName &name) const
        {
            auto it = std::lower_bound(ComponentNames.begin(), ComponentNames.end(), name);
            if (it != ComponentNames.end() && *it == name)
            {
                int index = std::distance(ComponentNames.begin(), it);
                return Components[index];
            }

            return nullptr;
        }

        template <typename T>
        T *GetComponentOfType() const;

        void AddComponent(AComponent *component);

        void RemoveComponent(AComponent *component);

        bool HasComponentOfType(const AName &name);

        bool HasComponentsByMask(const ComponentBitset &mask);

        bool HasComponentsOfType(const std::vector<AName> &names);
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
        AWorld *World = nullptr;
        std::unordered_map<std::string, std::unique_ptr<AResource>> Resources;

        AResourceHolder(AWorld *world)
        {
            World = world;
        }

        AResourceHandle GetTexture(std::string path);

        void* GetResourcePtr(std::string path);
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

    struct no_deleter
    {
        void operator()(void *const ptr) const
        {
        }
    };

    struct AObjectCreateCommand
    {
        std::function<void()> Callback;
    };

    struct AWorld
    {
        std::map<AName, AClassData, ANameComparer> CData;

        std::map<AName, std::unique_ptr<AObject>, ANameComparer> CDOs;
        std::map<AName, std::vector<std::unique_ptr<AObject, no_deleter>>, ANameComparer> ObjectLists;
        std::map<AName, std::vector<AObjPtr<AObject>>> DeadObjects;
        std::vector<std::unique_ptr<ASystem>> Systems;
        // TEMP for testing
        std::vector<std::unique_ptr<ASystem>> SystemsRenderThread;
        std::mutex RenderThreadMutex;
        std::mutex ProfilingMutex;
        const std::thread::id MAIN_THREAD_ID = std::this_thread::get_id();
        std::vector<std::function<void()>> RenderThreadCallQueue;

        AResourceHolder ResourceHolder = AResourceHolder(this);

        std::vector<std::function<void()>> ObjectCreateCommandsQueue;
        std::vector<std::function<void()>> ObjectModifyQueue;
        std::vector<AObjPtr<AObject>> ObjectDestroyQueue;

        std::vector<AName> ComponentNames;

        std::atomic<bool> MainThreadProcessing = false;
        std::atomic<bool> RenderThreadProcessing = false;

        SSimpleProfiler* ProfilerMainThread;
        SSimpleProfiler* ProfilerRenderThread;

        struct AllocatorMemoryHelper
        {
            size_t Start;
            size_t Count;
            size_t Limit;
            size_t Increment;
        };

        std::map<AName, AllocatorMemoryHelper, ANameComparer> AllocatorHelpers;
        // std::map<AName, size_t, ANameComparer> ObjAllocStart;

        /*void RegisterClass(AObject *obj)
        {
            AClassData data = obj->GetClassData();
            CData.emplace(data.Name, data);

            CDOs.emplace(data.Name, std::make_shared<AObject>(*obj));
        }*/

        template <typename T, size_t Amount, size_t Increment = Amount>
        void RegisterDefault(AName name = AName::None())
        {
            T obj;
            AClassData data = obj.GetClassData();
            AName objName = name == AName::None() ? data.Name : name;

            CData.insert_or_assign(objName, data);

            CDOs.insert_or_assign(objName, std::make_unique<T>(obj));

            size_t memBlock = (size_t)malloc(Amount * data.Size);

            AllocatorMemoryHelper allocatorHelper;
            allocatorHelper.Start = memBlock;
            allocatorHelper.Count = 0;
            allocatorHelper.Limit = Amount;
            allocatorHelper.Increment = Increment;

            AllocatorHelpers.insert_or_assign(objName, allocatorHelper);
            // ObjAllocStart.emplace(data.Name, memBlock);

            T *objPtr = &obj;
            if (dynamic_cast<AComponent *>(objPtr) != nullptr)
            {
                // insert sorted
                auto it = std::lower_bound(
                    ComponentNames.begin(), ComponentNames.end(), objName);
                if (it == ComponentNames.end() || *it != objName)
                {
                    ComponentNames.insert(it, objName);
                }
            }

            ObjectLists[objName].reserve(allocatorHelper.Limit);
            DeadObjects[objName].reserve(allocatorHelper.Limit);
        }

        template <typename T>
        void RegisterDefault(AName name = AName::None())
        {
            RegisterDefault<T, 10000>(name);
        }

        template <typename T>
        const T *GetCDO(const AName &name)
        {
            return dynamic_cast<T *>(CDOs[name].get());
        }

        template <typename T>
        T *NewObject_Base(const AName &name)
        {
            _registryVersion++;
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
        T *NewObject_Internal(const AName &name)
        {
            T *obj = NewObject_Base<T>(name);

            return obj;
        }

        /*template <typename T,
                  std::enable_if_t<!std::is_base_of_v<AEntity, T>> * = nullptr>
        T *NewObject_Internal(const AName &name)
        {
            return NewObject_Base<T>(name);
        }

        template <typename T,
                  std::enable_if_t<std::is_base_of_v<AEntity, T>> * = nullptr>
        T *NewObject_Internal(const AName &name)
        {
            T *obj = NewObject_Base<T>(name);
            obj->World = this;

            return obj;
        }*/

        template <typename T>
        T *NewObject_Internal()
        {
            return NewObject_Internal<T>(T::GetClassDataStatic().Name);
        }

        template <typename T>
        void QueueNewObject(std::function<void(T *)> lambda)
        {
            ObjectCreateCommandsQueue.push_back([this, lambda]()
            {
                T* obj = NewObject_Internal<T>();
                lambda(obj);
            });
        }

        void QueueSystem(std::function<void()> lambda);

        void QueueModifyObject(AObjPtr<AObject> object, std::function<void(AObject *)> lambda);

        void MarkObjectDead(AObject *object);

        void QueueObjectDeletion(AObjPtr<AObject> object);

        float GetDeltaTime() const;

        bool IsMainThread() const;
        
        uint32_t GetRegistryVersion() const;

        ~AWorld()
        {
            for (auto thing : AllocatorHelpers)
            {
                free((void *)thing.second.Start);
            }
        }

        void RegisterSystem(ASystem *system, const std::vector<AName> &beforeLabels = {});

        void RegisterSystem(std::function<void(AWorld *)> lambda, const std::vector<AName> &labels = {}, const std::vector<AName> &beforeLabels = {}, bool renderThread = false);

        void RegisterSystemRenderThread(std::function<void(AWorld *)> lambda, const std::vector<AName> &labels = {}, const std::vector<AName> &beforeLabels = {});

        void RegisterSystemTimesliced(int objectsPerFrame, std::function<void(AWorld*, ASystem *)> lambda, const std::vector<AName>& labels, const std::vector<AName>& beforeLabels, bool renderThread = false);

        template <typename T>
        T* GetSystem(const AName &name)
        {
            ASystem* retSystem = nullptr;

            if (IsMainThread())
            {
                std::find_if(Systems.begin(), Systems.end(), [&name, &retSystem] (const std::unique_ptr<ASystem> &system)
                {
                    if (system->Labels.contains(name))
                    {
                        retSystem = system.get();
                        return true;
                    }
                    return false;
                });

                return dynamic_cast<T*>(retSystem);
            }
            else
            {
                std::find_if(SystemsRenderThread.begin(), SystemsRenderThread.end(), [&name, &retSystem](const std::unique_ptr<ASystem> &system)
                {
                    if (system->Labels.contains(name))
                    {
                        retSystem = system.get();
                        return true;
                    }
                    return false;
                });

                return dynamic_cast<T*>(retSystem);
            }
        }

        void ProcessSystems();
        
        void ProcessSystemsRenderThread();

        void QueueRenderThreadCall(std::function<void()> lambda);

        void SyncEntities();

        const std::vector<std::unique_ptr<AObject, no_deleter>> &GetObjectsByName(const AName &objectName);

        const void* GetObjectsByNameRaw(const AName &objectName);

        const std::vector<AEntity *> GetEntitiesWithComponents(const ComponentBitset &componentsNames);

        void ForEntitiesWithComponents(const ComponentBitset &componentMask, std::function<void(AEntity *)> lambda, bool parallel = false, ASystem* system = nullptr);

        ComponentBitset GetComponentMaskForComponents(std::vector<AName> componentsNames);

        size_t GetObjectCountByType(const AName &objectName);

        void Clear();

        void OnPreHotReload();

        void OnPostHotReload();

        void OnShutdown();

        template <typename T>
        void GetNamesOfComponents(std::vector<AName> &names)
        {
            static T tmp;
            static AName tmpName = tmp.GetClassData().Name;
            names.push_back(tmpName);
        }

        template <typename T1, typename T2, typename... Types>
        void GetNamesOfComponents(std::vector<AName> &names)
        {
            static AName tmpName = T1::GetClassDataStatic().Name;
            names.push_back(tmpName);

            GetNamesOfComponents<T2, Types...>(names);
        }

        template <typename T>
        bool ShouldComponentsBlockRenderThread()
        {
            static bool tmp = GetCDO<T>(T::GetClassDataStatic().Name)->_shouldBlockRenderThread;
            return tmp;
        }

        template <typename T1, typename T2, typename... Types>
        bool ShouldComponentsBlockRenderThread()
        {
            static bool tmp = (std::is_const<T1>::value ? false : GetCDO<T1>(T1::GetClassDataStatic().Name)->_shouldBlockRenderThread) || ShouldComponentsBlockRenderThread<T2, Types...>();
            return tmp;
        }

        template <typename T, typename... Types>
        const std::vector<AEntity *>& GetEntitiesWithComponents()
        {
            static std::vector<AName> names;
            static ComponentBitset mask;
            if (names.size() == 0)
            {
                GetNamesOfComponents<T, Types...>(names);
                mask = GetComponentMaskForComponents(names);
            }

            static uint32_t lastRegistryVersion = GetRegistryVersion();
            static auto entities = GetEntitiesWithComponents(mask);

            if (lastRegistryVersion != GetRegistryVersion())
            {
                lastRegistryVersion = GetRegistryVersion();
                entities = GetEntitiesWithComponents(mask);
            }

            return entities;
        }

        template <typename T>
        struct identity
        {
            typedef T type;
        };

        template<typename T>
        struct fun_type
        {
            using type = void;
        };

        template<typename Ret, typename Class, typename... Args>
        struct fun_type<Ret(Class::*)(Args...) const>
        {
            using type = std::function<Ret(Args...)>;
        };

        template<typename F>
        typename fun_type<decltype(&F::operator())>::type lambdaToFun(F const &func)
        {
            return func;
        }

        // TODO:
        // Check if the component type should block render (or physics or something) thread
        // If it does, then we need to queue the lambda to be executed during the sync phase
        // Otherwise, we can execute it immediately
        // If the component type is const, then we can execute it immediately as well

        template <typename T, typename... Types>
        void ForEntitiesWithComponents(identity<std::function<void(AEntity*, T*, Types*...)>>::type lambda, bool parallel = false)
        {
            static std::vector<AName> names;
            static ComponentBitset mask;
            static bool shouldQueue = false;
            if (names.size() == 0)
            {
                GetNamesOfComponents<T, Types...>(names);
                mask = GetComponentMaskForComponents(names);
                shouldQueue = ShouldComponentsBlockRenderThread<T, Types...>();
            }

            std::function<void(AEntity *)> lambdaWrapper = [lambda](AEntity *entity)
            {
                lambda(entity, entity->GetComponentOfType<T>(), entity->GetComponentOfType<Types>()...);
            };

            if (shouldQueue)
            {
                QueueSystem([this, lambdaWrapper, parallel]()
                {
                    ForEntitiesWithComponents(mask, lambdaWrapper, parallel);
                });
            }
            else
            {
                ForEntitiesWithComponents(mask, lambdaWrapper, parallel);
            }
        }

        template <typename T, typename... Types>
        void ForEntitiesWithComponents2(std::function<void(AEntity*, T*, Types*...)> lambda, bool parallel = false, ASystem* system = nullptr)
        {
            static std::vector<AName> names;
            static ComponentBitset mask;
            static bool shouldQueue = false;
            if (names.size() == 0)
            {
                GetNamesOfComponents<T, Types...>(names);
                mask = GetComponentMaskForComponents(names);
                shouldQueue = ShouldComponentsBlockRenderThread<T, Types...>();
            }

            std::function<void(AEntity *)> lambdaWrapper = [lambda](AEntity *entity)
            {
                lambda(entity, entity->GetComponentOfType<T>(), entity->GetComponentOfType<Types>()...);
            };

            if (shouldQueue)
            {
                QueueSystem([this, lambdaWrapper, parallel, system]()
                {
                    ForEntitiesWithComponents(mask, lambdaWrapper, parallel, system);
                });
            }
            else
            {
                ForEntitiesWithComponents(mask, lambdaWrapper, parallel, system);
            }
        }

        template <typename FunType>
        void ForEntitiesWithComponents(FunType lambda, bool parallel = false)
        {
            ForEntitiesWithComponents2(lambdaToFun(lambda), parallel);
        }

        template <typename FunType>
        void ForEntitiesWithComponentsParallel(FunType lambda)
        {
            ForEntitiesWithComponents2(lambdaToFun(lambda), true);
        }

        template <typename FunType>
        void ForEntitiesWithComponents(ASystem* system, FunType lambda, bool parallel = false)
        {
            ForEntitiesWithComponents2(lambdaToFun(lambda), parallel, system);
        }

private:
        double _lastFrameTime = 0.0;
        double _currentFrameTime = 0.0;

        float _deltaTime = 0.001f;

        uint32_t _frame = 0;
        uint32_t _registryVersion = 0;
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

        T *obj = static_cast<T *>(_world->GetObjectsByName(_typeName)[_uid].get());
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

        return static_cast<T *>(_world->GetObjectsByName(_typeName)[_uid].get());
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

        return static_cast<T *>(_world->GetObjectsByName(name)[_uid].get());
    }

    template<typename T>
    inline T* AEntity::GetComponentOfType() const
    {
        /*static const AName& name = T::GetClassDataStatic().Name;

        auto it = std::lower_bound(ComponentNames.begin(), ComponentNames.end(), name);
        if (it != ComponentNames.end() && *it == name)
        {
            int index = std::distance(ComponentNames.begin(), it);
            return static_cast<T *>(Components[index]);
        }

        return nullptr;*/

        // it seems binary search above is still faster than this on a small benchmark. Try on a more realistic scenario
        static const uint32_t staticIndex =
            std::distance(World->ComponentNames.begin(),
                          std::find(World->ComponentNames.begin(),
                                    World->ComponentNames.end(),
                                    T::GetClassDataStatic().Name));
        
        if (_componentMask[staticIndex] == 0)
        {
            return nullptr;
        }

        static const size_t shift = MAX_COMPONENTS - staticIndex;

        return static_cast<T*>(Components[(_componentMask << shift).count()]);
    }
}

#endif
