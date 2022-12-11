#include "helpers.h"
#include "game.h"
#include "engine/core.h"

#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

using namespace Atlantis;

AWorld* World = nullptr;

extern "C" {

LIB_EXPORT void Init(AWorld& world)
{
    World = &world;
}

LIB_EXPORT void PreHotReload()
{
}

LIB_EXPORT void PostHotReload()
{
}

} // extern "C"