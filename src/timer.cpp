#include "timer.h"
#include "raylib.h"

TimerFn Timer(int time)
{
    double currentTime = GetTime() * 1000.0f;

    TimerFn fn = [time, currentTime]()
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
