#ifndef RENDERER_H
#define RENDERER_H

#include "raylib.h"

#include "engine/reflection/reflectionHelpers.h"
#include "engine/core.h"
#include "./generated/renderer.gen.h"


struct position : public AComponent
{
    DEF_CLASS();

    DEF_PROPERTY();
    float x;
    DEF_PROPERTY();
    float y;
    DEF_PROPERTY();
    float z;

    position() {};
    position(const position& other) {};
};

struct renderable : public AComponent
{
    DEF_CLASS();

    DEF_PROPERTY();
    std::string renderType = "Rectangle";

    renderable() {};
    renderable(const renderable& other) {};
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

    velocity() {};
    velocity(const velocity& other) {};
};

struct color : public AComponent
{
    DEF_CLASS();

    DEF_PROPERTY();
    Color col;

    color() {};
    color(const color& other) {};

    // temp hack
    virtual nlohmann::json Serialize() override
    {
        nlohmann::json json;
        const auto& classData = GetClassData();

        json["Name"] = classData.Name.Name;
        json["Properties"] = nlohmann::json::array({});
        for (const auto& propData: classData.Properties)
        {
            if (propData.Type == "Color")
            {
                json["Properties"].push_back({{"Name", propData.Name.Name}, {"Type", propData.Type.Name}, {"Offset", propData.Offset}, {"Value", ColorToInt(col)}});
            }
            else
            {
                json["Properties"].push_back({{"Name", propData.Name.Name}, {"Type", propData.Type.Name}, {"Offset", propData.Offset}});
            }
        }

        return json;
    }

    // temp hack
    virtual void Deserialize(nlohmann::json& json) override
    {
        for (auto& prop: json["Properties"])
        {
            if (prop["Type"].get<std::string>() == "Color")
            {
                col = GetColor(prop["Value"].get<uint>());
            }
        }
    }
};

class Renderer
{
public:
    Renderer(Registry& registry);
    ~Renderer();

    void Render(Registry& registry);
};





#endif
