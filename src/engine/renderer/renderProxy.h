#ifndef RENDER_PROXY_H
#define RENDER_PROXY_H

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

    struct ARenderProxy2D
    {
        // temp
        typedef unsigned int GLuint;

        // data for rendering
        Vector2 position;
        Color color;
        GLuint textureIndex;
        float rotation = 0.0f;
        float zoom = 1.0f;
        Vector2 pivot = { 0.5f, 0.5f };
        uint8_t colorOverrideFactor = 0;
        // end data for rendering

        // housekeeping
        size_t _uid;
        Texture2D* _textureResourceAddress = nullptr;

        ARenderProxy2D() = default;

        ARenderProxy2D(Vector2 pos, Color col, GLuint texIdx, float rot, float zm, Vector2 piv, uint8_t colorOverride) :
            position(pos),
            color(col),
            textureIndex(texIdx),
            rotation(rot),
            zoom(zm),
            pivot(piv),
            colorOverrideFactor(colorOverride)
        {
        }

        ARenderProxy2D(const ARenderProxy2D& other) :
            position(other.position),
            color(other.color),
            textureIndex(other.textureIndex),
            rotation(other.rotation),
            zoom(other.zoom),
            pivot(other.pivot),
            colorOverrideFactor(other.colorOverrideFactor),
            _uid(other._uid),
            _textureResourceAddress(other._textureResourceAddress)
        {
        }

        bool operator==(const ARenderProxy2D& other) const
        {
            return other._uid == _uid;
        }

        bool operator!=(const ARenderProxy2D& other) const
        {
            return other._uid != _uid;
        }
    };
} // namespace Atlantis

#endif // RENDER_PROXY_H