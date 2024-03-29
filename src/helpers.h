#ifndef AHELPERS_H
#define AHELPERS_H

#include <filesystem>

namespace Atlantis
{
    class Helpers
    {
    public:
        static std::filesystem::path GetExeDirectory();
        static std::filesystem::path GetProjectDirectory();
    };
}

#endif // !AHELPERS_H
