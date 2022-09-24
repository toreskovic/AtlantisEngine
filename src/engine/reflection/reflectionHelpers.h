#ifndef REFLECTIONHELPERS_H
#define REFLECTIONHELPERS_H

#include <vector>
#include <functional>
#include <map>
#include "nlohmann/json.hpp"

#define __DEF_CLASS_HELPER_1(line) __DEF_CLASS_HELPER_L_##line()
#define __DEF_CLASS_HELPER_2(line) __DEF_CLASS_HELPER_1(line)
#define DEF_CLASS() __DEF_CLASS_HELPER_2(__LINE__)

#define DEF_PROPERTY()

namespace Atlantis
{
    struct HName
    {
        std::vector<char> Name;

        size_t Hash = 0;

        HName()
        {
        }

        HName(std::string name)
        {
            Name = { name.begin(), name.end() };

            if (Name[Name.size() - 1] != 0)
            {
                Name.push_back(0);
            }

            Hash = std::hash<std::string>{}(name);
        }

        HName(const char *name)
        {
            std::string n = name;
            Name = { n.begin(), n.end() };

            if (Name[Name.size() - 1] != 0)
            {
                Name.push_back(0);
            }

            Hash = std::hash<std::string>{}(name);
        }

        std::string GetName() const
        {
            std::string n = { Name.begin(), Name.end() - 1 };
            return n;
        }

        bool operator==(const char *name) const
        {
            return Hash == HName(name).Hash;
            // return Name == name;
        }

        bool operator==(const HName &other) const
        {
            return Hash == other.Hash;
        }

        bool operator<(const HName &other) const
        {
            return Hash < other.Hash;
        }

        friend std::ostream &operator<<(std::ostream &os, const HName &name)
        {
            std::string n = { name.Name.begin(), name.Name.end() };
            return std::operator<<(os, n);
        }

        bool IsValid()
        {
            // return Hash > 0 && !Name.empty();
            return Hash > 0;
        }
    };

    class HNameHashFunction
    {
    public:
        size_t operator()(const HName &p) const
        {
            return p.Hash;
        }
    };

    struct ResourceHandle
    {
        size_t Address = 0;

        template <typename T>
        T *get()
        {
            void* ptr = (void*)Address;
            return static_cast<T *>(ptr);
        }

        bool operator==(const ResourceHandle& other) const
        {
            return Address == other.Address;
        }
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ResourceHandle, Address);

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

NLOHMANN_JSON_NAMESPACE_BEGIN
template <>
struct adl_serializer<Atlantis::HName>
{
    static void to_json(json &j, const Atlantis::HName &name)
    {
        j = {{"Name", name.GetName()}, {"Hash", name.Hash}};
    }

    static void from_json(const json &j, Atlantis::HName &name)
    {
        std::string s;
        j.at("Name").get_to(s);
        
        name = Atlantis::HName(s);
    }
};
NLOHMANN_JSON_NAMESPACE_END

#endif