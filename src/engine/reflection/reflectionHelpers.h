#ifndef REFLECTIONHELPERS_H
#define REFLECTIONHELPERS_H

#include <vector>
#include <functional>
#include <map>

#define __DEF_CLASS_HELPER_1(line) __DEF_CLASS_HELPER_L_##line()
#define __DEF_CLASS_HELPER_2(line) __DEF_CLASS_HELPER_1(line)
#define DEF_CLASS() __DEF_CLASS_HELPER_2(__LINE__)

#define DEF_PROPERTY()

namespace Atlantis
{
    struct HName
    {
        std::string Name = "";

        size_t Hash = 0;

        HName()
        {
        }

        HName(std::string name)
        {
            Name = name;
            Hash = std::hash<std::string>{}(name);
        }

        HName(const char *name)
        {
            Name = name;
            Hash = std::hash<std::string>{}(name);
        }

        bool operator==(const char *name) const
        {
            return Name == name;
        }

        bool operator==(const HName &other) const
        {
            return Hash == other.Hash;
        }

        bool operator<(const HName &other) const
        {
            return Hash < other.Hash;
        }

        bool IsValid()
        {
            return Hash > 0 && !Name.empty();
        }
    };

    struct PropertyData
    {
        HName Name;
        HName Type;
        size_t Offset;
    };

    struct MethodData
    {
        HName Name;
        HName ReturnType;
        std::vector<HName> ParamTypes;
        // std::function<std::any(void *)> FunctionHelper;
    };

    struct ClassData
    {
        HName Name;
        std::vector<PropertyData> Properties;
        std::vector<MethodData> Methods;
        size_t Size;

        bool IsValid()
        {
            return Name.IsValid();
        }
    };
}

#endif