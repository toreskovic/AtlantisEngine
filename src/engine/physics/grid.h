#pragma once
#include "./generated/grid.gen.h"
#include "engine/core.h"

namespace Atlantis
{
class AWorld;

struct CGridElement : public AComponent
{
    DEF_CLASS();

    DEF_PROPERTY()
    std::vector<int32_t> ElementIndices;

    DEF_PROPERTY();
    uint8_t Group = 0;
};

struct AGridNode
{
    // index of the next node in the list
    int32_t NextIndex = -1;

    // index of the first element in the list
    int32_t ElementIndex = -1;
};

struct AGridElement
{
    // index of the next element in the list
    int32_t NextIndex = -1;

    // index of the previous element in the list
    int32_t PrevIndex = -1;

    // id of the entity this element belongs to
    int32_t EntityId = -1;

    // index of the node this element belongs to
    uint32_t NodeIndex;

    float x;
    float y;
};

struct AGridHelpers
{
    void GetEntitiesInArea(AWorld* world,
                           float x,
                           float y,
                           float width,
                           float height,
                           std::vector<AEntity*>& entities,
                           int maxElementsToCheck = -1);

    void GetEntitiesInArea(AWorld* world,
                           float x,
                           float y,
                           float width,
                           float height,
                           SmallArray<AEntity*, 32>& entities);

    uint32_t _cellWidth;
    uint32_t _cellHeight;
    uint32_t _width;
    uint32_t _height;

    AGridNode* Nodes;
    std::vector<AGridElement>* Elements;

    int MaxElementsToCheck = 4;
};

struct AGridNodeCoarse
{
    std::bitset<8> ContainedGroups = 0;
};

template<uint32_t gridWidth,
         uint32_t gridHeight,
         uint32_t cellWidth,
         uint32_t cellHeight>
struct AGridCoarse
{
    AGridNodeCoarse Nodes[gridHeight][gridWidth];

    uint32_t CellWidth = cellWidth;
    uint32_t CellHeight = cellHeight;

    void Clear()
    {
        // clear all nodes
        memset(Nodes, 0, sizeof(Nodes));
    }

    void AddEntity(AEntity* entity, float x, float y, float width, float height)
    {
        // NaN check
        if (x != x || y != y || width != width || height != height)
        {
            return;
        }

        int32_t left = (int32_t)(x / cellWidth);
        int32_t top = (int32_t)(y / cellHeight);
        int32_t right = (int32_t)((x + width) / cellWidth);
        int32_t bottom = (int32_t)((y + height) / cellHeight);

        CGridElement* gridComponent =
            entity->GetComponentOfType<CGridElement>();

        for (int32_t y = top; y <= bottom; y++)
        {
            for (int32_t x = left; x <= right; x++)
            {
                int32_t index = y * gridWidth + x;

                AGridNodeCoarse& node = Nodes[y][x];
                node.ContainedGroups.set(gridComponent->Group, true);
            }
        }
    }

    bool ContainsGroup(uint8_t group,
                       float x,
                       float y,
                       float width,
                       float height)
    {
        // NaN check
        if (x != x || y != y || width != width || height != height)
        {
            return false;
        }

        int32_t left = (int32_t)(x / cellWidth);
        int32_t top = (int32_t)(y / cellHeight);
        int32_t right = (int32_t)((x + width) / cellWidth);
        int32_t bottom = (int32_t)((y + height) / cellHeight);

        for (int32_t y = top; y <= bottom; y++)
        {
            for (int32_t x = left; x <= right; x++)
            {
                int32_t index = y * gridWidth + x;

                AGridNodeCoarse& node = Nodes[y][x];
                if (node.ContainedGroups.test(group))
                {
                    return true;
                }
            }
        }

        return false;
    }
};

template<uint32_t gridWidth,
         uint32_t gridHeight,
         uint32_t cellWidth,
         uint32_t cellHeight>
struct AGrid
{
    friend class SPhysics;
    AGridNode Nodes[gridHeight][gridWidth];
    AGridHelpers Helpers;

    std::vector<AGridElement> Elements;
    std::vector<uint32_t> EmptyElementIds;

    float ElementWidth = 32.0f;
    float ElementHeight = 32.0f;

    AGrid(uint32_t elementsCount = 10000)
    {
        for (uint32_t y = 0; y < gridHeight; y++)
        {
            for (uint32_t x = 0; x < gridWidth; x++)
            {
                Nodes[y][x].NextIndex = -1;
                Nodes[y][x].ElementIndex = -1;
            }
        }

        Elements.reserve(elementsCount);
        Helpers._cellWidth = cellWidth;
        Helpers._cellHeight = cellHeight;
        Helpers._width = gridWidth;
        Helpers._height = gridHeight;
        Helpers.Nodes = &Nodes[0][0];
        Helpers.Elements = &Elements;
    }

    void Add(AEntity* entity, float posX, float posY, float width, float height)
    {
        int32_t left = (int32_t)(posX / cellWidth);
        int32_t top = (int32_t)(posY / cellHeight);
        int32_t right = (int32_t)((posX + width) / cellWidth);
        int32_t bottom = (int32_t)((posY + height) / cellHeight);

        CGridElement* gridComponent =
            entity->GetComponentOfType<CGridElement>();

        // check if the entity is already in the grid
        if (gridComponent->ElementIndices.size() > 0)
        {
            return;
        }

        for (int32_t y = top; y <= bottom; y++)
        {
            for (int32_t x = left; x <= right; x++)
            {
                int32_t index = y * gridWidth + x;

                AGridElement element;
                element.EntityId = entity->_uid;
                element.NodeIndex = index;
                element.x = posX;
                element.y = posY;

                if (EmptyElementIds.size() > 0)
                {
                    Elements[EmptyElementIds.back()] = element;
                    gridComponent->ElementIndices.push_back(
                        EmptyElementIds.back());

                    // Elements[Nodes[y][x].ElementIndex].NextIndex =
                    // EmptyElementIds.back();
                    //  get the last element from the list
                    if (Nodes[y][x].ElementIndex == -1)
                    {
                        Nodes[y][x].ElementIndex = EmptyElementIds.back();
                    }
                    else
                    {
                        int32_t elementIndex = Nodes[y][x].ElementIndex;
                        while (Elements[elementIndex].NextIndex != -1)
                        {
                            elementIndex = Elements[elementIndex].NextIndex;
                        }

                        Elements[elementIndex].NextIndex =
                            EmptyElementIds.back();
                        element.PrevIndex = elementIndex;
                    }

                    EmptyElementIds.pop_back();
                }
                else
                {
                    gridComponent->ElementIndices.push_back(Elements.size());
                    // Elements[Nodes[y][x].ElementIndex].NextIndex =
                    // Elements.size();
                    if (Nodes[y][x].ElementIndex == -1)
                    {
                        Nodes[y][x].ElementIndex = Elements.size();
                    }
                    else
                    {
                        // get the last element from the list
                        int32_t elementIndex = Nodes[y][x].ElementIndex;
                        while (Elements[elementIndex].NextIndex != -1)
                        {
                            elementIndex = Elements[elementIndex].NextIndex;
                        }

                        Elements[elementIndex].NextIndex = Elements.size();
                        element.PrevIndex = elementIndex;
                    }

                    Elements.push_back(element);
                }
            }
        }
    }

    void Remove(AEntity* entity)
    {
        CGridElement* gridComponent =
            entity->GetComponentOfType<CGridElement>();
        if (gridComponent == nullptr)
        {
            return;
        }

        for (int32_t i = 0; i < gridComponent->ElementIndices.size(); i++)
        {
            int32_t index = gridComponent->ElementIndices[i];
            AGridElement& element = Elements[index];
            if (element.EntityId != entity->_uid)
            {
                continue;
            }

            uint32_t nodeIndex = element.NodeIndex;

            int32_t x = nodeIndex % gridWidth;
            int32_t y = nodeIndex / gridWidth;

            int32_t prevIndex = element.PrevIndex;
            int32_t nextIndex = element.NextIndex;

            if (Nodes[y][x].ElementIndex == index)
            {
                Nodes[y][x].ElementIndex = nextIndex;
            }

            if (prevIndex != -1)
            {
                Elements[prevIndex].NextIndex = nextIndex;
            }

            if (nextIndex != -1)
            {
                Elements[nextIndex].PrevIndex = prevIndex;
            }

            element.EntityId = -1;
            element.NextIndex = -1;
            element.NodeIndex = -1;
            element.x = 0.0f;
            element.y = 0.0f;

            EmptyElementIds.push_back(index);
        }

        std::vector<int32_t> elementsIndices = gridComponent->ElementIndices;

        gridComponent->ElementIndices.clear();
    }

    void Update(AEntity* entity, float x, float y, float width, float height)
    {
        if (x < 0.0f || y < 0.0f || x + width > gridWidth * cellWidth ||
            y + height > gridHeight * cellHeight)
        {
            return;
        }

        // check NaN
        if (x != x || y != y || width != width || height != height)
        {
            return;
        }

        // only update if the entity is in a different cell
        int32_t left = (int32_t)(x / cellWidth);
        int32_t top = (int32_t)(y / cellHeight);
        int32_t right = (int32_t)((x + width) / cellWidth);
        int32_t bottom = (int32_t)((y + height) / cellHeight);

        CGridElement* gridComponent =
            entity->GetComponentOfType<CGridElement>();
        bool needsUpdate = true;
        if (gridComponent->ElementIndices.size() == 0)
        {
            needsUpdate = true;
        }
        else
        {
            for (int32_t i = 0; i < gridComponent->ElementIndices.size(); i++)
            {
                int32_t index = gridComponent->ElementIndices[i];
                AGridElement& element = Elements[index];
                if (element.EntityId != entity->_uid)
                {
                    continue;
                }

                int32_t nodeIndex = element.NodeIndex;
                int32_t x = nodeIndex % gridWidth;
                int32_t y = nodeIndex / gridWidth;

                if (x >= left && x <= right && y >= top && y <= bottom)
                {
                    continue;
                }

                needsUpdate = true;
                break;
            }
        }

        if (!needsUpdate)
        {
            return;
        }

        Remove(entity);
        Add(entity, x, y, width, height);
    }

    void GetEntitiesInArea(AWorld* world,
                           float x,
                           float y,
                           float width,
                           float height,
                           std::vector<AEntity*>& entities,
                           int maxElementsToCheck = -1)
    {
        Helpers.GetEntitiesInArea(
            world, x, y, width, height, entities, maxElementsToCheck);
    }

    void GetEntitiesInArea(AWorld* world,
                           float x,
                           float y,
                           float width,
                           float height,
                           SmallArray<AEntity*, 32>& entities)
    {
        Helpers.GetEntitiesInArea(world, x, y, width, height, entities);
    }

    void GetElementsInArea(float xPos,
                           float yPos,
                           float width,
                           float height,
                           std::vector<AGridElement*>& elements)
    {
        // check if NaN
        if (xPos != xPos || yPos != yPos || width != width || height != height)
        {
            return;
        }

        int32_t left = (int32_t)(xPos / cellWidth);
        int32_t top = (int32_t)(yPos / cellHeight);
        int32_t right = (int32_t)((xPos + width) / cellWidth);
        int32_t bottom = (int32_t)((yPos + height) / cellHeight);

        for (int32_t y = top; y <= bottom; y++)
        {
            for (int32_t x = left; x <= right; x++)
            {
                int32_t index = y * gridWidth + x;

                int32_t elementIndex = Nodes[y][x].ElementIndex;
                while (elementIndex != -1)
                {
                    AGridElement& element = Elements[elementIndex];
                    if (element.x >= xPos && element.x < xPos + ElementWidth &&
                        element.y >= yPos && element.y < yPos + ElementHeight)
                    {
                        elements.push_back(&element);
                    }

                    elementIndex = element.NextIndex;
                }
            }
        }
    }

    uint32_t _cellWidth = cellWidth;
    uint32_t _cellHeight = cellHeight;
};
}