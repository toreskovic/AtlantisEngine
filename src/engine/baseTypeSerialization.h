#ifndef BASETYPESERIALIZATION_H
#define BASETYPESERIALIZATION_H

#include "nlohmann/json.hpp"
#include "raylib.h"

using json = nlohmann::json;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, r, g, b, a);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Texture2D, id, width, height, mipmaps, format);

// custom specialization
/*NLOHMANN_JSON_NAMESPACE_BEGIN
template <>
struct adl_serializer<Color>
{
    static void to_json(json &j, const Color &col)
    {
        j = {{"Color", ColorToInt(col)}};
    }

    static void from_json(const json &j, Color &col)
    {
        uint u;
        j.at("Color").get_to<uint>(u);
        col = GetColor(u);
    }
};
NLOHMANN_JSON_NAMESPACE_END*/

#endif