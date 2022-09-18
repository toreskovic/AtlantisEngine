cmake_minimum_required(VERSION 3.11) # FetchContent is available in 3.11+
project(AtlantisGame)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(AtlantisGame SHARED
    src/entry.cpp
)

include_directories(../src)