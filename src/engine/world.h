#ifndef ATLANTIS_WORLD_H
#define ATLANTIS_WORLD_H

#include "engine/core.h"
#include "engine/reflection/reflectionHelpers.h"
#include "engine/taskScheduler.h"
#include "engine/ui/uiSystem.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <atomic>
#include <bitset>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Atlantis
{
struct no_deleter
{
    void operator()(void* const ptr) const {}
};

struct AObjectCreateCommand
{
    std::function<void()> Callback;
};

struct AWorld
{
    std::map<AName, AClassData, ANameComparer> CData;

    std::map<AName, std::unique_ptr<AObject>, ANameComparer> CDOs;
    std::map<AName,
             std::vector<std::unique_ptr<AObject, no_deleter>>,
             ANameComparer>
        ObjectLists;
    std::map<AName, std::vector<AObjPtr<AObject>>> DeadObjects;
    std::vector<std::unique_ptr<ASystem>> Systems;
    // TEMP for testing
    std::vector<std::unique_ptr<ASystem>> SystemsRenderThread;
    std::mutex RenderThreadMutex;
    std::mutex ProfilingMutex;
    const std::thread::id MAIN_THREAD_ID = std::this_thread::get_id();
    std::vector<std::function<void()>> RenderThreadCallQueue;

    AResourceHolder ResourceHolder = AResourceHolder(this);
    AInputHandler InputHandler;
    SUiSystem UiSystem;

    std::vector<std::function<void()>> ObjectCreateCommandsQueue;
    std::vector<std::function<void()>> ObjectModifyQueue;
    std::vector<AObjPtr<AObject>> ObjectDestroyQueue;

    std::vector<AName> ComponentNames;

    std::atomic<bool> MainThreadProcessing = false;
    std::atomic<bool> RenderThreadProcessing = false;

    bool IsProcessingObjectCreationQueue = false;

    ATaskScheduler TaskScheduler;

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

    template<typename T, size_t Amount, size_t Increment = Amount>
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

        T* objPtr = &obj;
        if (dynamic_cast<AComponent*>(objPtr) != nullptr)
        {
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

    template<typename T>
    void RegisterDefault(AName name = AName::None())
    {
        RegisterDefault<T, 10000>(name);
    }

    template<typename T>
    const T* GetCDO(const AName& name)
    {
        return dynamic_cast<T*>(CDOs[name].get());
    }

    template<typename T>
    T* NewObject_Base(const AName& name)
    {
        _registryVersion++;
        const T* CDO = GetCDO<T>(name);

        AClassData classData = CDO->GetClassData();

        // reuse dead objects
        if (DeadObjects[name].size() > 0)
        {
            AObjPtr<AObject> objPtr = DeadObjects[name].back();
            DeadObjects[name].pop_back();

            T* obj = static_cast<T*>(objPtr.Get(name, false));
            obj->_isAlive = true;

            return obj;
        }

        // allocate new objects
        AllocatorMemoryHelper& allocatorHelper =
            AllocatorHelpers.at(classData.Name);
        if (allocatorHelper.Count >= allocatorHelper.Limit)
        {
            std::cout << "Reallocating memory for type "
                      << classData.Name.GetName() << " from "
                      << allocatorHelper.Limit << " to "
                      << allocatorHelper.Limit + allocatorHelper.Increment
                      << std::endl;

            allocatorHelper.Limit += allocatorHelper.Increment;
            allocatorHelper.Start =
                (size_t)realloc((void*)allocatorHelper.Start,
                                allocatorHelper.Limit * classData.Size);

            ObjectLists[name].clear();
            ObjectLists[name].reserve(allocatorHelper.Limit);
            DeadObjects[name].reserve(allocatorHelper.Limit);

            for (size_t i = 0; i < allocatorHelper.Count; i++)
            {
                T* objPtr = static_cast<T*>(
                    (void*)(allocatorHelper.Start + i * classData.Size));
                std::unique_ptr<AObject, no_deleter> sPtr(objPtr);
                ObjectLists[name].push_back(std::move(sPtr));

                // TODO: Ugly
                if (AEntity* entity = dynamic_cast<AEntity*>(objPtr))
                {
                    for (auto* component : entity->Components)
                    {
                        component->Owner = entity;
                    }
                }
                else if (AComponent* component =
                             dynamic_cast<AComponent*>(objPtr))
                {
                    if (component->Owner != nullptr)
                    {
                        for (int j = 0; j < component->Owner->Components.size();
                             j++)
                        {
                            if (component->Owner->ComponentNames[j] ==
                                classData.Name)
                            {
                                component->Owner->Components[j] = component;
                                break;
                            }
                        }
                    }
                }
            }
        }

        void* cpy = (void*)(allocatorHelper.Start +
                            allocatorHelper.Count * classData.Size);

        // void *cpy = malloc(classData.Size);
        memcpy(cpy, (void*)CDO, classData.Size);

        T* cpy_T = static_cast<T*>(cpy);

        cpy_T->_uid = allocatorHelper.Count;
        cpy_T->World = this;

        allocatorHelper.Count++;

        std::unique_ptr<AObject, no_deleter> sPtr(cpy_T);
        ObjectLists[name].push_back(std::move(sPtr));

        auto& vec = ObjectLists[name];

        return static_cast<T*>(vec[vec.size() - 1].get());
    }

    template<typename T>
    T* NewObject_Internal(const AName& name)
    {
        T* obj = NewObject_Base<T>(name);

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

    template<typename T>
    T* NewObject_Internal()
    {
        return NewObject_Internal<T>(T::GetClassDataStatic().Name);
    }

    template<typename T>
    void QueueNewObject(std::function<void(AObjPtr<T>)> lambda)
    {
        if (IsProcessingObjectCreationQueue)
        {
            AObjPtr<T> obj = NewObject_Internal<T>();
            lambda(obj);
        }
        else
        {
            ObjectCreateCommandsQueue.push_back(
                [this, lambda]()
                {
                    AObjPtr<T> obj = NewObject_Internal<T>();
                    lambda(obj);
                });
        }
    }

    void QueueSystem(std::function<void()> lambda);

    void QueueModifyObject(AObjPtr<AObject> object,
                           std::function<void(AObject*)> lambda);

    void MarkObjectDead(AObject* object);

    void QueueObjectDeletion(AObjPtr<AObject> object);

    float GetDeltaTime() const;

    double GetGameTime() const;

    bool IsMainThread() const;

    uint32_t GetRegistryVersion() const;

    ATaskScheduler* GetTaskScheduler();

    ~AWorld()
    {
        for (auto thing : AllocatorHelpers)
        {
            free((void*)thing.second.Start);
        }
    }

    void RegisterSystem(ASystem* system,
                        const std::vector<AName>& beforeLabels = {});

    void RegisterSystem(std::function<void(AWorld*)> lambda,
                        const std::vector<AName>& labels = {},
                        const std::vector<AName>& beforeLabels = {},
                        bool renderThread = false);

    void RegisterSystemRenderThread(
        std::function<void(AWorld*)> lambda,
        const std::vector<AName>& labels = {},
        const std::vector<AName>& beforeLabels = {});

    void RegisterSystemTimesliced(int objectsPerFrame,
                                  std::function<void(AWorld*, ASystem*)> lambda,
                                  const std::vector<AName>& labels,
                                  const std::vector<AName>& beforeLabels,
                                  bool renderThread = false);

    template<typename T>
    T* GetSystem(const AName& name)
    {
        ASystem* retSystem = nullptr;

        if (IsMainThread())
        {
            std::find_if(
                Systems.begin(),
                Systems.end(),
                [&name, &retSystem](const std::unique_ptr<ASystem>& system)
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
            std::find_if(
                SystemsRenderThread.begin(),
                SystemsRenderThread.end(),
                [&name, &retSystem](const std::unique_ptr<ASystem>& system)
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

    const std::vector<std::unique_ptr<AObject, no_deleter>>& GetObjectsByName(
        const AName& objectName);

    const void* GetObjectsByNameRaw(const AName& objectName);

    const std::vector<AEntity*> GetEntitiesWithComponents(
        const ComponentBitset& componentsNames);

    void ForEntitiesWithComponents(const ComponentBitset& componentMask,
                                   std::function<void(AEntity*)> lambda,
                                   bool parallel = false,
                                   ASystem* system = nullptr);

    ComponentBitset GetComponentMaskForComponents(
        std::vector<AName> componentsNames);

    size_t GetObjectCountByType(const AName& objectName);

    void Clear();

    void OnPreHotReload();

    void OnPostHotReload();

    void OnShutdown();

    template<typename T>
    void GetNamesOfComponents(std::vector<AName>& names)
    {
        static T tmp;
        static AName tmpName = tmp.GetClassData().Name;
        names.push_back(tmpName);
    }

    template<typename T1, typename T2, typename... Types>
    void GetNamesOfComponents(std::vector<AName>& names)
    {
        static AName tmpName = T1::GetClassDataStatic().Name;
        names.push_back(tmpName);

        GetNamesOfComponents<T2, Types...>(names);
    }

    template<typename T>
    bool ShouldComponentsBlockRenderThread()
    {
        static bool tmp =
            GetCDO<T>(T::GetClassDataStatic().Name)->_shouldBlockRenderThread;
        return tmp;
    }

    template<typename T1, typename T2, typename... Types>
    bool ShouldComponentsBlockRenderThread()
    {
        static bool tmp = (std::is_const<T1>::value
                               ? false
                               : GetCDO<T1>(T1::GetClassDataStatic().Name)
                                     ->_shouldBlockRenderThread) ||
                          ShouldComponentsBlockRenderThread<T2, Types...>();
        return tmp;
    }

    template<typename T, typename... Types>
    const std::vector<AEntity*>& GetEntitiesWithComponents()
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

    template<typename T>
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
    struct fun_type<Ret (Class::*)(Args...) const>
    {
        using type = std::function<Ret(Args...)>;
    };

    template<typename F>
    typename fun_type<decltype(&F::operator())>::type lambdaToFun(F const& func)
    {
        return func;
    }

    // TODO:
    // Check if the component type should block render (or physics or something)
    // thread If it does, then we need to queue the lambda to be executed during
    // the sync phase Otherwise, we can execute it immediately If the component
    // type is const, then we can execute it immediately as well

    template<typename T, typename... Types>
    void ForEntitiesWithComponents(
        typename identity<std::function<void(AEntity*, T*, Types*...)>>::type lambda,
        bool parallel = false)
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

        std::function<void(AEntity*)> lambdaWrapper = [lambda](AEntity* entity)
        {
            lambda(entity,
                   entity->GetComponentOfType<T>(),
                   entity->GetComponentOfType<Types>()...);
        };

        if (shouldQueue)
        {
            QueueSystem(
                [this, lambdaWrapper, parallel]()
                { ForEntitiesWithComponents(mask, lambdaWrapper, parallel); });
        }
        else
        {
            ForEntitiesWithComponents(mask, lambdaWrapper, parallel);
        }
    }

    template<typename T, typename... Types>
    void ForEntitiesWithComponents2(
        std::function<void(AEntity*, T*, Types*...)> lambda,
        bool parallel = false,
        ASystem* system = nullptr)
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

        std::function<void(AEntity*)> lambdaWrapper = [lambda](AEntity* entity)
        {
            lambda(entity,
                   entity->GetComponentOfType<T>(),
                   entity->GetComponentOfType<Types>()...);
        };

        if (shouldQueue)
        {
            QueueSystem(
                [this, lambdaWrapper, parallel, system]() {
                    ForEntitiesWithComponents(
                        mask, lambdaWrapper, parallel, system);
                });
        }
        else
        {
            ForEntitiesWithComponents(mask, lambdaWrapper, parallel, system);
        }
    }

    template<typename FunType>
    void ForEntitiesWithComponents(FunType lambda, bool parallel = false)
    {
        ForEntitiesWithComponents2(lambdaToFun(lambda), parallel);
    }

    template<typename FunType>
    void ForEntitiesWithComponentsParallel(FunType lambda,
                                           ASystem* system = nullptr)
    {
        ForEntitiesWithComponents2(lambdaToFun(lambda), true, system);
    }

    template<typename FunType>
    void ForEntitiesWithComponents(ASystem* system,
                                   FunType lambda,
                                   bool parallel = false)
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
}

#endif // ATLANTIS_WORLD_H
