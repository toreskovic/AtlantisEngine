#ifndef RENDERER_H
#define RENDERER_H

#include "raylib.h"

#include "engine/reflection/reflectionHelpers.h"
#include "engine/core.h"
#include "engine/system.h"
#include "./generated/renderer.gen.h"

namespace Atlantis
{
    struct CPosition : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        float x;
        DEF_PROPERTY();
        float y;
        DEF_PROPERTY();
        float z;

        CPosition(){};
        CPosition(const CPosition &other){};
    };

    struct CRenderable : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        AResourceHandle textureHandle;

        CRenderable(){};
        CRenderable(const CRenderable &other){};
    };

    struct CVelocity : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        float x;

        DEF_PROPERTY();
        float y;

        DEF_PROPERTY();
        float z;

        CVelocity(){};
        CVelocity(const CVelocity &other){};
    };

    struct CColor : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        Color col;

        CColor(){};
        CColor(const CColor &other){};
    };

    struct SRenderer : public ASystem
    {
        virtual void Process(ARegistry* registry) override;
    };
}

#endif
