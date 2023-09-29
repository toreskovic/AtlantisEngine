#include "renderer.h"
#include "engine/profiling.h"
#include <iostream>

namespace Atlantis
{
void SRenderer::Process(AWorld* world)
{
    DO_PROFILE("SRenderer::Process", DARKBLUE);

    // get camera
    float Zoom = 1.0f;
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    int halfWidth = width / 2;
    int halfHeight = height / 2;

    int camX = 0;
    int camY = 0;

    auto& cameras = world->GetEntitiesWithComponents<CCamera, CPosition>();
    if (cameras.size() > 0)
    {
        CCamera* cam = cameras[0]->GetComponentOfType<CCamera>();
        CPosition* pos = cameras[0]->GetComponentOfType<CPosition>();
        Zoom = cam->Zoom;
        camX = pos->x;
        camY = pos->y;
    }

    static const ComponentBitset componentMask =
        world->GetComponentMaskForComponents(
            { "CRenderable", "CPosition", "CColor" });

    static std::vector<size_t> entitiesIds;

    static const AName entityName = "AEntity";
    const size_t entityCount = world->GetObjectCountByType(entityName);
    const AEntity* rawEntities = (const AEntity*)world->GetObjectsByNameRaw(entityName);

    if (entitiesIds.size() < entityCount)
    {
        for (int i = entitiesIds.size(); i < entityCount; i++)
        {
            entitiesIds.push_back(rawEntities[i]._uid);
        }
    }

    for (size_t entityId = 0; entityId < entitiesIds.size(); entityId++)
    {
        AEntity* e = (AEntity*)&rawEntities[entityId];

        if (!e->_isAlive || !e->HasComponentsByMask(componentMask))
        {
            continue;
        }

        CRenderable* ren = e->GetComponentOfType<CRenderable>();
        CPosition* pos = e->GetComponentOfType<CPosition>();
        CColor* col = e->GetComponentOfType<CColor>();

        // scale using zoom
        auto x = (pos->x - halfWidth) * Zoom + halfWidth - camX * Zoom;
        auto y = (pos->y - halfHeight) * Zoom + halfHeight - camY * Zoom;

        // don't draw if outside of screen
        if (x + ren->cellSize * Zoom < 0 || x > width ||
            y + ren->cellSize * Zoom < 0 || y > height)
        {
            continue;
        }

        ATextureResource* tex = ren->textureHandle.get<ATextureResource>();
        if (tex != nullptr)
        {
            if (ren->spriteHeight != 0 && ren->spriteWidth != 0)
            {
                Rectangle source;
                source.x = ren->spriteX * ren->spriteWidth;
                source.y = ren->spriteY * ren->spriteHeight;
                source.width = ren->spriteWidth;
                source.height = ren->spriteHeight;

                DrawTexturePro(
                    tex->Texture,
                    source,
                    { x, y, ren->spriteWidth * Zoom, ren->spriteHeight * Zoom },
                    { 0, 0 },
                    0.0f,
                    col->col);
            }
            else
            {
                DrawTextureEx(tex->Texture, { x, y }, 0.0f, Zoom, col->col);
            }
        }
    }
}
}
