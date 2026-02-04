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

size_t AWorld::AddRenderProxy(const ARenderProxy2D& proxy)
{
    if (RenderProxies2D.size() > proxy._uid)
    {
        RenderProxies2D[proxy._uid] = proxy;
        RenderProxies2D2[proxy._uid] = proxy;
        return proxy._uid;
    }

    RenderProxies2D.push_back(proxy);
    RenderProxies2D2.push_back(proxy);
    return RenderProxies2D.size();
}

void AWorld::RemoveRenderProxy(size_t uid)
{
    ARenderProxy2D& proxy = RenderProxies2D[uid];
    ARenderProxy2D& proxy2 = RenderProxies2D2[uid];
    proxy._uid = std::numeric_limits<size_t>::max();
    proxy.zoom = 0.0f;
    proxy2._uid = std::numeric_limits<size_t>::max();
    proxy2.zoom = 0.0f;
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
    _frame++;
    _currentFrameTime = GetTime();

    if (_lastFrameTime > 0.0f)
    {
        _deltaTime = _currentFrameTime - _lastFrameTime;
    }

    SyncEntities();

    static AWorld* world = this;
    DO_PROFILE("AWorld::ProcessSystems - Systems", RED);
    for (std::unique_ptr<ASystem>& system : Systems)
    {
        system->Process(this);
    }

    ProfilerMainThread->Process(this);
    _lastFrameTime = _currentFrameTime;
}

void AWorld::ProcessSystemsRenderThread()
{
    while (MainThreadProcessing)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    RenderThreadMutex.lock();
    RenderThreadProcessing = true;
    RenderUsingRenderProxies2 = !RenderUsingRenderProxies2;

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

    MainThreadProcessing = true;
    RenderThreadMutex.unlock();

    for (std::function<void()>& lambda : RenderThreadCallQueueAsync)
    {
        lambda();
    }
    RenderThreadCallQueueAsync.clear();

    // UI
    UiSystem.Process(this);

    RenderThreadProcessing = false;

    ProfilerRenderThread->Process(this);
    EndDrawing();
}

void AWorld::QueueRenderThreadCall(std::function<void()> lambda)
{
    RenderThreadMutex.lock();
    RenderThreadCallQueue.push_back(lambda);
    RenderThreadMutex.unlock();
}

void AWorld::QueueRenderThreadCallAsync(std::function<void()> lambda)
{
    // todo: atomic push
    RenderThreadCallQueueAsync.push_back(lambda);
}

void AWorld::SyncEntities()
{
    while (RenderThreadProcessing)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    RenderThreadMutex.lock();
    MainThreadProcessing = true;
    while (MainUsingRenderProxies2 == RenderUsingRenderProxies2)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
    MainUsingRenderProxies2 = !MainUsingRenderProxies2;
    RenderThreadMutex.unlock();

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

    RenderThreadMutex.lock();
    MainThreadProcessing = false;
    RenderThreadProcessing = true;
    RenderThreadMutex.unlock();

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
    static const AName entityName = "AEntity";
    const AEntity* entities = (const AEntity*)GetObjectsByNameRaw(entityName);
    const size_t entityCount = GetObjectCountByType(entityName);

    int start = 0;
    int end = entityCount;

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
        std::for_each(std::execution::par, entities + start, entities + end,
                      [lambda, componentMask, this](const AEntity& entity)
                        {
                            AEntity* ent = const_cast<AEntity*>(&entity);
    
                            if (ent->_isAlive &&
                                ent->HasComponentsByMask(componentMask))
                            {
                                lambda(ent);
                            }
                        });
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
    ObjectCreateCommandsQueue.clear();
    ObjectDestroyQueue.clear();
    ObjectModifyQueue.clear();
    ComponentNames.clear();

    for (auto thing : AllocatorHelpers)
    {
        free((void*)thing.second.Start);
    }

    AllocatorHelpers.clear();
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
    MainThreadProcessing = false;
    RenderThreadProcessing = false;
    MainUsingRenderProxies2 = !RenderUsingRenderProxies2;
}
}
