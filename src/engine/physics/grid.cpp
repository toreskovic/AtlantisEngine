#include "grid.h"
#include "engine/world.h"

using namespace Atlantis;

void AGridHelpers::GetEntitiesInArea(AWorld* world, float x, float y, float width, float height, std::vector<AEntity*>& entities, int maxElementsToCheck)
{
    // check if NaN
    if (x != x || y != y || width != width || height != height)
    {
        return;
    }

    if (x < 0.0f || y < 0.0f || width < 0.0f || height < 0.0f)
    {
        return;
    }

    static const AName typeName = AName("AEntity");
    auto& allEntities = world->GetObjectsByName(typeName);

    int32_t left = (int32_t)(x / _cellWidth);
    int32_t top = (int32_t)(y / _cellHeight);
    int32_t right = (int32_t)((x + width) / _cellWidth);
    int32_t bottom = (int32_t)((y + height) / _cellHeight);

    int counter = 0;

    for (int32_t y = top; y <= bottom; y++)
    {
        for (int32_t x = left; x <= right; x++)
        {
            int32_t elementIndex = Nodes[y * _width + x].ElementIndex;
            while (elementIndex != -1)
            {
                AGridElement& element = (*Elements)[elementIndex];
                entities.push_back(static_cast<AEntity*>(allEntities[element.EntityId].get()));
                elementIndex = element.NextIndex;

                counter++;

                if (maxElementsToCheck > 0 && counter >= maxElementsToCheck)
                {
                    return;
                }
            }
        }
    }
}

void Atlantis::AGridHelpers::GetEntitiesInArea(
    AWorld* world,
    float x,
    float y,
    float width,
    float height,
    SmallArray<AEntity*, 32>& entities)
{
    // check if NaN
    if (x != x || y != y || width != width || height != height)
    {
        return;
    }

    if (x < 0.0f || y < 0.0f || width < 0.0f || height < 0.0f)
    {
        return;
    }

    static const AName typeName = AName("AEntity");
    auto& allEntities = world->GetObjectsByName(typeName);

    int32_t left = (int32_t)(x / _cellWidth);
    int32_t top = (int32_t)(y / _cellHeight);
    int32_t right = (int32_t)((x + width) / _cellWidth);
    int32_t bottom = (int32_t)((y + height) / _cellHeight);

    int counter = 0;

    for (int32_t y = top; y <= bottom; y++)
    {
        for (int32_t x = left; x <= right; x++)
        {
            int32_t elementIndex = Nodes[y * _width + x].ElementIndex;
            while (elementIndex != -1)
            {
                AGridElement& element = (*Elements)[elementIndex];
                entities.Add(static_cast<AEntity*>(allEntities[element.EntityId].get()));
                elementIndex = element.NextIndex;

                counter++;
                
                if (counter >= 32)
                {
                    return;
                }
            }
        }
    }
}
