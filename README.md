# Atlantis Engine

This provides a base project template which builds with [CMake](https://cmake.org).

## Prerequisites

- CMake (https://cmake.org/) ( >= 3.11)
- Python 3 ( >= 3.9)
- libclang python library (https://pypi.org/project/libclang/)
- C++20 support in your compiler of choice

## Compiling the example

### Linux

```
cmake -B build -D CMAKE_BUILD_TYPE=Release
python set_active_project.py AtlantisBunnyMark
cmake --build build --target AtlantisEngine
cmake --build build --target AtlantisBunnyMark
```

### Windows

```
git checkout Windows
cmake -B build -D CMAKE_BUILD_TYPE=Release
python set_active_project.py AtlantisBunnyMark
```

After doing this, you should have your fancy new Visual Studio solution and projects inside the build folder. We're not done yet:

1. Set AtlantisEngine as the startup project
2. Open the AtlantisEngine project's properties
   - Under the Build-Events category, set the Pre-Link event to the following
   ```
   @ECHO ON
   @ECHO "$(VC_ExecutablePath_x64)\lib.exe" /out:"$(OutDir)$(ProjectName).lib" "$(SolutionDir)$(ProjectName).dir\$(Configuration)\*.obj"
   "$(VC_ExecutablePath_x64)\lib.exe" /out:"$(OutDir)$(ProjectName).lib" "$(SolutionDir)$(ProjectName).dir\$(Configuration)\*.obj"
   ```
   - Under the Linker category, find System, and finally set SubSystem to Windows
3. You can build the AtlantisBunnyMark project now
4. Inside the build folder, navigate to \_deps/raylib-build/raylib/Debug(or Release) and copy the dll back to the root build/Debug(or Release)
5. Go to build/projects/AtlantisBunnyMark/Debug and copy that dll over to build/Debug/projects/AtlantisBunnyMark

NOTE: when changing Debug / Release configurations, steps from 2. onward need to be repeated for the new configuration

## Running the example

```
cd build
./AtlantisEngine
```
