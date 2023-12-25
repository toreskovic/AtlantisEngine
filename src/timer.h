#pragma once

#include <chrono>
#include <functional>

typedef std::function<bool()> TimerFn;

TimerFn Timer(int time = 0);