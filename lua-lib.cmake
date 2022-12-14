include(FetchContent)

# Note: the 'add_subriectory' line was commented becuyase 
#       library that will be downloaded does not have 
#       a CMakeListst.txt file at the root directory. 
macro(Download_Library_Git  NAME TAG REPOSITORY_URL)
    FetchContent_Declare(
        ${NAME}
        GIT_REPOSITORY  ${REPOSITORY_URL}
        GIT_TAG         ${TAG}
    )
    FetchContent_GetProperties(${NAME})
    if(NOT cpputest_POPULATED)
        FetchContent_Populate(${NAME})
        message("${NAME}_SOURCE_DIR} = ${${NAME}_SOURCE_DIR}")        

        # => Disable following line: the library does not have a CMakeLists.txt
        #    at the root directory.
        # add_subdirectory(${${NAME}_SOURCE_DIR} ${${NAME}_BINARY_DIR})
    endif()
endmacro()


# ====>> Download Lua library <<==========================#

Download_Library_Git( lua                       
                      v5.3.5
                      https://github.com/lua/lua
                    )

file(GLOB_RECURSE lua_sources "${lua_SOURCE_DIR}/*.c")
file(GLOB_RECURSE lua_headers" ${lua_SOURCE_DIR}/*.h")

message( [TRACE] " lua_SOURCE_DIR = ${lua_SOURCE_DIR} ")

               add_library( lua STATIC ${lua_sources} ${lua_headers} )
target_include_directories( lua PUBLIC ${lua_SOURCE_DIR} )

add_library( lua::lualib  ALIAS lua)

# ====>> Download Sol C++ binding library <<====================#

FetchContent_Declare( sol2 
                      GIT_REPOSITORY  https://github.com/ThePhD/sol2
                      GIT_TAG         v3.2.0
                    )

FetchContent_MakeAvailable( sol2 )
include_directories( ${sol2_SOURCE_DIR}/include )