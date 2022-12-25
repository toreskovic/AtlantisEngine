#include "renderer.h"
#include <iostream>

namespace Atlantis
{
    void SRenderer::Process(AWorld *world)
    {
        static const ComponentBitset componentMask = world->GetComponentMaskForComponents({"CRenderable", "CPosition", "CColor"});
        // const auto& entities = world->GetEntitiesWithComponents(componentMask);

        /*const auto& entities = world->GetEntitiesWithComponents<CRenderable, CPosition, CColor>();
        for (auto* e: entities)
        {
            CRenderable *ren = e->GetComponentOfType<CRenderable>();
            CPosition *pos = e->GetComponentOfType<CPosition>();
            CColor *col = e->GetComponentOfType<CColor>();

            ATextureResource *tex = ren->textureHandle.get<ATextureResource>();
            if (tex != nullptr)
                DrawTexture(tex->Texture, pos->x, pos->y, col->col);
        }*/

        world->ForEntitiesWithComponents(componentMask, [&](AEntity *e)
                                         {
            CRenderable *ren = e->GetComponentOfType<CRenderable>();
            CPosition *pos = e->GetComponentOfType<CPosition>();
            CColor *col = e->GetComponentOfType<CColor>();

            ATextureResource *tex = ren->textureHandle.get<ATextureResource>();
            if (tex != nullptr)
            {
                DrawTexture(tex->Texture, pos->x, pos->y, col->col);
            } });
    }
}
