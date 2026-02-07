#ifndef RENDERER_H
#define RENDERER_H

#include "raylib.h"

#include "engine/reflection/reflectionHelpers.h"
#include "engine/core.h"
#include "engine/renderer/renderProxy.h"
#include "engine/system.h"
#include "./generated/renderer.gen.h"

namespace Atlantis
{
    struct ShaderParamScalar
    {
        AName name;
        float value = 0.0f;
        int shaderLocation = -1;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ShaderParamScalar, name, value);

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

        DEF_PROPERTY();
        int spriteWidth = 0;

        DEF_PROPERTY();
        int spriteHeight = 0;

        DEF_PROPERTY();
        int spriteX = 0;

        DEF_PROPERTY();
        int spriteY = 0;

        DEF_PROPERTY();
        int cellSize = 64;

        DEF_PROPERTY();
        bool flip = false;

        DEF_PROPERTY();
        float rotation = 0.0f;

        DEF_PROPERTY();
        float scaleX = 1.0f;

        DEF_PROPERTY();
        float scaleY = 1.0f;

        DEF_PROPERTY();
        float pivotX = 0.0f;

        DEF_PROPERTY();
        float pivotY = 0.0f;

        DEF_PROPERTY();
        AName shaderPath;

        DEF_PROPERTY();
        AResourceHandle shaderHandle;

        std::vector<ShaderParamScalar> shaderParamsScalar;

        CRenderable() : AComponent() { _shouldBlockRenderThread = true; };
        CRenderable(const CRenderable &other){ _shouldBlockRenderThread = true; };

        virtual void OnAddedToEntity(AEntity* entity) override;
        virtual void OnRemovedFromEntity(AEntity* entity) override;

        void OnCreated(bool firstTime = false);
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

        DEF_PROPERTY();
        int InternalWidth = 1920;

        DEF_PROPERTY();
        int InternalHeight = 1080;

        CCamera(){ _shouldBlockRenderThread = true; };
        CCamera(const CCamera &other){ _shouldBlockRenderThread = true; };
    };

    struct TextureData
    {
        // GLuint textureId;
        unsigned int textureId;
        int width;
        int height;
        int atlasX;
        int atlasY;
        int atlasWidth;
        int atlasHeight;
    };

    struct SRenderer : public ASystem
    {
        SRenderer()
        {
            IsRenderSystem = true;
        }
        
        static void RenderAllEntities(AWorld *world);
        static void PrepareAtlasTexture(AWorld* world, RenderTexture2D& atlasTexture, std::vector<TextureData>& textureData);
        static bool RenderEntities(AWorld* world, RenderTexture2D& atlasTexture, std::vector<TextureData> textureData, std::vector<size_t> entitiesIds, bool overrideColor = false, Color color = WHITE);

        virtual void Process(AWorld *world) override;
    };
}

#endif
