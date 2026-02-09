#ifndef ATLANTIS_WORLD_H
#define ATLANTIS_WORLD_H

#include "engine/core.h"
#include "engine/reflection/reflectionHelpers.h"
#include "engine/taskScheduler.h"
#include "engine/ui/uiSystem.h"
#include "engine/renderer/renderProxy.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <atomic>
#include <bitset>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <execution>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Atlantis
{
struct AWorld;

struct no_deleter
{
    void operator()(void* const ptr) const {}
};

struct AObjectCreateCommand
{
    std::function<void()> Callback;
};

template <typename, typename = void>
struct has_oncreated : std::false_type {};

template <typename T>
struct has_oncreated<T, std::void_t<decltype(std::declval<T&>().OnCreated(std::declval<bool>))>> : std::true_type {};

// specialization when OnCreated exists
template <typename T, std::enable_if_t<has_oncreated<T>::value, int> = 0>
void OnCreated(T* t, bool firstTime = false) { t->OnCreated(firstTime); }

// fallback when OnCreated does not exist
template <typename T, std::enable_if_t<!has_oncreated<T>::value, int> = 0>
void OnCreated(T*, bool firstTime = false) { /* ... */ }

// views operate on entities with specific components, hold separate lists for entity and each component
struct ISystemViewBase
{
    virtual ~ISystemViewBase() = default;
    virtual void AddEntity(AEntity* entity) = 0;
    virtual void RemoveEntity(AEntity* entity) = 0;
    virtual void RefreshPointers(AWorld* world, size_t capacity) = 0;
    virtual const std::vector<AEntity*>& GetEntities() const = 0;
};

template<typename... Types>
struct ASystemView : public ISystemViewBase
{
    std::vector<size_t> Indices;
    std::vector<AEntity*> Entities;
    std::unordered_map<size_t, std::vector<void*>> ComponentVectors;
    std::unordered_map<size_t, size_t> EntityIndexById;

    template<typename C>
    std::vector<C*>& GetComponentVector()
    {
        return *reinterpret_cast<std::vector<C*>*>(&ComponentVectors[C::GetClassDataStatic().Name]);
    }

    void AddEntity(AEntity* entity) override
    {
        if (entity == nullptr)
        {
            return;
        }

        size_t uid = entity->_uid;
        if (EntityIndexById.contains(uid))
        {
            return;
        }

        size_t index = Entities.size();
        Entities.push_back(entity);
        Indices.push_back(index);
        EntityIndexById.emplace(uid, index);
        AddComponents(entity);
    }

    void RemoveEntity(AEntity* entity) override
    {
        if (entity == nullptr)
        {
            return;
        }

        auto it = EntityIndexById.find(entity->_uid);
        if (it == EntityIndexById.end())
        {
            return;
        }

        size_t index = it->second;
        size_t last = Entities.size() - 1;
        if (index != last)
        {
            Entities[index] = Entities[last];
            SwapComponents(index, last);

            AEntity* swappedEntity = Entities[index];
            if (swappedEntity != nullptr)
            {
                EntityIndexById[swappedEntity->_uid] = index;
            }
        }

        Entities.pop_back();
        Indices.pop_back();
        PopComponents();
        EntityIndexById.erase(it);
    }

    void RefreshPointers(AWorld* world, size_t capacity) override;

    const std::vector<AEntity*>& GetEntities() const override
    {
        return Entities;
    }

    void Reserve(size_t capacity)
    {
        Entities.reserve(capacity);
        Indices.reserve(capacity);
        (ComponentVectors[Types::GetClassDataStatic().Name].reserve(capacity), ...);
    }

private:
    void AddComponents(AEntity* entity)
    {
        (ComponentVectors[Types::GetClassDataStatic().Name].push_back(
             entity->GetComponentOfType<Types>()),
         ...);
    }

    void ResizeComponents(size_t size)
    {
        (ComponentVectors[Types::GetClassDataStatic().Name].resize(size), ...);
    }

    void RefreshComponents()
    {
        for (size_t i = 0; i < Entities.size(); ++i)
        {
            AEntity* entity = Entities[i];
            if (entity == nullptr)
            {
                (void)std::initializer_list<int>{(ComponentVectors[Types::GetClassDataStatic().Name][i] = nullptr, 0)...}; 
                continue;
            }

            (void)std::initializer_list<int>{(ComponentVectors[Types::GetClassDataStatic().Name][i] =
                 entity->GetComponentOfType<Types>(), 0
             )...};
        }
    }

    void SwapComponents(size_t a, size_t b)
    {
         (std::swap(ComponentVectors[Types::GetClassDataStatic().Name][a],
                    ComponentVectors[Types::GetClassDataStatic().Name][b]),
          ...);
    }

    void PopComponents()
    {
        (ComponentVectors[Types::GetClassDataStatic().Name].pop_back(), ...);
    }
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
    std::mutex FrameSyncMutex;
    std::mutex ProfilingMutex;
    std::condition_variable FrameReadyCv;
    std::condition_variable FrameDoneCv;
    std::condition_variable PhaseCv;
    const std::thread::id MAIN_THREAD_ID = std::this_thread::get_id();
    const std::thread::id RENDER_THREAD_ID;
    std::vector<std::function<void()>> RenderThreadCallQueue;
    std::vector<std::function<void()>> RenderThreadCallQueueAsync;
    std::mutex RenderThreadCallQueueAsyncMutex;
    std::vector<size_t> DirtyRenderProxyIds;

    // system views, mapped by component mask
    std::unordered_map<ComponentBitset, std::unique_ptr<ISystemViewBase>> SystemViews;

    AResourceHolder ResourceHolder = AResourceHolder(this);
    AInputHandler InputHandler;
    SUiSystem UiSystem;

    std::vector<std::function<void()>> ObjectCreateCommandsQueue;
    std::vector<std::function<void()>> ObjectModifyQueue;
    std::vector<AObjPtr<AObject>> ObjectDestroyQueue;

    std::vector<AName> ComponentNames;

    std::atomic<bool> MainThreadProcessing = false;
    std::atomic<bool> RenderThreadProcessing = false;
    bool MainSyncActive = false;
    bool RenderPreAsyncActive = false;
    bool ShutdownRequested = false;
    uint64_t FrameProduced = 0;
    uint64_t FrameRendered = 0;

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
        // RegisterDefault<T, 10000, 10000>(name);
        RegisterDefault<T, 2097152, 2097152>(name);
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
            OnCreated<T>(obj, false);

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

            RefreshSystemViews();
        }

        void* cpy = (void*)(allocatorHelper.Start +
                            allocatorHelper.Count * classData.Size);

        // void *cpy = malloc(classData.Size);
        memcpy(cpy, (void*)CDO, classData.Size);

        T* cpy_T = static_cast<T*>(cpy);

        cpy_T->_uid = allocatorHelper.Count;
        cpy_T->World = this;

        allocatorHelper.Count++;

        OnCreated<T>(cpy_T, true);

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

    std::vector<ARenderProxy2DHigh> RenderProxies2DHigh;
    std::vector<ARenderProxy2DHigh> RenderProxies2DHigh2;
    std::vector<ARenderProxy2DMid> RenderProxies2DMid;
    std::vector<ARenderProxy2DMid> RenderProxies2DMid2;
    std::vector<ARenderProxy2DLow> RenderProxies2DLow;
    std::vector<ARenderProxy2DLow> RenderProxies2DLow2;
    std::vector<ARenderProxy2DMeta> RenderProxies2DMeta;
    std::vector<ARenderProxy2DMeta> RenderProxies2DMeta2;
    std::atomic<bool> RenderUsingRenderProxies2 = true;

    size_t AddRenderProxy(const ARenderProxy2DHigh& high,
                          const ARenderProxy2DMid& mid,
                          const ARenderProxy2DLow& low,
                          const ARenderProxy2DMeta& meta);

    void RemoveRenderProxy(size_t uid);

    void MarkRenderProxyDirty(size_t uid);

    std::vector<ARenderProxy2DHigh>& GetMainRenderProxiesHigh();
    std::vector<ARenderProxy2DHigh>& GetRenderProxiesHigh();
    std::vector<ARenderProxy2DMid>& GetMainRenderProxiesMid();
    std::vector<ARenderProxy2DMid>& GetRenderProxiesMid();
    std::vector<ARenderProxy2DLow>& GetMainRenderProxiesLow();
    std::vector<ARenderProxy2DLow>& GetRenderProxiesLow();
    std::vector<ARenderProxy2DMeta>& GetMainRenderProxiesMeta();
    std::vector<ARenderProxy2DMeta>& GetRenderProxiesMeta();

    void UpdateSystemViewsForEntity(AEntity* entity,
                                    const ComponentBitset& oldMask,
                                    const ComponentBitset& newMask);

    void RefreshSystemViews();

    float GetDeltaTime() const;

    double GetGameTime() const;

    bool IsMainThread() const;
    bool IsGameThread() const;
    bool IsRenderThread() const;

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

    // system views
    template<typename... Types>
    void RegisterSystemView()
    {
        ComponentBitset mask = GetComponentMaskForComponents<Types...>();
        if (SystemViews.contains(mask))
        {
            return;
        }

        auto view = std::make_unique<ASystemView<Types...>>();
        ASystemView<Types...>* viewPtr = view.get();
        viewPtr->Reserve(AllocatorHelpers["AEntity"].Limit);
        SystemViews[mask] = std::move(view);

        const std::vector<AEntity*> entities = GetEntitiesWithComponents(mask);
        for (AEntity* entity : entities)
        {
            viewPtr->AddEntity(entity);
        }
    }

    template<typename... Types>
    ASystemView<Types...>* GetSystemView()
    {
        ComponentBitset mask = GetComponentMaskForComponents<Types...>();
        auto it = SystemViews.find(mask);
        if (it != SystemViews.end())
        {
            return static_cast<ASystemView<Types...>*>(it->second.get());
        }
        return nullptr;
    }

    void ProcessSystems();

    void ProcessSystemsRenderThread();

    void QueueRenderThreadCall(std::function<void()> lambda);
    void QueueRenderThreadCallAsync(std::function<void()> lambda);

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
    
    template<typename... Types>
    ComponentBitset GetComponentMaskForComponents()
    {
        static std::vector<AName> names;
        static ComponentBitset mask;
        if (names.size() == 0)
        {
            GetNamesOfComponents<Types...>(names);
            mask = GetComponentMaskForComponents(names);
        }

        return mask;
    }

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
            // shouldQueue = ShouldComponentsBlockRenderThread<T, Types...>();
            shouldQueue = false;
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
            // shouldQueue = ShouldComponentsBlockRenderThread<T, Types...>();
            shouldQueue = false;
        }

        if (shouldQueue)
        {
            std::function<void(AEntity*)> lambdaWrapper = [lambda](AEntity* entity)
            {
                lambda(entity,
                       entity->GetComponentOfType<T>(),
                       entity->GetComponentOfType<Types>()...);
            };

            QueueSystem(
                [this, lambdaWrapper, parallel, system]() {
                    ForEntitiesWithComponents(
                        mask, lambdaWrapper, parallel, system);
                });
            return;
        }

        ASystemView<T, Types...>* view = GetSystemView<T, Types...>();
        if (view != nullptr)
        {
            std::vector<AEntity*>& entities = view->Entities;
            std::vector<T*>& vecT = view->template GetComponentVector<T>();

            int start = 0;
            int end = static_cast<int>(entities.size());
            if (system != nullptr && system->IsTimesliced)
            {
                start = system->CurrentObjectIndex;
                end = start + system->ObjectsPerFrame;

                if (end > static_cast<int>(entities.size()))
                {
                    end = static_cast<int>(entities.size());
                    system->CurrentObjectIndex = 0;
                }
                else
                {
                    system->CurrentObjectIndex = end;
                }
            }

            auto invokeAt = [&](size_t index)
            {
                lambda(entities[index],
                       vecT[index],
                       view->template GetComponentVector<Types>()[index]...);
            };

            if (parallel)
            {
                std::vector<size_t> indices;
                if (system != nullptr && system->IsTimesliced)
                {
                    indices.reserve(end - start);
                    for (int i = start; i < end; ++i)
                    {
                        indices.push_back(static_cast<size_t>(i));
                    }
                }
                else
                {
                    indices = view->Indices;
                }

                std::for_each(std::execution::par, indices.begin(), indices.end(),
                              [&](size_t index) { invokeAt(index); });
            }
            else
            {
                for (int i = start; i < end; ++i)
                {
                    invokeAt(static_cast<size_t>(i));
                }
            }

            return;
        }

        std::function<void(AEntity*)> lambdaWrapper = [lambda](AEntity* entity)
        {
            lambda(entity,
                   entity->GetComponentOfType<T>(),
                   entity->GetComponentOfType<Types>()...);
        };

        ForEntitiesWithComponents(mask, lambdaWrapper, parallel, system);
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

template<typename... Types>
void ASystemView<Types...>::RefreshPointers(AWorld* world, size_t capacity)
{
    if (world == nullptr)
    {
        return;
    }

    Reserve(capacity);

    const AName entityType = AEntity::GetClassDataStatic().Name;
    const AEntity* entities =
        static_cast<const AEntity*>(world->GetObjectsByNameRaw(entityType));
    for (const auto& entry : EntityIndexById)
    {
        size_t uid = entry.first;
        size_t index = entry.second;
        Entities[index] = const_cast<AEntity*>(&entities[uid]);
    }

    ResizeComponents(Entities.size());
    RefreshComponents();
}
}

#endif // ATLANTIS_WORLD_H
