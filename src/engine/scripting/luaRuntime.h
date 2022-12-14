#include <sol/sol.hpp>
#include <iostream>
#include <filesystem>

namespace Atlantis
{
    struct ALuaRuntime
    {
        sol::state lua;

        void RunScript(const std::string &file)
        {
            if (std::filesystem::exists(file))
            {
                lua.script_file(file);
            }
            else
            {
                std::cout << "Lua file " << file << " does not exist! " << std::endl;
            }
        }

        void InitLua()
        {
            // open some common libraries
            lua.open_libraries(sol::lib::base, sol::lib::package);
        }

        void DoLua()
        {
        }

        void UnloadLua()
        {
        }
    };
}