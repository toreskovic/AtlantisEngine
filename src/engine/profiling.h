#ifndef ATLANTIS_ENGINE_PROFILING_H
#define ATLANTIS_ENGINE_PROFILING_H

#include <chrono>
#include <string>
#include <vector>
#include "raylib.h"

struct DebugProfileData
{
    std::string name;
    float time;
    Color color = PURPLE;
    float offset = 0.0f;
};

std::vector<DebugProfileData> debugProfileData;

struct DebugProfileHelper
{
    std::string name;
    Color color = RAYWHITE;
    float offset = 0.0f;
    std::chrono::high_resolution_clock::time_point start;

    DebugProfileHelper(std::string name) : name(name)
    {
        start = std::chrono::high_resolution_clock::now();
    }
    DebugProfileHelper(std::string name, Color col) : name(name), color(col)
    {
        start = std::chrono::high_resolution_clock::now();
    }
    DebugProfileHelper(std::string name, Color col, float offset) : name(name), color(col), offset(offset)
    {
        start = std::chrono::high_resolution_clock::now();
    }
    ~DebugProfileHelper()
    {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> duration = end - start;

        auto it = std::find_if(debugProfileData.begin(), debugProfileData.end(), [this](const DebugProfileData &data) { return data.name == name; });
        if (it != debugProfileData.end())
        {
            it->time += duration.count();
        }
        else
        {
            debugProfileData.push_back({name, duration.count(), color, offset});
        }
    }
};

#define DO_PROFILE(name, color) DebugProfileHelper profileHelper##__LINE__(name, color)
#define DO_PROFILE_OFFSET(name, color, offset) DebugProfileHelper profileHelper##__LINE__(name, color, offset)

#endif //ATLANTIS_ENGINE_PROFILING_H