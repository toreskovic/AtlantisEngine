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
        float x = 0.0f;
        DEF_PROPERTY();
        float y = 0.0f;
        DEF_PROPERTY();
        float z = 0.0f;

        CPosition() : AComponent() { _shouldBlockRenderThread = true; };
        CPosition(const CPosition &other) { _shouldBlockRenderThread = true; };
    };

    struct CRenderable : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        AResourceHandle textureHandle;

        CRenderable() : AComponent() { _shouldBlockRenderThread = true; };
        CRenderable(const CRenderable &other){ _shouldBlockRenderThread = true; };
    };

    struct CVelocity : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        float x = 0.0f;

        DEF_PROPERTY();
        float y = 0.0f;

        DEF_PROPERTY();
        float z = 0.0f;

        CVelocity() : AComponent() {};
        CVelocity(const CVelocity &other){};
    };

    struct CColor : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        Color col = WHITE;

        CColor(){ _shouldBlockRenderThread = true; };
        CColor(const CColor &other){ _shouldBlockRenderThread = true; };
    };

    struct CCamera : public AComponent
    {
        DEF_CLASS();

        DEF_PROPERTY();
        float Zoom = 1.0f;

        CCamera(){ _shouldBlockRenderThread = true; };
        CCamera(const CCamera &other){ _shouldBlockRenderThread = true; };
    };

    struct SRenderer : public ASystem
    {
        SRenderer()
        {
            IsRenderSystem = true;
        }

        virtual void Process(AWorld *world) override;
    };
}

#endif
