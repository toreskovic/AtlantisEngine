#include "helpers.h"
#include "game.h"
#include "engine/core.h"

#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

using namespace Atlantis;

ARegistry* ObjRegistry = nullptr;

extern "C" {

LIB_EXPORT double pi_value = 3.14159;
LIB_EXPORT void *ptr = (void *)1;

LIB_EXPORT void print_hello() {
    std::cout << "Hello World" << std::endl;
}

LIB_EXPORT void print_helper() {
    print_test();
}

LIB_EXPORT void Init(ARegistry& registry)
{
    ObjRegistry = &registry;
}

LIB_EXPORT void PreHotReload()
{
}

LIB_EXPORT void PostHotReload()
{
}

} // extern "C"