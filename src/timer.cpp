#include "timer.h"
#include "raylib.h"

std::function<bool()> Timer(int time)
{
    double currentTime = GetTime() * 1000.0f;

    std::function<bool()> fn = [time, currentTime]()
    {
        double now = GetTime() * 1000.0f;

        if (now - currentTime > time)
        {
            return true;
        }

        return false;
    };
    return fn;
}
