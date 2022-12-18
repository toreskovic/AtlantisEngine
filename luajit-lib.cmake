include(FetchContent)


# Download LuaJIT library
FetchContent_Declare(
    luajit
    GIT_REPOSITORY  https://github.com/LuaJIT/LuaJIT
    GIT_TAG         v2.1.0-beta3
)
FetchContent_GetProperties(luajit)
if(NOT luajit_POPULATED)
    FetchContent_Populate(luajit)

    # build luajit with make / msbuild
    # make
    if(UNIX)
        execute_process(COMMAND make
                        WORKING_DIRECTORY ${luajit_SOURCE_DIR}
                        RESULT_VARIABLE luajit_build_result
                        OUTPUT_VARIABLE luajit_build_output
                        ERROR_VARIABLE luajit_build_error
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_STRIP_TRAILING_WHITESPACE
        )
        message( [TRACE] " luajit_build_result = ${luajit_build_result} ")
        message( [TRACE] " luajit_build_output = ${luajit_build_output} ")
        message( [TRACE] " luajit_build_error  = ${luajit_build_error}  ")
    # msbuild
    elseif(WIN32)
        # find vswhere.exe
        find_program(_vswhere_tool
            NAMES vswhere
            PATHS "C:/Program Files (x86)/Microsoft Visual Studio/Installer")
        if(NOT _vswhere_tool)
            message(FATAL_ERROR "vswhere.exe not found")
        endif()
        # find VsDevCmd.bat using vswhere.exe
        execute_process(COMMAND ${_vswhere_tool} -latest -property installationPath
                    OUTPUT_VARIABLE _vs_installation_path
                    OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        set(_vs_dev_cmd "${_vs_installation_path}/Common7/Tools/VsDevCmd.bat")
        if(NOT EXISTS ${_vs_dev_cmd})
            message(FATAL_ERROR "VsDevCmd.bat not found")
        endif()
        message( [TRACE] " _vs_dev_cmd = ${_vs_dev_cmd} ")
        # build luajit
        execute_process(COMMAND ${_vs_dev_cmd} "-arch=amd64" "-host_arch=amd64" && "msvcbuild.bat" "static"
                        WORKING_DIRECTORY "${luajit_SOURCE_DIR}/src"
                        RESULT_VARIABLE luajit_build_result
                        OUTPUT_VARIABLE luajit_build_output
                        ERROR_VARIABLE luajit_build_error
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_STRIP_TRAILING_WHITESPACE
        )

        if(luajit_build_error)
            message(FATAL_ERROR "luajit_build_error = ${luajit_build_error}")
        endif()

        message( [TRACE] " luajit_build_result = ${luajit_build_result} ")
        message( [TRACE] " luajit_build_output = ${luajit_build_output} ")
        message( [TRACE] " luajit_build_error  = ${luajit_build_error}  ")
    endif()

endif()

# Expose luajit library
add_library( luajit INTERFACE )
link_directories( ${luajit_SOURCE_DIR}/src )
include_directories( ${luajit_SOURCE_DIR}/src )
# windows
if(WIN32)
    target_link_libraries(luajit INTERFACE lua51.lib luajit.lib minilua.lib buildvm.lib)
# linux
elseif(UNIX)
    target_link_libraries(luajit INTERFACE libluajit.a liblua51.a libminilua.a libbuildvm.a)
endif()
add_library( lua::lualib  ALIAS luajit )

# ====>> Download Sol C++ binding library <<====================#

FetchContent_Declare( sol2 
                      GIT_REPOSITORY  https://github.com/ThePhD/sol2
                      GIT_TAG         v3.3.0
                    )

FetchContent_MakeAvailable( sol2 )
include_directories( ${sol2_SOURCE_DIR}/include )