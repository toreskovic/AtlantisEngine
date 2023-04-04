#include "core.h"
#include "system.h"
#include <vector>
#include <iostream>

#define SERIALIZE_PROP_HELPER(type)                            \
    if (propData.Type == #type)                                \
    {                                                          \
        type prop = GetProperty<type>(propData.Name);          \
        type defProp = cdo->GetProperty<type>(propData.Name);  \
        propJson["Value"] = prop;                              \
        propJson["IsDefault"] = Helper_IsEqual(prop, defProp); \
    }

#define DESERIALIZE_PROP_HELPER(type)                                            \
    if (prop["Type"].get<std::string>() == #type)                                \
    {                                                                            \
        SetProperty(prop["Name"].get<std::string>(), prop["Value"].get<type>()); \
    }

namespace Atlantis
{
    template <typename T>
    bool Helper_IsEqual(const T &l, const T &r)
    {
        return l == r;
    }

    template <>
    bool Helper_IsEqual(const Color &l, const Color &r)
    {
        return l.r == r.r && l.g == r.g && l.b == r.b && l.a == r.a;
    }

    template <>
    bool Helper_IsEqual(const Texture2D &l, const Texture2D &r)
    {
        return l.id == r.id;
    }

    void AObject::MarkObjectDead()
    {
        if (World != nullptr)
        {
            World->MarkObjectDead(this);
        }
    }

    nlohmann::json AObject::Serialize()
    {
        nlohmann::json json;
        const auto &classData = GetClassData();

        json["Name"] = classData.Name.GetName();
        json["Properties"] = nlohmann::json::array({});
        for (const auto &propData : classData.Properties)
        {
            nlohmann::json propJson = {{"Name", propData.Name.GetName()}, {"Type", propData.Type.GetName()}, {"Offset", propData.Offset}};
            const AObject *cdo = World->GetCDO<AObject>(classData.Name);

            SERIALIZE_PROP_HELPER(bool);
            SERIALIZE_PROP_HELPER(int);
            SERIALIZE_PROP_HELPER(float);
            SERIALIZE_PROP_HELPER(double);
            SERIALIZE_PROP_HELPER(Color);
            SERIALIZE_PROP_HELPER(Texture2D);
            SERIALIZE_PROP_HELPER(std::string);
            SERIALIZE_PROP_HELPER(Atlantis::AName);
            SERIALIZE_PROP_HELPER(Atlantis::AResourceHandle);

            json["Properties"].push_back(propJson);
        }

        return json;
    }

    void AObject::Deserialize(const nlohmann::json &json)
    {
        for (auto &prop : json["Properties"])
        {
            if (prop["IsDefault"])
            {
                continue;
            }

            DESERIALIZE_PROP_HELPER(bool);
            DESERIALIZE_PROP_HELPER(int);
            DESERIALIZE_PROP_HELPER(float);
            DESERIALIZE_PROP_HELPER(double);
            DESERIALIZE_PROP_HELPER(Color);
            DESERIALIZE_PROP_HELPER(Texture2D);
            DESERIALIZE_PROP_HELPER(std::string);
            DESERIALIZE_PROP_HELPER(Atlantis::AName);
            DESERIALIZE_PROP_HELPER(Atlantis::AResourceHandle);
        }
    }

    void AComponent::MarkObjectDead()
    {
        AObject::MarkObjectDead();

        if (Owner != nullptr)
        {
            Owner->RemoveComponent(this);
        }
    }

    void AEntity::MarkObjectDead()
    {
        // remove all components from entity
        for (AComponent *component : Components)
        {
            component->OnRemovedFromEntity(this);
        }

        Components.clear();
        ComponentNames.clear();

        _componentMask.reset();

        AObject::MarkObjectDead();
    }

    void AEntity::AddComponent(AComponent *component)
    {
        Components.push_back(component);

        ComponentNames.push_back(component->GetClassData().Name);
        std::sort(ComponentNames.begin(), ComponentNames.end());

        _componentMask = World->GetComponentMaskForComponents(ComponentNames);

        component->OnAddedToEntity(this);
    }

    void AEntity::RemoveComponent(AComponent *component)
    {
        for (int i = 0; i < Components.size(); i++)
        {
            if (Components[i] == component)
            {
                Components.erase(Components.begin() + i);
                ComponentNames.erase(ComponentNames.begin() + i);
                _componentMask = World->GetComponentMaskForComponents(ComponentNames);

                component->OnRemovedFromEntity(this);
                break;
            }
        }
    }

    bool AEntity::HasComponentOfType(const AName &name)
    {
        for (const AName &compName : ComponentNames)
        {
            if (compName == name)
            {
                return true;
            }
        }

        return false;
    }

    bool AEntity::HasComponentsByMask(const ComponentBitset &mask)
    {
        return (mask & _componentMask) == mask;
    }

    bool AEntity::HasComponentsOfType(const std::vector<AName> &names)
    {
        return HasComponentsByMask(World->GetComponentMaskForComponents(names));
    }

    void AWorld::QueueSystem(std::function<void()> lambda)
    {
        ObjectModifyQueue.push_back(lambda);
    }

    void AWorld::QueueModifyObject(AObject *object, std::function<void(AObject *)> lambda)
    {
        ObjectModifyQueue.push_back([object, lambda, this]()
                                    { 
                        lambda(object);
                        _registryVersion++; });
    }

    void AWorld::MarkObjectDead(AObject *object)
    {
        object->_isAlive = false;
        DeadObjects[object->GetClassData().Name].push_back(object);
        _registryVersion++;
    }

    void AWorld::QueueObjectDeletion(AObject *object)
    {
        ObjectDestroyQueue.push_back(object);
    }

    float AWorld::GetDeltaTime() const
    {
        return _deltaTime;
    }

    bool AWorld::IsMainThread() const
    {
        return std::this_thread::get_id() == MAIN_THREAD_ID;
    }

    uint AWorld::GetRegistryVersion() const
    {
        return _registryVersion;
    }

    void AWorld::RegisterSystem(ASystem *system, const std::vector<AName> &beforeLabels)
    {
        std::unique_ptr<ASystem> systemPtr(system);

        if (system->IsRenderSystem)
        {
            if (beforeLabels.size() > 0)
            {
                for (int i = 0; i < SystemsRenderThread.size(); i++)
                {
                    const ASystem *sys = SystemsRenderThread[i].get();

                    for (const AName &label : beforeLabels)
                    {
                        if (sys->Labels.count(label))
                        {
                            SystemsRenderThread.insert(SystemsRenderThread.begin() + i, std::move(systemPtr));
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
                    const ASystem *sys = Systems[i].get();

                    for (const AName &label : beforeLabels)
                    {
                        if (sys->Labels.count(label))
                        {
                            Systems.insert(Systems.begin() + i, std::move(systemPtr));
                            return;
                        }
                    }
                }
            }

            Systems.push_back(std::move(systemPtr));
        }
    }

    void AWorld::RegisterSystem(std::function<void(AWorld *)> lambda, const std::vector<AName> &labels, const std::vector<AName> &beforeLabels, bool renderThread /* false */)
    {
        ALambdaSystem *system = new ALambdaSystem();
        system->Lambda = lambda;
        system->Labels = {labels.begin(), labels.end()};
        system->IsRenderSystem = renderThread;

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

        for (std::unique_ptr<ASystem> &system : Systems)
        {
            system->Process(this);
        }

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

        for (std::function<void()> &lambda : RenderThreadCallQueue)
        {
            lambda();
        }

        RenderThreadCallQueue.clear();

        for (std::unique_ptr<ASystem> &system : SystemsRenderThread)
        {
            system->Process(this);
        }

        RenderThreadProcessing = false;
        MainThreadProcessing = true;
        RenderThreadMutex.unlock();
    }

    void AWorld::QueueRenderThreadCall(std::function<void()> lambda)
    {
        RenderThreadMutex.lock();
        RenderThreadCallQueue.push_back(lambda);
        RenderThreadMutex.unlock();
    }

    void AWorld::SyncEntities()
    {
        while (RenderThreadProcessing)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }

        RenderThreadMutex.lock();
        MainThreadProcessing = true;

        // Process object creation queue
        for (auto &command : ObjectCreateCommandsQueue)
        {
            command();
        }

        // Process object deletion queue
        for (auto &obj : ObjectDestroyQueue)
        {
            if (obj->_isAlive)
            {
                obj->MarkObjectDead();
            }
        }

        // Process object iteration queue
        for (auto &command : ObjectModifyQueue)
        {
            command();
        }

        MainThreadProcessing = false;
        RenderThreadProcessing = true;
        RenderThreadMutex.unlock();

        ObjectCreateCommandsQueue.clear();
        ObjectDestroyQueue.clear();
        ObjectModifyQueue.clear();
    }

    const std::vector<std::unique_ptr<AObject, no_deleter>> &AWorld::GetObjectsByName(const AName &objectName)
    {
        const std::vector<std::unique_ptr<AObject, no_deleter>> &objList = ObjectLists[objectName];

        return objList;
    }

    size_t AWorld::GetObjectCountByType(const AName &objectName)
    {
        return GetObjectsByName(objectName).size();
    }

    const std::vector<AEntity *> AWorld::GetEntitiesWithComponents(const ComponentBitset &componentMask)
    {
        std::vector<AEntity *> intersection;
        const auto &entities = GetObjectsByName("AEntity");
        intersection.reserve(entities.size());

        for (auto &entityObj : entities)
        {
            AEntity *entity = static_cast<AEntity *>(entityObj.get());

            bool isValid = entity->_isAlive && entity->HasComponentsByMask(componentMask);

            if (isValid)
            {
                intersection.push_back(entity);
            }
        }

        return intersection;
    }

    void AWorld::ForEntitiesWithComponents(const ComponentBitset &componentMask, std::function<void(AEntity *)> lambda, bool parallel)
    {
        const auto &entities = GetObjectsByName("AEntity");

        if (parallel)
        {
// MSVC currently supports only OpenMP 2.0, which doesn't like range-based for loops :(
#pragma omp parallel for
            for (int i = 0; i < entities.size(); i++)
            {
                AEntity *entity = static_cast<AEntity *>(entities[i].get());

                if (entity->_isAlive && entity->HasComponentsByMask(componentMask))
                {
                    lambda(entity);
                }
            }
        }
        else
        {
            for (auto &entityObj : entities)
            {
                AEntity *entity = static_cast<AEntity *>(entityObj.get());

                if (entity->_isAlive && entity->HasComponentsByMask(componentMask))
                {
                    lambda(entity);
                }
            }
        }
    }

    ComponentBitset AWorld::GetComponentMaskForComponents(std::vector<AName> componentsNames)
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
            free((void *)thing.second.Start);
        }

        AllocatorHelpers.clear();
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
    }

    AResourceHandle AResourceHolder::GetTexture(std::string path)
    {
        if (World == nullptr)
        {
            std::cout << "AResourceHolder::GetTexture | Error: World is null" << std::endl;
            return AResourceHandle();
        }

        if (Resources.contains(path))
        {
            AResourceHandle ret(Resources.at(path).get());
            return ret;
        }

        if (World->IsMainThread())
        {
            World->QueueRenderThreadCall([this, path]()
                                         { Resources.emplace(path, std::move(std::make_unique<ATextureResource>(LoadTexture((Helpers::GetProjectDirectory().string() + path).c_str())))); });
        }
        else
        {
            Resources.emplace(path, std::move(std::make_unique<ATextureResource>(LoadTexture((Helpers::GetProjectDirectory().string() + path).c_str()))));
        }

        AResourceHandle ret(this, path);
        return ret;
    }

    void *AResourceHolder::GetResourcePtr(std::string path)
    {
        if (Resources.contains(path))
        {
            return Resources.at(path).get();
        }

        return nullptr;
    }

} // namespace Atlantis
