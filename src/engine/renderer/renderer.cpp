#include "renderer.h"
#include <iostream>

namespace Atlantis
{
    void Renderer::Process(ARegistry *registry)
    {
        static const std::vector<HName> components = {"renderable", "position", "color"};
        const auto& entities = registry->GetEntitiesWithComponents(components);
        for (auto* e: entities)
        {
            renderable *ren = e->GetComponentOfType<renderable>();
            position *pos = e->GetComponentOfType<position>();
            color *col = e->GetComponentOfType<color>();
            
            static const HName typeRectangle = "Rectangle";
            static const HName typeCircle = "Circle";

            ATextureResource *tex = ren->textureHandle.get<ATextureResource>();
            if (tex != nullptr)
                DrawTexture(tex->Texture, pos->x, pos->y, col->col);
        }
    }
}