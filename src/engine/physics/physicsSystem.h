#pragma once

#include "box2d/box2d.h"
#include "engine/core.h"
#include "engine/physics/grid.h"
#include "engine/reflection/reflectionHelpers.h"
#include "engine/system.h"
#include "engine/taskScheduler.h"
#include "engine/world.h"
#include "generated/physicsSystem.gen.h"
#include <vector>

namespace Atlantis
{
struct CDynamicBody2D : public AComponent
{
    DEF_CLASS();

    DEF_PROPERTY();
    float width = 0.0f;

    DEF_PROPERTY();
    float height = 0.0f;

    DEF_PROPERTY();
    float density = 1.0f;

    DEF_PROPERTY();
    float friction = 0.0f;

    DEF_PROPERTY();
    float velocityX = 0.0f;

    DEF_PROPERTY();
    float velocityY = 0.0f;

    DEF_PROPERTY();
    bool enabled = true;

    DEF_PROPERTY();
    bool dirty = true;

    b2BodyId body;

    virtual void OnAddedToEntity(AEntity* entity) override;

    virtual void OnRemovedFromEntity(AEntity* entity) override;
};

static void* EnqueueTask(b2TaskCallback* task,
                         int32_t itemCount,
                         int32_t minRange,
                         void* taskContext,
                         void* userContext)
{
    ATaskScheduler* scheduler = static_cast<ATaskScheduler*>(userContext);
    if (scheduler->GetTaskCount() < MAX_TASKS)
    {
        ATask& sampleTask = scheduler->GetNewTask();
        sampleTask.StartIndex = 0;
        sampleTask.EndIndex = itemCount;
        sampleTask.MinRange = minRange;
        sampleTask.Task = task;
        sampleTask.TaskContext = taskContext;
        sampleTask.IsParentTask = false;
        sampleTask.ParentTask = nullptr;
        sampleTask.SubTaskCount = 0;
        scheduler->AddTask(&sampleTask);

        return &sampleTask;
    }
    else
    {
        // This is not fatal but MAX_TASKS should be increased
        assert(false);
        task(0, itemCount, 0, taskContext);

        return nullptr;
    }
};

static void FinishTask(void* taskPtr, void* userContext)
{
    if (taskPtr != nullptr)
    {
        ATask* task = static_cast<ATask*>(taskPtr);
        ATaskScheduler* scheduler = static_cast<ATaskScheduler*>(userContext);
        scheduler->WaitforTask(task);
    }
}

class SPhysics : public ASystem
{
public:

    SPhysics(AWorld* world)
        : ASystem()
        , _world(world)
    {
        _unitsInMeter = 32.0f;
        _metersInUnit = 1.0f / _unitsInMeter;

        b2WorldDef worldDef = b2DefaultWorldDef();
        worldDef.gravity = b2Vec2{0.0f, 0.0f};

        worldDef.workerCount = ATaskScheduler::GetThreadCount();
        worldDef.enqueueTask = EnqueueTask;
        worldDef.finishTask = FinishTask;
        worldDef.userTaskContext = world->GetTaskScheduler();
        worldDef.enableSleep = true;

        _physicsWorld = b2CreateWorld(&worldDef);
    };

    ~SPhysics() { b2DestroyWorld(_physicsWorld); }

    virtual void Process(AWorld* world) override;
    void Process2(AWorld* world);

    std::vector<AEntity*> GetEntitiesInArea(float x,
                                            float y,
                                            float width,
                                            float height);
    bool IsGroupInArea(uint8_t group,
                       float x,
                       float y,
                       float width,
                       float height);

private:

    AWorld* _world;
    b2WorldId _physicsWorld;
    float _unitsInMeter = 1.0f;
    float _metersInUnit = 1.0f;
    int _iterationsVelocity = 8;
    int _iterationsPosition = 3;
    float _frequency = 30.0f;
    float _lastTime = -1.0f;

    AGrid<1000, 1000, 24, 24> _grid;
    // coarser grid for broad phase collision detection
    // 4x ratio between the two grids
    AGridCoarse<250, 250, 128, 128> _gridCoarse;
};
}
