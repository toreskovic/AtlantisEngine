cmake_minimum_required(VERSION 3.11) # FetchContent is available in 3.11+
project(AtlantisBunnyMark)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(raylib 4.2.0 QUIET) # QUIET or REQUIRED
if (NOT raylib_FOUND) # If there's none, fetch and build raylib
  include(FetchContent)
  FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/4.2.0.tar.gz
  )
  FetchContent_GetProperties(raylib)
  if (NOT raylib_POPULATED) # Have we downloaded raylib yet?
    set(FETCHCONTENT_QUIET NO)
    FetchContent_Populate(raylib)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
    set(BUILD_SHARED_LIBS ON CACHE BOOL "Build raylib as a shared library" FORCE)
    add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})
  endif()
endif()

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY "https://github.com/nlohmann/json"
    GIT_TAG        "v3.11.2"
)

FetchContent_MakeAvailable(nlohmann_json)


FetchContent_Declare(
    fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt"
    GIT_TAG        "9.1.0"
)

FetchContent_MakeAvailable(fmt)


file(GLOB_RECURSE sources      src/*.cpp src/*.h)

add_library(${PROJECT_NAME} SHARED
    ${sources}
)
target_link_libraries(${PROJECT_NAME} nlohmann_json::nlohmann_json)

target_link_libraries(${PROJECT_NAME} raylib)

target_link_libraries(${PROJECT_NAME} fmt)

target_link_libraries(AtlantisBunnyMark AtlantisEngine)

include_directories(../../src)

# temp to get rid of warnings
add_definitions(-w)
