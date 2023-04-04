#ifndef ATLANTIS_ENGINE_PROFILING_H
#define ATLANTIS_ENGINE_PROFILING_H

#include <chrono>
#include <string>
#include <vector>
#include "engine/system.h"
#include <mutex>

namespace Atlantis
{
    struct ADebugProfileData
    {
        std::string name;
        float time = -1.0f;
        Color color = PURPLE;
        int offset = 0;
    };

    struct SSimpleProfiler;

    struct ADebugProfileHelper
    {
        SSimpleProfiler *profiler = nullptr;
        std::string name;
        Color color = RAYWHITE;
        int offset = 0;
        std::chrono::high_resolution_clock::time_point start;

        ADebugProfileHelper(std::string name, Color col, SSimpleProfiler *profiler);

        ~ADebugProfileHelper();
    };

    struct AWorld;

    struct SSimpleProfiler : public ASystem
    {
        AWorld* _world = nullptr;

        float Offset = 0.0f;
        float MaxOffset = 0.0f;
        std::vector<ADebugProfileData> debugProfileData;

        SSimpleProfiler() { Labels.insert("SimpleProfiler"); };
        virtual void Process(AWorld *world) override;

        void AddProfileData(ADebugProfileData profileData);
        void PopProfileData();

        void SetOffset(float offset);
        float GetOffset() const;
    };


#define DO_PROFILE(name, color) ;
//#define DO_PROFILE(name, color) ADebugProfileHelper profileHelper##__LINE__(name, color, world->GetSystem<SSimpleProfiler>("SimpleProfiler"))
} // namespace Atlantis

#endif // ATLANTIS_ENGINE_PROFILING_H