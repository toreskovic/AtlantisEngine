#include "engine/profiling.h"
#include "engine/core.h"
#include "engine/world.h"
#include "timer.h"
#include "fmt/core.h"
#include "raylib.h"

namespace Atlantis
{
    void SSimpleProfiler::Process(AWorld *world)
    {
#if !ATLANTIS_PROFILING
        return;
#endif

        if (_world == nullptr)
        {
            _world = world;
        }

        if (world->IsMainThread())
        {
            // count entities that are alive
            auto& entities = world->GetObjectsByName("AEntity");
            _entitiesCount = 0;
            for (auto& e : entities)
            {
                if (e->_isAlive)
                {
                    _entitiesCount++;
                }
            }
        }

        if (!world->IsRenderThread())
        {
            return;
        }

        _world->ProfilingMutex.lock();
        // BeginDrawing();

        static float fps = 0.0f;

        static auto timer = Timer(100);
        static float fpsAggregator = 0.0f;
        static int fpsCounter = 0;

        fpsAggregator += 1.0f / world->GetDeltaTime();
        fpsCounter++;
        if (timer())
        {
            float currentFps = fpsAggregator / fpsCounter;
            fps = fps * 0.7f + currentFps * 0.3f;

            fpsAggregator = 0.0f;
            fpsCounter = 0;
            timer = Timer(100);
        }

        auto fpsStr = fmt::format("FPS: {:.2f}", fps);
        int fontSize = 20;
        int textSize = MeasureText(fpsStr.c_str(), fontSize);

        auto entityStr = fmt::format("Entities: {}", world->ProfilerMainThread->_entitiesCount);
        textSize = std::max(textSize, MeasureText(entityStr.c_str(), fontSize));

        Color bg = DARKGRAY;
        bg.a = 150;

        float totalDuration = 0.0f;
        float totalDurationMainThread = 0.0f;
        float totalDurationRenderThread = 0.0f;
        for (auto &profileData : debugProfileData)
        {
            totalDurationRenderThread += profileData.time;
        }

        for (auto &profileData : world->ProfilerMainThread->debugProfileData)
        {
            totalDurationMainThread += profileData.time;
        }

        totalDuration = totalDurationMainThread > totalDurationRenderThread ? totalDurationMainThread : totalDurationRenderThread;

        // draw the profile info
        int x = 0;
        int offset_tmp = 0;
        for (auto &profileData : debugProfileData)
        {
            int y = profileData.offset;
            float duration = profileData.time;
            float percent = duration / totalDuration;
            int width = (int)(percent * GetScreenWidth());
            DrawRectangle(x, y, width, 40, profileData.color);
            auto profileStr = fmt::format("{}: {:.2f}ms", profileData.name, profileData.time * 1000.0f);
            DrawText(profileStr.c_str(), x + 10, y + 10, 20, LIGHTGRAY);
            DrawRectangle(x + width - 2, y, 2, 40, BLACK);
            x += width;
            offset_tmp = std::max(offset_tmp, profileData.offset);
        }

        x = 0;
        offset_tmp += 40;

        // totalDuration = 0.0f;

        for (auto& profileData : world->ProfilerMainThread->debugProfileData)
        {
            int y = offset_tmp + profileData.offset;
            float duration = profileData.time;
            float percent = duration / totalDuration;
            int width = (int)(percent * GetScreenWidth());
            DrawRectangle(x, y, width, 40, profileData.color);
            auto profileStr = fmt::format("{}: {:.2f}ms", profileData.name, profileData.time * 1000.0f);
            DrawText(profileStr.c_str(), x + 10, y + 10, 20, LIGHTGRAY);
            DrawRectangle(x + width - 2, y, 2, 40, BLACK);
            x += width;
            offset_tmp = std::max(offset_tmp, profileData.offset);
        }

        debugProfileData.clear();
        world->ProfilerMainThread->debugProfileData.clear();

        DrawRectangle(0, offset_tmp + 40, textSize + 30, fontSize * 2 + 30, bg);
        DrawText(fpsStr.c_str(), 10, offset_tmp + 40 + 10, fontSize, LIGHTGRAY);
        DrawText(entityStr.c_str(), 10, offset_tmp + 40 + 30, fontSize, LIGHTGRAY);

        // EndDrawing();
        _world->ProfilingMutex.unlock();
    }

    void SSimpleProfiler::AddProfileData(ADebugProfileData profileData)
    {
        if (_world == nullptr)
        {
            return;
        }

        debugProfileData.push_back(profileData);
    }

    void SSimpleProfiler::SetOffset(float offset)
    {
        Offset = offset;
        MaxOffset = std::max(MaxOffset, offset);
    }

    float SSimpleProfiler::GetOffset() const
    {
        return Offset;
    }

    ADebugProfileHelper::ADebugProfileHelper(std::string name, Color col, AWorld* world)
    {
        if (world == nullptr)
        {
            return;
        }

        SSimpleProfiler* profiler = nullptr;
        if (world->IsGameThread())
        {
            profiler = world->ProfilerMainThread;
        }
        else
        {
            profiler = world->ProfilerRenderThread;
        }

        if (profiler == nullptr)
        {
            return;
        }

        this->profiler = profiler;
        this->name = name;
        this->color = col;
        this->start = std::chrono::high_resolution_clock::now();
        this->_world = world;

        _world->ProfilingMutex.lock();

        offset = profiler->GetOffset();

        auto it = std::find_if(profiler->debugProfileData.begin(), profiler->debugProfileData.end(), [name](const ADebugProfileData &data)
                               { return data.name == name; });
        if (it == profiler->debugProfileData.end())
        {
            profiler->SetOffset(profiler->GetOffset() + 40);
        }
    }

    ADebugProfileHelper::~ADebugProfileHelper()
    {
        if (profiler == nullptr)
        {
            return;
        }

        if (_world == nullptr)
        {
            return;
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> duration = end - start;

        auto it = std::find_if(profiler->debugProfileData.begin(), profiler->debugProfileData.end(), [this](const ADebugProfileData &data)
                               { return data.name == name; });
        if (it != profiler->debugProfileData.end())
        {
            it->time += duration.count();
        }
        else
        {
            profiler->AddProfileData({name, duration.count(), color, offset});
            profiler->SetOffset(profiler->GetOffset() - 40);
        }

        _world->ProfilingMutex.unlock();
    }

} // namespace Atlantis