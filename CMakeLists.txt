cmake_minimum_required(VERSION 3.11) # FetchContent is available in 3.11+
project(AtlantisEngine)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE sources      src/*.cpp src/*.h)

# Dependencies
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
    add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})
  endif()
endif()

include(FetchContent)

FetchContent_Declare(
    dylib
    GIT_REPOSITORY "https://github.com/martin-olivier/dylib"
    GIT_TAG        "v2.1.0"
)

FetchContent_MakeAvailable(dylib)

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

include(lua-lib.cmake)

# Our Project
include_directories(src)

add_executable(${PROJECT_NAME} ${sources})

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set_property(TARGET AtlantisEngine PROPERTY ENABLE_EXPORTS ON)
##target_compile_options(${PROJECT_NAME} PUBLIC -fPIC -rdynamic)

#set(raylib_VERBOSE 1)
set_property(TARGET raylib PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${PROJECT_NAME} raylib)

target_link_libraries(${PROJECT_NAME} dylib)

set_property(TARGET nlohmann_json PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${PROJECT_NAME} nlohmann_json::nlohmann_json)

set_property(TARGET fmt PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${PROJECT_NAME} fmt)

target_link_libraries(${PROJECT_NAME} lua::lualib)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} OpenMP::OpenMP_CXX)
endif()

target_compile_options(${PROJECT_NAME} PRIVATE -Werror -Wno-invalid-offsetof)

# Web Configurations
if (${PLATFORM} STREQUAL "Web")
    # Tell Emscripten to build an example.html file.
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
endif()

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()


add_custom_target(ParseHeaders
    COMMAND python ${PROJECT_SOURCE_DIR}/header_parser.py ./src/main.cpp ${PROJECT_SOURCE_DIR}
    COMMAND python ${PROJECT_SOURCE_DIR}/header_parser.py ./src/entry.cpp ${PROJECT_SOURCE_DIR}/projects/AtlantisGame
    COMMAND python ${PROJECT_SOURCE_DIR}/header_parser.py ./src/entry.cpp ${PROJECT_SOURCE_DIR}/projects/AtlantisBunnyMark
    COMMAND python ${PROJECT_SOURCE_DIR}/header_parser.py ./src/entry.cpp ${PROJECT_SOURCE_DIR}/projects/AtlantisBunnyMarkLua
)

add_custom_target(CreateProjectFileGame
    COMMAND python ${PROJECT_SOURCE_DIR}/set_active_project.py AtlantisGame
)

add_custom_target(CreateProjectFileBunnyMark
    COMMAND python ${PROJECT_SOURCE_DIR}/set_active_project.py AtlantisBunnyMark
)

add_custom_target(CreateProjectFileBunnyMarkLua
    COMMAND python ${PROJECT_SOURCE_DIR}/set_active_project.py AtlantisBunnyMarkLua
)

add_dependencies(AtlantisEngine ParseHeaders)

add_subdirectory("projects/AtlantisGame")
add_dependencies(AtlantisGame AtlantisEngine)
add_dependencies(AtlantisGame CreateProjectFileGame)

add_subdirectory("projects/AtlantisBunnyMark")
add_dependencies(AtlantisBunnyMark AtlantisEngine)
add_dependencies(AtlantisBunnyMark CreateProjectFileBunnyMark)

add_subdirectory("projects/AtlantisBunnyMarkLua")
add_dependencies(AtlantisBunnyMarkLua AtlantisEngine)
add_dependencies(AtlantisBunnyMarkLua CreateProjectFileBunnyMarkLua)

##target_compile_options(AtlantisBunnyMark PUBLIC -fPIC -rdynamic)

# temp to get rid of warnings
#add_definitions(-w)

add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
                  COMMAND ${CMAKE_COMMAND} -E copy
                    ${CMAKE_SOURCE_DIR}/project.aeng $<TARGET_FILE_DIR:${PROJECT_NAME}>/project.aeng)