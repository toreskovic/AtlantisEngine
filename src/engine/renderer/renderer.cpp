#include "renderer.h"
#include <iostream>

namespace Atlantis
{
    Renderer::Renderer(Registry &registry)
    {
    }

    Renderer::~Renderer()
    {
    }

    void Renderer::Render(Registry &registry)
    {
        auto &comps = registry.GetAllComponentsByName(HName("renderable"));
        //std::cout << "-----------" << std::endl;
        for (auto &comp : comps)
        {
            renderable *ren = static_cast<renderable *>(comp.get());
            AEntity *e = ren->Owner;

            // position* pos = dynamic_cast<position*>(e->GetComponentOfType(HName("position")));
            // color* col = dynamic_cast<color*>(e->GetComponentOfType(HName("color")));

            position *pos = e->GetComponentOfType<position>();
            color *col = e->GetComponentOfType<color>();
            
            //std::cout << ren->renderType << std::endl;
            static const HName typeRectangle = "Rectangle";
            static const HName typeCircle = "Circle";

            if (ren->renderType == typeRectangle)
                DrawRectangle(pos->x, pos->y, 50, 50, col->col);
            else if (ren->renderType == typeCircle)
                DrawCircle(pos->x, pos->y, 50, col->col);
        }
    }
}