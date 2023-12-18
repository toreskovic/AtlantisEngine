cmake_minimum_required(VERSION 3.11) # FetchContent is available in 3.11+

project(AtlantisEngine)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE sources      src/*.cpp src/*.h)
# we want our .dll, .lib and .exe to be in the root build folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Dependencies
include(FetchContent)

# raylib
find_package(raylib 5.0 QUIET) # QUIET or REQUIRED
if (NOT raylib_FOUND) # If there's none, fetch and build raylib
  FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
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

# raygui
FetchContent_Declare(
    raygui
    GIT_REPOSITORY "https://github.com/raysan5/raygui"
    GIT_TAG        "4.0"
)

FetchContent_MakeAvailable(raygui)

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


include(luajit-lib.cmake)

# Our Project
include_directories(src)

add_executable(${PROJECT_NAME} ${sources})
if (WIN32)
# ENABLE_EXPORTS on windows for AtlantisEngine to create a .lib file
    set_target_properties(${PROJECT_NAME} PROPERTIES ENABLE_EXPORTS ON)
# export all symbols for windows
    set_target_properties(${PROJECT_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
# set subsystem to windows for windows
    #target_link_options(${PROJECT_NAME} PRIVATE "/SUBSYSTEM:WINDOWS")
# set startup project for visual studio
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set_property(TARGET AtlantisEngine PROPERTY ENABLE_EXPORTS ON)
##target_compile_options(${PROJECT_NAME} PUBLIC -fPIC -rdynamic)

#set(raylib_VERBOSE 1)
set_property(TARGET raylib PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${PROJECT_NAME} raylib)

target_include_directories(${PROJECT_NAME} PRIVATE ${raygui_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME} dylib)

#set_property(TARGET nlohmann_json PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${PROJECT_NAME} nlohmann_json::nlohmann_json)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build raylib as a shared library" FORCE)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt"
    GIT_TAG        "9.1.0"
)

FetchContent_MakeAvailable(fmt)

set_property(TARGET fmt PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(${PROJECT_NAME} fmt)

target_link_libraries(${PROJECT_NAME} lua::lualib)

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} OpenMP::OpenMP_CXX)
endif()

IF (WIN32)
  # set stuff for windows
ELSE()
    target_compile_options(${PROJECT_NAME} PRIVATE -Werror -Wno-invalid-offsetof -Wno-enum-compare -Wno-unused-result)
ENDIF()

# Web Configurations
#if (${PLATFORM} STREQUAL "Web")
    # Tell Emscripten to build an example.html file.
#    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
#endif()

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()

# find all projects
file( GLOB GAME_PROJECTS LIST_DIRECTORIES true RELATIVE ${PROJECT_SOURCE_DIR}/projects/ projects/*)
message("found projects:")
foreach( atlantis_project_name ${GAME_PROJECTS} )
    message("${atlantis_project_name}")
endforeach( atlantis_project_name ${GAME_PROJECTS} )

# create custom commands for parsing headers
set(custom_commands)
foreach( atlantis_project_name ${GAME_PROJECTS} )
    list(APPEND custom_commands
        COMMAND python ${PROJECT_SOURCE_DIR}/header_parser.py ./src/entry.cpp ${PROJECT_SOURCE_DIR}/projects/${atlantis_project_name}
        COMMAND python ${PROJECT_SOURCE_DIR}/header_parser.py ./src/game.cpp ${PROJECT_SOURCE_DIR}/projects/${atlantis_project_name}
    )
endforeach( atlantis_project_name ${GAME_PROJECTS} )

# parse headers for all projects
add_custom_target(ParseHeaders
    COMMAND python ${PROJECT_SOURCE_DIR}/header_parser.py ./src/main.cpp ${PROJECT_SOURCE_DIR}
    ${custom_commands}
)

add_dependencies(AtlantisEngine ParseHeaders)

# add all projects
foreach( atlantis_project_name ${GAME_PROJECTS} )
    add_custom_target(CreateProjectFile${atlantis_project_name}
        COMMAND python ${PROJECT_SOURCE_DIR}/set_active_project.py ${atlantis_project_name}
    )

    add_subdirectory("projects/${atlantis_project_name}")
    add_dependencies(${atlantis_project_name} AtlantisEngine)
    add_dependencies(${atlantis_project_name} CreateProjectFile${atlantis_project_name})
endforeach( atlantis_project_name ${GAME_PROJECTS} )

##target_compile_options(AtlantisBunnyMark PUBLIC -fPIC -rdynamic)

# temp to get rid of warnings
#add_definitions(-w)

add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
                  COMMAND ${CMAKE_COMMAND} -E copy
                    ${CMAKE_SOURCE_DIR}/project.aeng $<TARGET_FILE_DIR:${PROJECT_NAME}>/project.aeng)
