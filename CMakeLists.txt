cmake_minimum_required(VERSION 3.11) # FetchContent is available in 3.11+
project(AtlantisEngine)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)

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
# Our Project

include_directories(src)

add_executable(${PROJECT_NAME} ${sources})

#set(raylib_VERBOSE 1)
target_link_libraries(${PROJECT_NAME} raylib)

#target_link_libraries(${PROJECT_NAME} AtlantisGame)
target_link_libraries(${PROJECT_NAME} dylib)


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

add_subdirectory("AtlantisGame")

add_custom_target(ParseHeaders
    COMMAND python ${PROJECT_SOURCE_DIR}/header_parser.py ./src/main.cpp ${PROJECT_SOURCE_DIR}
    COMMAND python ${PROJECT_SOURCE_DIR}/header_parser.py ./src/entry.cpp ${PROJECT_SOURCE_DIR}/AtlantisGame
)

#add_dependencies(AtlantisGame ParseHeaders)
add_dependencies(AtlantisEngine ParseHeaders)
add_dependencies(AtlantisGame AtlantisEngine)

# temp to get rid of warnings
add_definitions(-w)