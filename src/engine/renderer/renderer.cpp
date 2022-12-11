#include "renderer.h"
#include <iostream>

namespace Atlantis
{
    void SRenderer::Process(AWorld *world)
    {
        //static const std::vector<HName> components = {"CRenderable", "CPosition", "CColor"};
        //const auto& entities = world->GetEntitiesWithComponents(components);
        const auto& entities = world->GetEntitiesWithComponents<CRenderable, CPosition, CColor>();
        for (auto* e: entities)
        {
            CRenderable *ren = e->GetComponentOfType<CRenderable>();
            CPosition *pos = e->GetComponentOfType<CPosition>();
            CColor *col = e->GetComponentOfType<CColor>();

            ATextureResource *tex = ren->textureHandle.get<ATextureResource>();
            if (tex != nullptr)
                DrawTexture(tex->Texture, pos->x, pos->y, col->col);
        }
    }
}
