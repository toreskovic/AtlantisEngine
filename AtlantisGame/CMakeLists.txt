cmake_minimum_required(VERSION 3.11) # FetchContent is available in 3.11+
project(AtlantisGame)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY "https://github.com/nlohmann/json"
    GIT_TAG        "v3.11.2"
)

FetchContent_MakeAvailable(nlohmann_json)

add_library(AtlantisGame SHARED
    src/entry.cpp
)
target_link_libraries(${PROJECT_NAME} nlohmann_json::nlohmann_json)

include_directories(../src)

# temp to get rid of warnings
add_definitions(-w)