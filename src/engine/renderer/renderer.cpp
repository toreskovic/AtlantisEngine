#include "renderer.h"
#include "engine/profiling.h"
#include "engine/world.h"
#include <iostream>
#include <raylib.h>
#include "rlgl.h"
#include <raymath.h>
#include <external/glad.h>

namespace Atlantis
{

// the following is used for indirect rendering
struct EntityData
{
    Vector2 position;
    Color color;
    GLuint textureIndex;
    float rotation = 0.0f;
    float zoom = 1.0f;
    Vector2 pivot = { 0.5f, 0.5f };
};

struct TextureData
{
    GLuint textureId;
    int width;
    int height;
    int atlasX;
    int atlasY;
    int atlasWidth;
    int atlasHeight;
};

void SRenderer::Process(AWorld* world)
{
    DO_PROFILE("SRenderer::Process", DARKBLUE);

    GLuint entitySSBO = 0;
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
        if (x + ren->cellSize * Zoom < 0 || x > width ||
            y + ren->cellSize * Zoom < 0 || y > height)
        {
            continue;
        }

        ATextureResource* tex = ren->textureHandle.get<ATextureResource>();
        if (tex != nullptr)
        {
            EntityData tmpData{
                { x, y }, col->col, tex->Texture.id, 0.0f, Zoom
            };

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
                tmpData.textureIndex = textureData.size() - 1;
            }

            entityData.push_back(tmpData);
        }
    }

    EndTextureMode();

    static Shader shader = LoadShaderFromMemory(
        R"""(
#version 430
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;  // Texture coordinates

layout(location = 2) in vec2 instancePos; // Per-sprite position
layout(location = 3) in vec4 instanceColor; // Per-sprite color
layout(location = 4) in uint instanceTextureIndex; // Per-sprite texture index
layout(location = 5) in float instanceRotation; // Per-sprite rotation
layout(location = 6) in float instanceZoom; // Per-sprite zoom
layout(location = 7) in vec2 instancePivot; // Per-sprite pivot

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

layout(std430, binding = 0) buffer TextureDataBuffer
{
    TextureData textureData[];
};

out vec2 fragTexCoord;   // Pass the texture coordinates to the fragment shader
out vec4 fragColor;      // Pass the color to the fragment shader

uniform mat4 projection;

void main() {
    // Get the texture data
    uint textureId = textureData[instanceTextureIndex].textureId;
    float width = textureData[instanceTextureIndex].width;
    float height = textureData[instanceTextureIndex].height;

    float atlasX = textureData[instanceTextureIndex].atlasX;
    float atlasY = textureData[instanceTextureIndex].atlasY;
    float atlasWidth = textureData[instanceTextureIndex].atlasWidth;
    float atlasHeight = textureData[instanceTextureIndex].atlasHeight;

    // calculate position, rotation, and scale
    // use the pivot point to rotate around
    vec2 position = inPosition * vec2(width, height) * instanceZoom;
    position -= instancePivot * vec2(width, height) * instanceZoom;
    position = vec2(position.x * cos(instanceRotation) - position.y * sin(instanceRotation),
                    position.x * sin(instanceRotation) + position.y * cos(instanceRotation));
    
    position += instancePos;
    
    gl_Position = projection * vec4(position, 0.0, 1.0);

    vec2 atlasPosition = vec2(atlasX, atlasY);
    vec2 atlasSize = vec2(atlasWidth, atlasHeight);
    vec2 atlasSpaceSize = vec2(width / atlasWidth, height / atlasHeight);
    fragTexCoord = inTexCoord * atlasSpaceSize + atlasPosition / atlasSize;
    fragTexCoord.y = 1.0 - fragTexCoord.y;

    fragColor = instanceColor;
}
)""",
        R"""(
#version 430
in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 outColor;

uniform sampler2D atlasTexture;

void main() {
    outColor = texture(atlasTexture, fragTexCoord) * fragColor;
})""");

    float quadVertices[] = {
        // First triangle
        -0.5f, -0.5f, 0.0f, 0.0f, // Bottom-left
        0.5f, -0.5f, 1.0f, 0.0f, // Bottom-right
        0.5f,  0.5f, 1.0f, 1.0f, // Top-right

        // Second triangle
        -0.5f, -0.5f, 0.0f, 0.0f, // Bottom-left
        0.5f,  0.5f, 1.0f, 1.0f, // Top-right
        -0.5f,  0.5f, 0.0f, 1.0f  // Top-left
    };

    GLuint vao, vbo, instanceVbo, indirectBuffer;
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
    glBufferData(GL_ARRAY_BUFFER, entityData.size() * sizeof(EntityData), entityData.data(), GL_DYNAMIC_DRAW);

    // Position (vec2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(EntityData), (void*)offsetof(EntityData, position));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);  // One per instance

    // Color (vec4)
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(EntityData), (void*)offsetof(EntityData, color));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);  // One per instance

    // Texture id (uint)
    glVertexAttribPointer(4, 1, GL_UNSIGNED_INT, GL_FALSE, sizeof(EntityData), (void*)offsetof(EntityData, textureIndex));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);  // One per instance

    // rotation (float)
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(EntityData), (void*)offsetof(EntityData, rotation));
    glEnableVertexAttribArray(5);
    glVertexAttribDivisor(5, 1);  // One per instance

    // zoom (float)
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(EntityData), (void*)offsetof(EntityData, zoom));
    glEnableVertexAttribArray(6);
    glVertexAttribDivisor(6, 1);  // One per instance

    // pivot (vec2)
    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, sizeof(EntityData), (void*)offsetof(EntityData, pivot));
    glEnableVertexAttribArray(7);
    glVertexAttribDivisor(7, 1);  // One per instance

    // texture data ssbo
    GLuint textureSSBO;
    glGenBuffers(1, &textureSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, textureSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, textureData.size() * sizeof(TextureData), textureData.data(), RL_STREAM_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    struct DrawArraysIndirectCommand {
        GLuint count;
        GLuint instanceCount;
        GLuint first;
        GLuint baseInstance;
    };

    // Indirect draw command buffer
    DrawArraysIndirectCommand cmd = { 6, (GLuint)entityData.size(), 0, 0 }; // Drawing 6 vertices (2 triangles) per instance
    glGenBuffers(1, &indirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(cmd), &cmd, GL_STATIC_DRAW);

    GLint texLoc = GetShaderLocation(shader, "atlasTexture");
    GLint projLoc = GetShaderLocation(shader, "projection");

    Matrix projection = MatrixOrtho(0, width, height, 0, -1, 1);
    projection = MatrixTranspose(projection);

    // set up shader
    glUseProgram(shader.id);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, textureSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, textureSSBO);

    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection.m0);

    rlSetUniformSampler(texLoc, atlasTexture.texture.id);

    // draw
    glBindVertexArray(vao);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);

    // Issue the indirect draw call
    glMultiDrawArraysIndirect(GL_TRIANGLE_STRIP, nullptr, 1, 0);

    // cleanup
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &instanceVbo);
    glDeleteBuffers(1, &indirectBuffer);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &textureSSBO);

    glUseProgram(0);
}
}
