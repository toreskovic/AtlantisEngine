#include "renderer.h"
#include <iostream>

namespace Atlantis
{
    void Renderer::Process(Registry *registry)
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

            if (ren->textureHandle.get<Texture2D>() != nullptr)
                DrawTexture(*ren->textureHandle.get<Texture2D>(), pos->x, pos->y, col->col);
            /*else if (ren->renderType == typeRectangle)
                DrawRectangle(pos->x, pos->y, 50, 50, col->col);
            else if (ren->renderType == typeCircle)
                DrawCircle(pos->x, pos->y, 50, col->col);*/
            
        }
    }
}