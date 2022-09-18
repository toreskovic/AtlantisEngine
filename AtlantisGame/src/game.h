#include "engine/reflection/reflectionHelpers.h"
#include "engine/core.h"
#include "generated/game.gen.h"

struct something : public AComponent
{
    DEF_CLASS();

    DEF_PROPERTY();
    int x;
};