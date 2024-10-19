project(box2d)

set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
include(FetchContent)

FetchContent_Declare(
  box2d
  GIT_REPOSITORY "https://github.com/erincatto/box2d"
  GIT_TAG        "v3.0.0"
)
FetchContent_GetProperties(box2d)
if (NOT box2d_POPULATED) # Have we downloaded yet?
  set(FETCHCONTENT_QUIET NO)
  FetchContent_Populate(box2d)
endif()

include(GNUInstallDirs)

file(GLOB_RECURSE box2d_SRC
	${box2d_SOURCE_DIR}/include/*.h
	${box2d_SOURCE_DIR}/src/*.cpp
	${box2d_SOURCE_DIR}/src/*.c
	${box2d_SOURCE_DIR}/extern/glad/src/*.h
	${box2d_SOURCE_DIR}/extern/glad/src/*.c
	${box2d_SOURCE_DIR}/extern/jsmn/*.h
	${box2d_SOURCE_DIR}/extern/jsmn/*.c
	${box2d_SOURCE_DIR}/extern/simde/*.h
	${box2d_SOURCE_DIR}/extern/simde/x86/*.h
	${box2d_SOURCE_DIR}/extern/simde/*.c
)

include_directories(${box2d_SOURCE_DIR}/include)
include_directories(${box2d_SOURCE_DIR}/src)
include_directories(${box2d_SOURCE_DIR}/extern/glad/include)
include_directories(${box2d_SOURCE_DIR}/extern/simde)
include_directories(${box2d_SOURCE_DIR}/extern/jsmn)

add_library(box2d SHARED ${box2d_SRC})
