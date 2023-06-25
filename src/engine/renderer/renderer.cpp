#include "renderer.h"
#include <iostream>
#include "engine/profiling.h"

namespace Atlantis
{
    void SRenderer::Process(AWorld *world)
    {
        DO_PROFILE("SRenderer::Process", DARKBLUE);
        static const ComponentBitset componentMask = world->GetComponentMaskForComponents({"CRenderable", "CPosition", "CColor"});
        // const auto& entities = world->GetEntitiesWithComponents(componentMask);

        static std::vector<AEntity*> entities;
        static uint lastRegistryVersion = -1;
        if (world->GetRegistryVersion() != lastRegistryVersion)
        {
            lastRegistryVersion = world->GetRegistryVersion();
            entities = world->GetEntitiesWithComponents<CRenderable, CPosition, CColor>();

            // insertion sort
            for (int i = 1; i < entities.size(); i++)
            {
                AEntity *key = entities[i];
                CPosition *posKey = key->GetComponentOfType<CPosition>();
                int j = i - 1;

                while (j >= 0 && entities[j]->GetComponentOfType<CPosition>()->z > posKey->z)
                {
                    entities[j + 1] = entities[j];
                    j = j - 1;
                }
                entities[j + 1] = key;
            }
        }

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
            CCamera *cam = cameras[0]->GetComponentOfType<CCamera>();
            CPosition *pos = cameras[0]->GetComponentOfType<CPosition>();
            Zoom = cam->Zoom;
            camX = pos->x;
            camY = pos->y;
        }

        for (auto *e : entities)
        {
            CRenderable *ren = e->GetComponentOfType<CRenderable>();
            CPosition *pos = e->GetComponentOfType<CPosition>();
            CColor *col = e->GetComponentOfType<CColor>();

            // scale using zoom
            auto x = (pos->x - halfWidth) * Zoom + halfWidth - camX * Zoom;
            auto y = (pos->y - halfHeight) * Zoom + halfHeight - camY * Zoom;

            // don't draw if outside of screen
            if (x + ren->cellSize * Zoom < 0 || x > width || y + ren->cellSize * Zoom < 0 || y > height)
            {
                continue;
            }

            ATextureResource *tex = ren->textureHandle.get<ATextureResource>();
            if (tex != nullptr)
            {
                if (ren->spriteHeight != 0 && ren->spriteWidth != 0)
                {
                    Rectangle source;
                    source.x = ren->spriteX * ren->spriteWidth;
                    source.y = ren->spriteY * ren->spriteHeight;
                    source.width = ren->spriteWidth;
                    source.height = ren->spriteHeight;
                    DrawTexturePro(tex->Texture, source, {x, y, ren->spriteWidth * Zoom, ren->spriteHeight * Zoom}, {0, 0}, 0.0f, col->col);
                }
                else
                {
                    DrawTextureEx(tex->Texture, {x, y}, 0.0f, Zoom, col->col);
                }
            }
        }

        /*world->ForEntitiesWithComponents(componentMask, [&](AEntity *e)
                                         {
            CRenderable *ren = e->GetComponentOfType<CRenderable>();
            CPosition *pos = e->GetComponentOfType<CPosition>();
            CColor *col = e->GetComponentOfType<CColor>();

            ATextureResource *tex = ren->textureHandle.get<ATextureResource>();
            if (tex != nullptr)
            {
                DrawTexture(tex->Texture, pos->x, pos->y, col->col);
            } });*/
    }
}
