#ifndef RENDER_PROXY_H
#define RENDER_PROXY_H

#include <cstddef>
#include <limits>

namespace Atlantis
{
    enum class ERenderProxyType
    {
        NONE = 0,
        SPRITE = 1,
        MAX
    };

    struct ARenderProxy
    {
        ERenderProxyType Type = ERenderProxyType::NONE;
    };

    struct ARenderProxy2DHigh
    {
        Vector2 position;
        float rotation = 0.0f;
        float zoom = 1.0f;
    };

    struct ARenderProxy2DMid
    {
        typedef unsigned int GLuint;

        Color color;
        GLuint textureIndex = 0;
    };

    struct ARenderProxy2DLow
    {
        Vector2 pivot = { 0.5f, 0.5f };
        uint8_t colorOverrideFactor = 0;
    };

    struct ARenderProxy2DMeta
    {
        size_t uid = std::numeric_limits<size_t>::max();
        Texture2D* textureResourceAddress = nullptr;
    };
} // namespace Atlantis

#endif // RENDER_PROXY_H