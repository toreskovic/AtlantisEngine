#ifndef RENDERER_H
#define RENDERER_H

#include "raylib.h"

#include "engine/reflection/reflectionHelpers.h"
#include "engine/core.h"
#include "engine/system.h"
#include "./generated/renderer.gen.h"

namespace Atlantis
{
    struct position : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        float x;
        DEF_PROPERTY();
        float y;
        DEF_PROPERTY();
        float z;

        position(){};
        position(const position &other){};
    };

    struct renderable : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        AResourceHandle textureHandle;

        renderable(){};
        renderable(const renderable &other){};
    };

    struct velocity : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        float x;

        DEF_PROPERTY();
        float y;

        DEF_PROPERTY();
        float z;

        velocity(){};
        velocity(const velocity &other){};
    };

    struct color : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        Color col;

        color(){};
        color(const color &other){};
    };

    struct Renderer : public ASystem
    {
        virtual void Process(ARegistry* registry) override;
    };
}

#endif
