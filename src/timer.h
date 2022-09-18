#include <chrono>
#include <functional>

std::function<bool()> Timer(int time){
    static auto created = std::chrono::high_resolution_clock::now();
    // "=" allow to pass by copy all used variables (created and period)
    // "&f" allow to pass by reference f variable
    std::function<bool()> fn = [=](){
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - created).count() > time){
            return true;
        }
        return false;
    };
    return fn;
}