#include "engine/world.h"
#include "engine/core.h"
#include "engine/system.h"
#include "engine/taskScheduler.h"
#include "world.h"
#include <execution>

namespace Atlantis
{
void AWorld::QueueSystem(std::function<void()> lambda)
{
    ObjectModifyQueue.push_back(lambda);
}

void Atlantis::AWorld::QueueModifyObject(AObjPtr<AObject> object,
                                         std::function<void(AObject*)> lambda)
{
    ObjectModifyQueue.push_back(
        [object, lambda, this]()
        {
            AObject* obj = object.Get();
            lambda(obj);
            _registryVersion++;
        });
}

void AWorld::MarkObjectDead(AObject* object)
{
    object->_isAlive = false;
    DeadObjects[object->GetClassData().Name].push_back(object);
    _registryVersion++;
}

void AWorld::QueueObjectDeletion(AObjPtr<AObject> object)
{
    ObjectDestroyQueue.push_back(object);
}

size_t AWorld::AddRenderProxy(const ARenderProxy2DHigh& high,
                              const ARenderProxy2DMid& mid,
                              const ARenderProxy2DLow& low,
                              const ARenderProxy2DMeta& meta)
{
    size_t uid = meta.uid;
    std::vector<ARenderProxy2DHigh>& proxiesHigh = GetMainRenderProxiesHigh();
    std::vector<ARenderProxy2DMid>& proxiesMid = GetMainRenderProxiesMid();
    std::vector<ARenderProxy2DLow>& proxiesLow = GetMainRenderProxiesLow();
    std::vector<ARenderProxy2DMeta>& proxiesMeta = GetMainRenderProxiesMeta();
    if (proxiesHigh.size() <= uid)
    {
        // temp hardcoded increment, should probably use the reserved space for render components
        size_t newSize = uid + 10000;
        proxiesHigh.resize(newSize);
        proxiesMid.resize(newSize);
        proxiesLow.resize(newSize);
        proxiesMeta.resize(newSize);
    }
    proxiesHigh[uid] = high;
    proxiesMid[uid] = mid;
    proxiesLow[uid] = low;
    proxiesMeta[uid] = meta;
    MarkRenderProxyDirty(uid);
    return uid;
}

void AWorld::RemoveRenderProxy(size_t uid)
{
    std::vector<ARenderProxy2DHigh>& proxiesHigh = GetMainRenderProxiesHigh();
    std::vector<ARenderProxy2DMeta>& proxiesMeta = GetMainRenderProxiesMeta();
    if (uid >= proxiesHigh.size())
    {
        return;
    }

    proxiesHigh[uid].zoom = 0.0f;
    proxiesMeta[uid].uid = std::numeric_limits<size_t>::max();
    proxiesMeta[uid].textureResourceAddress = nullptr;
    MarkRenderProxyDirty(uid);
}

void AWorld::MarkRenderProxyDirty(size_t uid)
{
    DirtyRenderProxyIds.push_back(uid);
}

void AWorld::UpdateSystemViewsForEntity(AEntity* entity,
                                        const ComponentBitset& oldMask,
                                        const ComponentBitset& newMask)
{
    if (entity == nullptr)
    {
        return;
    }

    for (auto& entry : SystemViews)
    {
        const ComponentBitset& viewMask = entry.first;
        bool oldMatches = (oldMask & viewMask) == viewMask;
        bool newMatches = (newMask & viewMask) == viewMask;
        if (oldMatches == newMatches)
        {
            continue;
        }

        ISystemViewBase* view = entry.second.get();
        if (newMatches)
        {
            view->AddEntity(entity);
        }
        else
        {
            view->RemoveEntity(entity);
        }
    }
}

void AWorld::RefreshSystemViews()
{
    for (auto& entry : SystemViews)
    {
        entry.second->RefreshPointers(this);
    }
}

std::vector<ARenderProxy2DHigh>& AWorld::GetMainRenderProxiesHigh()
{
    return RenderUsingRenderProxies2.load() ? RenderProxies2DHigh : RenderProxies2DHigh2;
}

std::vector<ARenderProxy2DHigh>& AWorld::GetRenderProxiesHigh()
{
    return RenderUsingRenderProxies2.load() ? RenderProxies2DHigh2 : RenderProxies2DHigh;
}

std::vector<ARenderProxy2DMid>& AWorld::GetMainRenderProxiesMid()
{
    return RenderUsingRenderProxies2.load() ? RenderProxies2DMid : RenderProxies2DMid2;
}

std::vector<ARenderProxy2DMid>& AWorld::GetRenderProxiesMid()
{
    return RenderUsingRenderProxies2.load() ? RenderProxies2DMid2 : RenderProxies2DMid;
}

std::vector<ARenderProxy2DLow>& AWorld::GetMainRenderProxiesLow()
{
    return RenderUsingRenderProxies2.load() ? RenderProxies2DLow : RenderProxies2DLow2;
}

std::vector<ARenderProxy2DLow>& AWorld::GetRenderProxiesLow()
{
    return RenderUsingRenderProxies2.load() ? RenderProxies2DLow2 : RenderProxies2DLow;
}

std::vector<ARenderProxy2DMeta>& AWorld::GetMainRenderProxiesMeta()
{
    return RenderUsingRenderProxies2.load() ? RenderProxies2DMeta : RenderProxies2DMeta2;
}

std::vector<ARenderProxy2DMeta>& AWorld::GetRenderProxiesMeta()
{
    return RenderUsingRenderProxies2.load() ? RenderProxies2DMeta2 : RenderProxies2DMeta;
}

float AWorld::GetDeltaTime() const
{
    return _deltaTime;
}

double AWorld::GetGameTime() const
{
    return GetTime();
}

bool AWorld::IsMainThread() const
{
    return std::this_thread::get_id() == MAIN_THREAD_ID;
}

bool AWorld::IsGameThread() const
{
    return !IsRenderThread();
}

bool AWorld::IsRenderThread() const
{
    return std::this_thread::get_id() == RENDER_THREAD_ID;
}


uint32_t AWorld::GetRegistryVersion() const
{
    return _registryVersion;
}

ATaskScheduler* AWorld::GetTaskScheduler()
{
    return &TaskScheduler;
}

void AWorld::RegisterSystem(ASystem* system,
                            const std::vector<AName>& beforeLabels)
{
    std::unique_ptr<ASystem> systemPtr(system);

    if (system->IsRenderSystem)
    {
        if (beforeLabels.size() > 0)
        {
            for (int i = 0; i < SystemsRenderThread.size(); i++)
            {
                const ASystem* sys = SystemsRenderThread[i].get();

                for (const AName& label : beforeLabels)
                {
                    if (sys->Labels.count(label))
                    {
                        SystemsRenderThread.insert(SystemsRenderThread.begin() +
                                                       i,
                                                   std::move(systemPtr));
                        return;
                    }
                }
            }
        }

        SystemsRenderThread.push_back(std::move(systemPtr));
    }
    else
    {
        if (beforeLabels.size() > 0)
        {
            for (int i = 0; i < Systems.size(); i++)
            {
                const ASystem* sys = Systems[i].get();

                for (const AName& label : beforeLabels)
                {
                    if (sys->Labels.count(label))
                    {
                        Systems.insert(Systems.begin() + i,
                                       std::move(systemPtr));
                        return;
                    }
                }
            }
        }

        Systems.push_back(std::move(systemPtr));
    }
}

void AWorld::RegisterSystem(std::function<void(AWorld*)> lambda,
                            const std::vector<AName>& labels,
                            const std::vector<AName>& beforeLabels,
                            bool renderThread /* false */)
{
    ALambdaSystem* system = new ALambdaSystem();
    system->Lambda = lambda;
    system->Labels = { labels.begin(), labels.end() };
    system->IsRenderSystem = renderThread;

    RegisterSystem(system, beforeLabels);
}

void AWorld::RegisterSystemRenderThread(std::function<void(AWorld*)> lambda,
                                        const std::vector<AName>& labels,
                                        const std::vector<AName>& beforeLabels)
{
    RegisterSystem(lambda, labels, beforeLabels, true);
}

void AWorld::RegisterSystemTimesliced(
    int objectsPerFrame,
    std::function<void(AWorld*, ASystem*)> lambda,
    const std::vector<AName>& labels,
    const std::vector<AName>& beforeLabels,
    bool renderThread /* false */)
{
    ALambdaSystemTimesliced* system = new ALambdaSystemTimesliced();
    system->LambdaTimesliced = lambda;
    system->Labels = { labels.begin(), labels.end() };
    system->IsRenderSystem = renderThread;
    system->IsTimesliced = true;
    system->ObjectsPerFrame = objectsPerFrame;

    RegisterSystem(system, beforeLabels);
}

void AWorld::ProcessSystems()
{
    {
        std::unique_lock<std::mutex> lock(FrameSyncMutex);
        FrameDoneCv.wait(
            lock,
            [this]() { return ShutdownRequested || FrameProduced == FrameRendered; });
        if (ShutdownRequested)
        {
            return;
        }
    }

    _frame++;
    _currentFrameTime = GetTime();

    if (_lastFrameTime > 0.0f)
    {
        _deltaTime = _currentFrameTime - _lastFrameTime;
    }

    SyncEntities();

    {
        std::lock_guard<std::mutex> lock(FrameSyncMutex);
        FrameProduced++;
    }
    FrameReadyCv.notify_one();

    static AWorld* world = this;
    DO_PROFILE("AWorld::ProcessSystems - Systems", RED);
    for (std::unique_ptr<ASystem>& system : Systems)
    {
        system->Process(this);
    }

    UiSystem.PreDraw();

    ProfilerMainThread->Process(this);
    _lastFrameTime = _currentFrameTime;
}

void AWorld::ProcessSystemsRenderThread()
{
    {
        std::unique_lock<std::mutex> lock(FrameSyncMutex);
        FrameReadyCv.wait(
            lock,
            [this]() { return ShutdownRequested || FrameProduced > FrameRendered; });
        if (ShutdownRequested)
        {
            return;
        }
        PhaseCv.wait(lock, [this]() { return !MainSyncActive; });
        RenderPreAsyncActive = true;
        RenderThreadProcessing = true;
    }

    RenderThreadMutex.lock();
    BeginDrawing();

    for (std::function<void()>& lambda : RenderThreadCallQueue)
    {
        lambda();
    }

    RenderThreadCallQueue.clear();

    for (std::unique_ptr<ASystem>& system : SystemsRenderThread)
    {
        system->Process(this);
    }

    // Process input
    InputHandler.SyncInput();

    RenderThreadMutex.unlock();

    std::vector<std::function<void()>> asyncQueue;
    {
        std::lock_guard<std::mutex> lock(RenderThreadCallQueueAsyncMutex);
        asyncQueue.swap(RenderThreadCallQueueAsync);
    }
    for (std::function<void()>& lambda : asyncQueue)
    {
        lambda();
    }

    // UI
    UiSystem.Process(this);

    RenderThreadProcessing = false;

    ProfilerRenderThread->Process(this);
    EndDrawing();

    {
        std::lock_guard<std::mutex> lock(FrameSyncMutex);
        RenderPreAsyncActive = false;
        FrameRendered++;
    }

    PhaseCv.notify_all();
    FrameDoneCv.notify_all();
}

void AWorld::QueueRenderThreadCall(std::function<void()> lambda)
{
    std::lock_guard<std::mutex> lock(RenderThreadMutex);
    RenderThreadCallQueue.push_back(lambda);
}

void AWorld::QueueRenderThreadCallAsync(std::function<void()> lambda)
{
    std::lock_guard<std::mutex> lock(RenderThreadCallQueueAsyncMutex);
    RenderThreadCallQueueAsync.push_back(lambda);
}

void AWorld::SyncEntities()
{
    {
        std::unique_lock<std::mutex> lock(FrameSyncMutex);
        PhaseCv.wait(lock, [this]() { return !RenderPreAsyncActive; });
        MainSyncActive = true;
        MainThreadProcessing = true;
    }

    static AWorld* world = this;
    DO_PROFILE("AWorld::SyncEntities", RED);
    // Process object creation queue
    IsProcessingObjectCreationQueue = true;
    for (auto& command : ObjectCreateCommandsQueue)
    {
        command();
    }
    IsProcessingObjectCreationQueue = false;

    // Process object deletion queue
    for (auto& obj : ObjectDestroyQueue)
    {
        if (obj.IsValid())
        {
            obj->MarkObjectDead();
        }
    }

    // Process object iteration queue
    for (auto& command : ObjectModifyQueue)
    {
        command();
    }

    std::vector<ARenderProxy2DHigh>& mainHigh = GetMainRenderProxiesHigh();
    std::vector<ARenderProxy2DMid>& mainMid = GetMainRenderProxiesMid();
    std::vector<ARenderProxy2DLow>& mainLow = GetMainRenderProxiesLow();
    std::vector<ARenderProxy2DMeta>& mainMeta = GetMainRenderProxiesMeta();
    std::vector<ARenderProxy2DHigh>& renderHigh = GetRenderProxiesHigh();
    std::vector<ARenderProxy2DMid>& renderMid = GetRenderProxiesMid();
    std::vector<ARenderProxy2DLow>& renderLow = GetRenderProxiesLow();
    std::vector<ARenderProxy2DMeta>& renderMeta = GetRenderProxiesMeta();
    if (renderHigh.size() < mainHigh.size())
    {
        size_t newSize = mainHigh.size();
        renderHigh.resize(newSize);
        renderMid.resize(newSize);
        renderLow.resize(newSize);
        renderMeta.resize(newSize);
    }

    for (size_t uid : DirtyRenderProxyIds)
    {
        if (uid < mainHigh.size())
        {
            renderHigh[uid] = mainHigh[uid];
            renderMid[uid] = mainMid[uid];
            renderLow[uid] = mainLow[uid];
            renderMeta[uid] = mainMeta[uid];
        }
    }

    DirtyRenderProxyIds.clear();
    RenderUsingRenderProxies2.store(!RenderUsingRenderProxies2.load());

    {
        std::lock_guard<std::mutex> lock(FrameSyncMutex);
        MainThreadProcessing = false;
        MainSyncActive = false;
    }

    PhaseCv.notify_all();

    ObjectCreateCommandsQueue.clear();
    ObjectDestroyQueue.clear();
    ObjectModifyQueue.clear();
}

const std::vector<std::unique_ptr<AObject, no_deleter>>&
AWorld::GetObjectsByName(const AName& objectName)
{
    const std::vector<std::unique_ptr<AObject, no_deleter>>& objList =
        ObjectLists[objectName];

    return objList;
}

const void* AWorld::GetObjectsByNameRaw(const AName& objectName)
{
    return (void*)AllocatorHelpers[objectName].Start;
}

size_t AWorld::GetObjectCountByType(const AName& objectName)
{
    return GetObjectsByName(objectName).size();
}

const std::vector<AEntity*> AWorld::GetEntitiesWithComponents(
    const ComponentBitset& componentMask)
{
    static const AName entityName = "AEntity";

    std::vector<AEntity*> intersection;

    const AEntity* entities = (const AEntity*)GetObjectsByNameRaw(entityName);
    const size_t entityCount = GetObjectCountByType(entityName);

    intersection.reserve(entityCount);

    for (int i = 0; i < entityCount; i++)
    {
        AEntity* entity = const_cast<AEntity*>(&entities[i]);

        bool isValid =
            entity->_isAlive && entity->HasComponentsByMask(componentMask);

        if (isValid)
        {
            intersection.push_back(entity);
        }
    }

    return intersection;
}

void AWorld::ForEntitiesWithComponents(const ComponentBitset& componentMask,
                                       std::function<void(AEntity*)> lambda,
                                       bool parallel,
                                       ASystem* system)
{
    const AEntity* entities = nullptr;
    size_t entityCount = 0;
    const std::vector<AEntity*>* viewEntities = nullptr;

    auto viewIt = SystemViews.find(componentMask);
    if (viewIt != SystemViews.end())
    {
        viewEntities = &viewIt->second->GetEntities();
        entityCount = viewEntities->size();
    }
    else
    {
        static const AName entityName = "AEntity";
        entities = (const AEntity*)GetObjectsByNameRaw(entityName);
        entityCount = GetObjectCountByType(entityName);
    }

    int start = 0;
    int end = static_cast<int>(entityCount);

    if (system != nullptr && system->IsTimesliced)
    {
        start = system->CurrentObjectIndex;
        end = start + system->ObjectsPerFrame;

        if (end > entityCount)
        {
            end = entityCount;
            system->CurrentObjectIndex = 0;
        }
        else
        {
            system->CurrentObjectIndex = end;
        }
    }

    if (parallel)
    {
        if (viewEntities != nullptr)
        {
            std::for_each(std::execution::par,
                          viewEntities->begin() + start,
                          viewEntities->begin() + end,
                          [lambda](AEntity* entity)
                          {
                              if (entity != nullptr)
                              {
                                  lambda(entity);
                              }
                          });
        }
        else
        {
            std::for_each(std::execution::par, entities + start, entities + end,
                          [lambda, componentMask](const AEntity& entity)
                          {
                              AEntity* ent = const_cast<AEntity*>(&entity);

                              if (ent->_isAlive &&
                                  ent->HasComponentsByMask(componentMask))
                              {
                                  lambda(ent);
                              }
                          });
        }
    }
    else
    {
        if (viewEntities != nullptr)
        {
            for (int i = start; i < end; i++)
            {
                AEntity* entity = (*viewEntities)[i];
                if (entity != nullptr)
                {
                    lambda(entity);
                }
            }
        }
        else
        {
            for (int i = start; i < end; i++)
            {
                AEntity* entity = const_cast<AEntity*>(&entities[i]);

                if (entity->_isAlive && entity->HasComponentsByMask(componentMask))
                {
                    lambda(entity);
                }
            }
        }
    }
}

ComponentBitset AWorld::GetComponentMaskForComponents(
    std::vector<AName> componentsNames)
{
    ComponentBitset ret = 0x0;

    for (int i = 0; i < componentsNames.size(); i++)
    {
        for (int j = 0; j < ComponentNames.size(); j++)
        {
            if (componentsNames[i] == ComponentNames[j])
            {
                ret.set(j, true);
            }
        }
    }

    return ret;
}

void AWorld::Clear()
{
    CData.clear();
    CDOs.clear();
    ObjectLists.clear();
    DeadObjects.clear();
    Systems.clear();
    SystemsRenderThread.clear();
    SystemViews.clear();
    ObjectCreateCommandsQueue.clear();
    ObjectDestroyQueue.clear();
    ObjectModifyQueue.clear();
    ComponentNames.clear();

    for (auto thing : AllocatorHelpers)
    {
        free((void*)thing.second.Start);
    }

    AllocatorHelpers.clear();
    RenderProxies2DHigh.clear();
    RenderProxies2DHigh2.clear();
    RenderProxies2DMid.clear();
    RenderProxies2DMid2.clear();
    RenderProxies2DLow.clear();
    RenderProxies2DLow2.clear();
    RenderProxies2DMeta.clear();
    RenderProxies2DMeta2.clear();
    RenderThreadMutex.unlock();
}

void AWorld::OnPreHotReload()
{
    ObjectCreateCommandsQueue.clear();
    ObjectDestroyQueue.clear();
    ObjectModifyQueue.clear();
}

void AWorld::OnPostHotReload()
{
    // TODO: hack
    _currentFrameTime = GetTime();
    _deltaTime = 0.0001f;
    _lastFrameTime = _currentFrameTime - _deltaTime;
    _frame++;
}

void AWorld::OnShutdown()
{
    {
        std::lock_guard<std::mutex> lock(FrameSyncMutex);
        ShutdownRequested = true;
        MainThreadProcessing = false;
        RenderThreadProcessing = false;
        MainSyncActive = false;
        RenderPreAsyncActive = false;
    }
    PhaseCv.notify_all();
    FrameReadyCv.notify_all();
    FrameDoneCv.notify_all();
}
}
