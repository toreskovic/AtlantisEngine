#include "helpers.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#if defined(_WIN32)
#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) ||           \
    defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
#define _AMD64_
#elif defined(i386) || defined(__i386) || defined(__i386__) ||                 \
    defined(__i386__) || defined(_M_IX86)
#define _X86_
#elif defined(__arm__) || defined(_M_ARM) || defined(_M_ARMT)
#define _ARM_
#endif

#include <libloaderapi.h>
#include <minwindef.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif

namespace Atlantis
{
std::filesystem::path Helpers::GetExeDirectory()
{
#ifdef _WIN32
    // Windows specific
    wchar_t szPath[MAX_PATH];
    GetModuleFileNameW(NULL, szPath, MAX_PATH);
#else
    // Linux specific
    char szPath[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", szPath, PATH_MAX);
    if (count < 0 || count >= PATH_MAX)
        return {}; // some error
    szPath[count] = '\0';
#endif
    return std::filesystem::path{ szPath }.parent_path() /
           ""; // to finish the folder path with (back)slash
}

std::filesystem::path Helpers::GetProjectDirectory()
{
    std::string projectName;
    std::ifstream projectFile("./project.aeng");
    std::getline(projectFile, projectName);

    return std::filesystem::path("../projects/" + projectName + "/");
}
}