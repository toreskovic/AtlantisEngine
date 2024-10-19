#include "physicsSystem.h"
#include "engine/renderer/renderer.h"

using namespace Atlantis;

void CDynamicBody2D::OnAddedToEntity(AEntity* entity)
{
    AComponent::OnAddedToEntity(entity);

    body.index1 = 0;
}

void Atlantis::CDynamicBody2D::OnRemovedFromEntity(AEntity* entity)
{
    AComponent::OnRemovedFromEntity(entity);

    b2DestroyBody(body);
    body.index1 = 0;
}

void SPhysics::Process(AWorld* world)
{
    float currentTime = world->GetGameTime();

    const float fixedDeltaTime = 1.0f / _frequency;
    bool doProcess = false;
    if (currentTime - _lastTime >= fixedDeltaTime)
    {
        _lastTime = currentTime - fixedDeltaTime;
        doProcess = true;
    }

    if (!doProcess)
    {
        return;
    }

    world->ForEntitiesWithComponents(
        [this](AEntity* entity, CDynamicBody2D* body, CPosition* position)
        {
            if (B2_IS_NULL(body->body))
            {
                b2BodyDef bodyDef = b2DefaultBodyDef();
                bodyDef.type = b2_dynamicBody;
                bodyDef.position = b2Vec2{position->x * _metersInUnit,
                                          position->y * _metersInUnit};

                bodyDef.userData = (void*)entity->_uid;
                body->body = b2CreateBody(_physicsWorld, &bodyDef);

                b2ShapeDef dynamicCircleDef = b2DefaultShapeDef();
                dynamicCircleDef.filter.groupIndex = 1;
                dynamicCircleDef.density = body->density;
                dynamicCircleDef.friction = body->friction;

                b2Circle dynamicCircle;
                dynamicCircle.center = b2Vec2{0.0f, 0.0f};
                dynamicCircle.radius = body->width * _metersInUnit / 3.0f;

                b2CreateCircleShape(
                    body->body, &dynamicCircleDef, &dynamicCircle);
            }
        });

    world->ForEntitiesWithComponentsParallel(
        [this](AEntity* entity, CDynamicBody2D* body, CPosition* position)
        {
            if (B2_IS_NULL(body->body))
            {
                return;
            }

            b2Body_SetLinearVelocity(body->body,
                                     b2Vec2{body->velocityX * _metersInUnit,
                                            body->velocityY * _metersInUnit});
        });

    b2World_Step(_physicsWorld, fixedDeltaTime, 2);
    world->GetTaskScheduler()->ResetTaskCount();

    _gridCoarse.Clear();

    // update grid
    world->ForEntitiesWithComponents(
        [this](AEntity* entity, const CPosition* position, CDynamicBody2D* body)
        {
            if (!body->enabled)
            {
                return;
            }

            float halfWidth = body->width / 2.0f;
            float halfHeight = body->height / 2.0f;

            _gridCoarse.AddEntity(entity,
                                  position->x - halfWidth,
                                  position->y - halfHeight,
                                  body->width,
                                  body->height);
        });

    world->ForEntitiesWithComponentsParallel(
        [this](AEntity* entity, CDynamicBody2D* body, CPosition* position)
        {
            if (B2_IS_NULL(body->body))
            {
                return;
            }

            b2Vec2 bodyPos = b2Body_GetPosition(body->body);
            position->x = bodyPos.x * _unitsInMeter;
            position->y = bodyPos.y * _unitsInMeter;
        });
}

class QueryCallbackHelper
{
public:

    std::vector<AEntity*> Entities;
    AWorld* World;
};

bool MyOverlapCallback(b2ShapeId shapeId, void* context)
{
    QueryCallbackHelper* callbackHelper =
        static_cast<QueryCallbackHelper*>(context);
    static AName typeName = AName("AEntity");

    b2BodyId body = b2Shape_GetBody(shapeId);
    size_t uid = (size_t)b2Body_GetUserData(body);
    AEntity* entity = static_cast<AEntity*>(
        callbackHelper->World->GetObjectsByName(typeName)[uid].get());
    callbackHelper->Entities.push_back(entity);
    return true;
};

std::vector<AEntity*> SPhysics::GetEntitiesInArea(float x,
                                                  float y,
                                                  float width,
                                                  float height)
{
    x *= _metersInUnit;
    y *= _metersInUnit;
    width *= _metersInUnit;
    height *= _metersInUnit;

    b2AABB aabb;
    aabb.lowerBound = b2Vec2{x, y};
    aabb.upperBound = b2Vec2{x + width, y + height};

    QueryCallbackHelper callbackHelper;
    callbackHelper.World = _world;
    b2QueryFilter filter = b2DefaultQueryFilter();
    b2World_OverlapAABB(
        _physicsWorld, aabb, filter, &MyOverlapCallback, &callbackHelper);

    return callbackHelper.Entities;
}

bool Atlantis::SPhysics::IsGroupInArea(uint8_t group,
                                       float x,
                                       float y,
                                       float width,
                                       float height)
{
    return _gridCoarse.ContainsGroup(group, x, y, width, height);
}
