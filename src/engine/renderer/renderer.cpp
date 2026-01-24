#include "renderer.h"
#include "engine/profiling.h"
#include "engine/world.h"
#include <iostream>
#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>
#include <external/glad.h>

namespace Atlantis
{


// Helpers for packing
static inline float WrapAnglePi(float radians) {
    // wrap to [-PI, PI]
    // const float PI = 3.14159265358979323846f;
    const float TWO_PI = 2.0f * PI;
    radians = std::fmod(radians + PI, TWO_PI);
    if (radians < 0.0f) radians += TWO_PI;
    return radians - PI;
}
static inline int16_t PackSnorm16(float x) {
    x = std::max(-1.0f, std::min(1.0f, x));
    return (int16_t)std::lroundf(x * 32767.0f);
}
static inline uint16_t PackUnorm16(float x) {
    x = std::max(0.0f, std::min(1.0f, x));
    return (uint16_t)std::lroundf(x * 65535.0f);
}
static inline uint8_t PackUnorm8(float x) {
    x = std::max(0.0f, std::min(1.0f, x));
    return (uint8_t)std::lroundf(x * 255.0f);
}

// the following is used for indirect rendering
struct EntityData
{
    Vector2 position;
    Color color;
    GLuint textureIndex;
    float rotation = 0.0f;
    float zoom = 1.0f;
    Vector2 pivot = { 0.5f, 0.5f };
    uint8_t colorOverrideFactor = 0;

    EntityData() = default;

    EntityData(Vector2 pos, Color col, GLuint texIdx, float rot, float zm, Vector2 piv, uint8_t colorOverride) :
        position(pos),
        color(col),
        textureIndex(texIdx),
        rotation(rot),
        zoom(zm),
        pivot(piv),
        colorOverrideFactor(colorOverride)
    {
    }

    EntityData(const EntityData& other) :
        position(other.position),
        color(other.color),
        textureIndex(other.textureIndex),
        rotation(other.rotation),
        zoom(other.zoom),
        pivot(other.pivot),
        colorOverrideFactor(other.colorOverrideFactor)
    {
    }

    EntityData(EntityData&& other) :
        position(other.position),
        color(other.color),
        textureIndex(other.textureIndex),
        rotation(other.rotation),
        zoom(other.zoom),
        pivot(other.pivot),
        colorOverrideFactor(other.colorOverrideFactor)
    {
    }

    // optimized for packing
    // Vector2 position;
    // Color color;
    // GLushort textureIndex;
    // int16_t rotation;
    // uint16_t zoom;
    // uint8_t pivot[2];
    // uint8_t colorOverrideFactor;

    // EntityData() = default;

    // EntityData(Vector2 pos, Color col, GLuint texIdx, float rot, float zm, Vector2 piv)
    // {
    //     position = pos;
    //     color = col;
    //     textureIndex = (GLushort)texIdx;
    //     rotation = PackSnorm16(WrapAnglePi(rot * DEG2RAD) / PI);
    //     zoom = PackSnorm16(zm / 16.0f);
    //     pivot[0] = PackUnorm8(piv.x);
    //     pivot[1] = PackUnorm8(piv.y);
    //     colorOverrideFactor = 0;
    // }
};

void RenderEntitiesInternal(const RenderTexture2D& atlasTexture, 
                             const std::vector<EntityData>& entityData,
                             const std::vector<TextureData>& textureData)
{
    static Shader shader = LoadShaderFromMemory(
        R"""(
#version 430
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;  // Texture coordinates

layout(location = 2) in vec2 instancePos; // Per-sprite position
layout(location = 3) in vec4 instanceColor; // Per-sprite color
layout(location = 4) in uint instanceTextureIndex; // Per-sprite texture index
layout(location = 5) in float rotNorm; // Per-sprite rotation
layout(location = 6) in float zoomNorm; // Per-sprite zoom
layout(location = 7) in vec2 instancePivot; // Per-sprite pivot
layout(location = 8) in float instanceColorOverrideFactor; // Per-sprite color override factor

struct TextureData
{
    uint textureId;
    int width;
    int height;
    int atlasX;
    int atlasY;
    int atlasWidth;
    int atlasHeight;
};

layout(std430, binding = 0) readonly buffer TextureDataBuffer
{
    TextureData texData[];
};

out vec2 fragTexCoord;   // Pass the texture coordinates to the fragment shader
out vec4 fragColor;      // Pass the color to the fragment shader
out float fragColorOverrideFactor; // Pass the color override factor to the fragment shader

uniform mat4 projection;

void main() {
    // Get the texture data
    uint textureId = texData[instanceTextureIndex].textureId;
    float width = texData[instanceTextureIndex].width;
    float height = texData[instanceTextureIndex].height;

    float atlasX = texData[instanceTextureIndex].atlasX;
    float atlasY = texData[instanceTextureIndex].atlasY;
    float atlasWidth = texData[instanceTextureIndex].atlasWidth;
    float atlasHeight = texData[instanceTextureIndex].atlasHeight;

    // Decode packed attributes
    // float instanceRotation = rotNorm * 3.14159265358979323846; // [-PI, PI]
    // float instanceZoom = zoomNorm * 16.0;
    float instanceRotation = rotNorm;
    float instanceZoom = zoomNorm;

    // calculate position, rotation, and scale
    // use the pivot point to rotate around
    vec2 position = inPosition * vec2(width, height) * instanceZoom;
    position -= instancePivot * vec2(width, height) * instanceZoom;
    position = vec2(position.x * cos(instanceRotation * 3.1415 / 180.0) - position.y * sin(instanceRotation * 3.1415 / 180.0),
                    position.x * sin(instanceRotation * 3.1415 / 180.0) + position.y * cos(instanceRotation * 3.1415 / 180.0));
    
    position += instancePos;
    
    gl_Position = projection * vec4(position, 0.0, 1.0);

    vec2 atlasPosition = vec2(atlasX, atlasY);
    vec2 atlasSize = vec2(atlasWidth, atlasHeight);
    vec2 atlasSpaceSize = vec2(width / atlasWidth, height / atlasHeight);
    fragTexCoord = inTexCoord * atlasSpaceSize + atlasPosition / atlasSize;
    fragTexCoord.y = 1.0 - fragTexCoord.y;

    fragColor = instanceColor;
    fragColorOverrideFactor = instanceColorOverrideFactor;
}
)""",
        R"""(
#version 430
in vec2 fragTexCoord;
in vec4 fragColor;
in float fragColorOverrideFactor;

out vec4 outColor;

uniform sampler2D atlasTexture;

void main() {
    vec4 textureColor = texture(atlasTexture, fragTexCoord);
    outColor = mix(textureColor * fragColor, fragColor * textureColor.a, fragColorOverrideFactor);
})""");

    static const float quadVertices[] = {
        // First triangle
        0.0f, 0.0f, 0.0f, 0.0f, // Bottom-left
        1.0f, 0.0f, 1.0f, 0.0f, // Bottom-right
        1.0f,  1.0f, 1.0f, 1.0f, // Top-right

        // Second triangle
        0.0f, 0.0f, 0.0f, 0.0f, // Bottom-left
        1.0f,  1.0f, 1.0f, 1.0f, // Top-right
        0.0f,  1.0f, 0.0f, 1.0f  // Top-left
    };

    struct DrawArraysIndirectCommand {
        GLuint count;
        GLuint instanceCount;
        GLuint first;
        GLuint baseInstance;
    };

    static GLuint vao = 0, vbo, instanceVbo, indirectBuffer, textureSSBO;
    if (vao == 0)
    {
        // Vertex Array and Vertex Buffer for unit quad
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        // Unit quad vertex buffer
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);

        // Vertex positions (vec2)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(0));
        glEnableVertexAttribArray(0);

        // Texture coordinates (vec2)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Instance buffer (for position, scale, rotation)
        glGenBuffers(1, &instanceVbo);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
        // glBufferData(GL_ARRAY_BUFFER, entityData.size() * sizeof(EntityData), entityData.data(), GL_DYNAMIC_DRAW);

        glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_STREAM_DRAW);
        // glBufferData(GL_ARRAY_BUFFER, entityData.size() * sizeof(EntityData), nullptr, GL_STREAM_DRAW);
        // glBufferSubData(GL_ARRAY_BUFFER, 0, entityData.size() * sizeof(EntityData), entityData.data());

        // Position (vec2)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(EntityData), (void*)offsetof(EntityData, position));
        glEnableVertexAttribArray(2);
        glVertexAttribDivisor(2, 1);  // One per instance

        // Color (vec4)
        glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(EntityData), (void*)offsetof(EntityData, color));
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(3, 1);  // One per instance

        // Texture id (uint)
        glVertexAttribIPointer(4, 1, GL_UNSIGNED_SHORT, sizeof(EntityData), (void*)offsetof(EntityData, textureIndex));
        glEnableVertexAttribArray(4);
        glVertexAttribDivisor(4, 1);  // One per instance

        // rotation (float)
        glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(EntityData), (void*)offsetof(EntityData, rotation));
        // glVertexAttribPointer(5, 1, GL_SHORT, GL_TRUE, sizeof(EntityData), (void*)offsetof(EntityData, rotation));
        glEnableVertexAttribArray(5);
        glVertexAttribDivisor(5, 1);  // One per instance

        // zoom (float)
        glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(EntityData), (void*)offsetof(EntityData, zoom));
        // glVertexAttribPointer(6, 1, GL_SHORT, GL_TRUE, sizeof(EntityData), (void*)offsetof(EntityData, zoom));
        glEnableVertexAttribArray(6);
        glVertexAttribDivisor(6, 1);  // One per instance

        // pivot (vec2)
        glVertexAttribPointer(7, 2, GL_FLOAT, GL_TRUE, sizeof(EntityData), (void*)offsetof(EntityData, pivot));
        // glVertexAttribPointer(7, 2, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(EntityData), (void*)offsetof(EntityData, pivot));
        glEnableVertexAttribArray(7);
        glVertexAttribDivisor(7, 1);  // One per instance

        // color override factor (float)
        glVertexAttribPointer(8, 1, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(EntityData), (void*)offsetof(EntityData, colorOverrideFactor));
        glEnableVertexAttribArray(8);
        glVertexAttribDivisor(8, 1);  // One per instance

        // Indirect draw command buffer
        DrawArraysIndirectCommand cmd = { 6, 0, 0, 0 }; // Drawing 6 vertices (2 triangles) per instance
        glGenBuffers(1, &indirectBuffer);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(cmd), &cmd, GL_STATIC_DRAW);

        // texture data ssbo
        glGenBuffers(1, &textureSSBO);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, textureSSBO);
        // glBufferData(GL_SHADER_STORAGE_BUFFER, textureData.size() * sizeof(TextureData), textureData.data(), RL_STREAM_COPY);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 1, nullptr, GL_STREAM_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    glBindVertexArray(vao);
    
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
    GLsizeiptr instanceBufferSize = entityData.size() * sizeof(EntityData);
    glBufferData(GL_ARRAY_BUFFER, instanceBufferSize, nullptr, GL_STREAM_DRAW); // orphan
    if (instanceBufferSize) glBufferSubData(GL_ARRAY_BUFFER, 0, instanceBufferSize, entityData.data());

    // Orphan + upload texture SSBO (cold-ish, but still easy)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, textureSSBO);
    GLsizeiptr texSize = (GLsizeiptr)(textureData.size() * sizeof(TextureData));
    glBufferData(GL_SHADER_STORAGE_BUFFER, texSize, nullptr, GL_STREAM_DRAW); // orphan
    if (texSize) glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, texSize, textureData.data());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, textureSSBO);

    // glCreateBuffers(1, &textureSSBO);
    // glNamedBufferStorage(textureSSBO, 100 * sizeof(TextureData), textureData.data(), GL_DYNAMIC_STORAGE_BIT);

    // Indirect draw command buffer
    // DrawArraysIndirectCommand cmd = { 6, (GLuint)entityData.size(), 0, 0 }; // Drawing 6 vertices (2 triangles) per instance
    // glGenBuffers(1, &indirectBuffer);
    // glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
    // glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(cmd), &cmd, GL_STATIC_DRAW);

    // Update indirect with instance count
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
    DrawArraysIndirectCommand* cmdPtr = (DrawArraysIndirectCommand*)glMapBufferRange(
        GL_DRAW_INDIRECT_BUFFER, 0, sizeof(DrawArraysIndirectCommand),
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
    if (cmdPtr) {
        cmdPtr->count = 6; // 6 verts for strip
        cmdPtr->instanceCount = (GLuint)entityData.size();
        cmdPtr->first = 0;
        cmdPtr->baseInstance = 0;
        glUnmapBuffer(GL_DRAW_INDIRECT_BUFFER);
    }

    GLint texLoc = GetShaderLocation(shader, "atlasTexture");
    GLint projLoc = GetShaderLocation(shader, "projection");

    int width = GetScreenWidth();
    int height = GetScreenHeight();
    Matrix projection = MatrixOrtho(0, width, height, 0, -1, 1);
    projection = MatrixTranspose(projection);

    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, textureSSBO);

    // set up shader
    glUseProgram(shader.id);

    // re-bind the atlas texture
    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D, atlasTexture.texture.id);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, atlasTexture.texture.id);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, textureSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, textureSSBO);

    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection.m0);

    rlSetUniformSampler(texLoc, atlasTexture.texture.id);

    // draw
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);

    // Issue the indirect draw call
    glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, 1, 0);

    // cleanup
    // glDeleteBuffers(1, &vbo);
    // glDeleteBuffers(1, &instanceVbo);
    // glDeleteBuffers(1, &indirectBuffer);
    // glDeleteVertexArrays(1, &vao);
    // glDeleteBuffers(1, &textureSSBO);

    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void SRenderer::RenderAllEntities(AWorld* world)
{
    static std::vector<EntityData> entityData;
    static std::vector<TextureData> textureData;

    static RenderTexture2D atlasTexture = LoadRenderTexture(16384, 16384);

    // clear the atlas texture
    BeginTextureMode(atlasTexture);
    ClearBackground(BLANK);

    // track atlas data
    int nextAtlasX = 0;
    int nextAtlasY = 0;
    int atlasWidth = atlasTexture.texture.width;
    int atlasHeight = atlasTexture.texture.height;

    int nextAtlasRowY = 0;

    auto commitToAtlas = [&] (Texture2D texture)
    {
        if (nextAtlasX + texture.width > atlasWidth)
        {
            nextAtlasX = 0;
            nextAtlasY = nextAtlasRowY;
        }
        
        DrawTextureEx(texture, { (float)nextAtlasX, (float)nextAtlasY }, 0.0f, 1.0f, WHITE);

        nextAtlasRowY = std::max(nextAtlasRowY, nextAtlasY + texture.height);
    };

    // get camera
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
        CCamera* cam = cameras[0]->GetComponentOfType<CCamera>();
        CPosition* pos = cameras[0]->GetComponentOfType<CPosition>();
        Zoom = cam->Zoom;
        camX = pos->x;
        camY = pos->y;
    }

    static const ComponentBitset componentMask =
        world->GetComponentMaskForComponents(
            { "CRenderable", "CPosition", "CColor" });

    static std::vector<size_t> entitiesIds;

    static const AName entityName = "AEntity";
    const size_t entityCount = world->GetObjectCountByType(entityName);
    const AEntity* rawEntities = (const AEntity*)world->GetObjectsByNameRaw(entityName);

    entityData.clear();
    textureData.clear();
    entityData.reserve(entityCount);

    if (entitiesIds.size() < entityCount)
    {
        for (int i = entitiesIds.size(); i < entityCount; i++)
        {
            entitiesIds.push_back(rawEntities[i]._uid);
        }
    }

    for (size_t entityId = 0; entityId < entitiesIds.size(); entityId++)
    {
        AEntity* e = (AEntity*)&rawEntities[entityId];

        if (!e->_isAlive || !e->HasComponentsByMask(componentMask))
        {
            continue;
        }

        CRenderable* ren = e->GetComponentOfType<CRenderable>();
        CPosition* pos = e->GetComponentOfType<CPosition>();
        CColor* col = e->GetComponentOfType<CColor>();

        // scale using zoom
        auto x = (pos->x - halfWidth) * Zoom + halfWidth - camX * Zoom;
        auto y = (pos->y - halfHeight) * Zoom + halfHeight - camY * Zoom;

        // don't draw if outside of screen
        // this should be handled automagically by opengl
        // if (x + ren->cellSize * Zoom < 0 || x > width ||
        //     y + ren->cellSize * Zoom < 0 || y > height)
        // {
        //     continue;
        // }

        ATextureResource* tex = ren->textureHandle.get<ATextureResource>();
        if (tex != nullptr)
        {
            // EntityData tmpData{
            //     { x, y }, col->col, tex->Texture.id, ren->rotation, Zoom, { ren->pivotX, ren->pivotY }
            // };

            bool foundTexture = false;
            GLuint textureIndex = tex->Texture.id;
            for (size_t texIdx = 0; texIdx < textureData.size(); texIdx++)
            {
                if (textureData[texIdx].textureId == tex->Texture.id)
                {
                    foundTexture = true;
                    textureIndex = texIdx;
                    break;
                }
            }

            if (!foundTexture)
            {
                TextureData tmpTextureData{ tex->Texture.id,
                                            tex->Texture.width,
                                            tex->Texture.height };

                commitToAtlas(tex->Texture);
                tmpTextureData.atlasX = nextAtlasX;
                tmpTextureData.atlasY = nextAtlasY;
                tmpTextureData.atlasWidth = atlasWidth;
                tmpTextureData.atlasHeight = atlasHeight;

                nextAtlasX += tex->Texture.width;

                textureData.push_back(tmpTextureData);
                textureIndex = textureData.size() - 1;
            }

            // entityData.push_back(tmpData);
            entityData.emplace_back(
                Vector2{ x, y },
                col->col,
                textureIndex,
                ren->rotation,
                Zoom,
                Vector2{ ren->pivotX, ren->pivotY },
                0
            );
        }
    }

    EndTextureMode();

    world->QueueRenderThreadCallAsync([world]()
    {
        DO_PROFILE("SRenderer::RenderEntitiesInternal", DARKBLUE);
        RenderEntitiesInternal(atlasTexture, entityData, textureData);
    });
    // RenderEntitiesInternal(atlasTexture, entityData, textureData);
}

void SRenderer::PrepareAtlasTexture(AWorld* world, RenderTexture2D& atlasTexture, std::vector<TextureData>& textureData)
{
    if (atlasTexture.id == 0)
    {
        atlasTexture = LoadRenderTexture(16384, 16384);
    }
    // static RenderTexture2D atlasTexture = LoadRenderTexture(16384, 16384);

    // clear the atlas texture
    BeginTextureMode(atlasTexture);
    ClearBackground(BLANK);

    // track atlas data
    int nextAtlasX = 0;
    int nextAtlasY = 0;
    int atlasWidth = atlasTexture.texture.width;
    int atlasHeight = atlasTexture.texture.height;

    int nextAtlasRowY = 0;

    auto commitToAtlas = [&] (Texture2D texture)
    {
        if (nextAtlasX + texture.width > atlasWidth)
        {
            nextAtlasX = 0;
            nextAtlasY = nextAtlasRowY;
        }
        
        DrawTextureEx(texture, { (float)nextAtlasX, (float)nextAtlasY }, 0.0f, 1.0f, WHITE);

        nextAtlasRowY = std::max(nextAtlasRowY, nextAtlasY + texture.height);
    };

    // get camera
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
        CCamera* cam = cameras[0]->GetComponentOfType<CCamera>();
        CPosition* pos = cameras[0]->GetComponentOfType<CPosition>();
        Zoom = cam->Zoom;
        camX = pos->x;
        camY = pos->y;
    }

    static const ComponentBitset componentMask =
        world->GetComponentMaskForComponents(
            { "CRenderable", "CPosition", "CColor" });

    textureData.clear();

    static const AName entityName = "AEntity";
    const size_t entityCount = world->GetObjectCountByType(entityName);
    const AEntity* rawEntities = (const AEntity*)world->GetObjectsByNameRaw(entityName);

    static std::vector<size_t> entitiesIds;

    if (entitiesIds.size() < entityCount)
    {
        for (int i = entitiesIds.size(); i < entityCount; i++)
        {
            entitiesIds.push_back(rawEntities[i]._uid);
        }
    }

    for (size_t entityId = 0; entityId < entitiesIds.size(); entityId++)
    {
        AEntity* e = (AEntity*)&rawEntities[entityId];

        if (!e->_isAlive || !e->HasComponentsByMask(componentMask))
        {
            continue;
        }

        CRenderable* ren = e->GetComponentOfType<CRenderable>();
        CPosition* pos = e->GetComponentOfType<CPosition>();
        CColor* col = e->GetComponentOfType<CColor>();

        // scale using zoom
        auto x = (pos->x - halfWidth) * Zoom + halfWidth - camX * Zoom;
        auto y = (pos->y - halfHeight) * Zoom + halfHeight - camY * Zoom;

        ATextureResource* tex = ren->textureHandle.get<ATextureResource>();
        if (tex != nullptr)
        {
            bool foundTexture = false;
            for (size_t texIdx = 0; texIdx < textureData.size(); texIdx++)
            {
                if (textureData[texIdx].textureId == tex->Texture.id)
                {
                    foundTexture = true;
                    break;
                }
            }

            if (!foundTexture)
            {
                TextureData tmpTextureData{ tex->Texture.id,
                                            tex->Texture.width,
                                            tex->Texture.height };

                commitToAtlas(tex->Texture);
                tmpTextureData.atlasX = nextAtlasX;
                tmpTextureData.atlasY = nextAtlasY;
                tmpTextureData.atlasWidth = atlasWidth;
                tmpTextureData.atlasHeight = atlasHeight;

                nextAtlasX += tex->Texture.width;

                textureData.push_back(tmpTextureData);
            }
        }
    }

    EndTextureMode();
}

bool SRenderer::RenderEntities(AWorld* world, RenderTexture2D& atlasTexture, std::vector<TextureData> textureData, std::vector<size_t> entitiesIds, bool overrideColor, Color color)
{
    static std::vector<EntityData> entityData;

    // get camera
    float Zoom = 1.0f;
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    int halfWidth = width / 2;
    int halfHeight = height / 2;

    int camX = 0;
    int camY = 0;

    bool needsAtlasRefresh = false;

    auto& cameras = world->GetEntitiesWithComponents<CCamera, CPosition>();
    if (cameras.size() > 0)
    {
        CCamera* cam = cameras[0]->GetComponentOfType<CCamera>();
        CPosition* pos = cameras[0]->GetComponentOfType<CPosition>();
        Zoom = cam->Zoom;
        camX = pos->x;
        camY = pos->y;
    }

    static const ComponentBitset componentMask =
        world->GetComponentMaskForComponents(
            { "CRenderable", "CPosition", "CColor" });

    entityData.clear();
    entityData.reserve(entitiesIds.size());

    static const AName entityName = "AEntity";
    const size_t entityCount = world->GetObjectCountByType(entityName);
    const AEntity* rawEntities = (const AEntity*)world->GetObjectsByNameRaw(entityName);

    for (size_t entityId : entitiesIds)
    {
        AEntity* e = (AEntity*)&rawEntities[entityId];

        if (!e->_isAlive || !e->HasComponentsByMask(componentMask))
        {
            continue;
        }

        CRenderable* ren = e->GetComponentOfType<CRenderable>();
        CPosition* pos = e->GetComponentOfType<CPosition>();
        CColor* col = e->GetComponentOfType<CColor>();

        // scale using zoom
        auto x = (pos->x - halfWidth) * Zoom + halfWidth - camX * Zoom;
        auto y = (pos->y - halfHeight) * Zoom + halfHeight - camY * Zoom;

        ATextureResource* tex = ren->textureHandle.get<ATextureResource>();
        if (tex != nullptr)
        {
            EntityData tmpData{
                { x, y }, col->col, tex->Texture.id, ren->rotation, Zoom, { ren->pivotX, ren->pivotY }, 0
            };

            if (overrideColor)
            {
                // tmpData.color = color;
                tmpData.colorOverrideFactor = 1;
            }

            bool foundTexture = false;
            for (size_t texIdx = 0; texIdx < textureData.size(); texIdx++)
            {
                if (textureData[texIdx].textureId == tex->Texture.id)
                {
                    foundTexture = true;
                    tmpData.textureIndex = texIdx;
                    break;
                }
            }

            if (foundTexture)
            {
                entityData.push_back(tmpData);
            }
            else
            {
                needsAtlasRefresh = true;
            }
        }
    }


    // world->QueueRenderThreadCallAsync([atlasTexture, textureData]()
    // {
        RenderEntitiesInternal(atlasTexture, entityData, textureData);
    // });
    // RenderEntitiesInternal(atlasTexture, entityData, textureData);
    return !needsAtlasRefresh;
}

void SRenderer::Process(AWorld* world)
{
    DO_PROFILE("SRenderer::Process", DARKBLUE);

    ClearBackground(RAYWHITE);
    // render all entities
    SRenderer::RenderAllEntities(world);
}
}