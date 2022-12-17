# Atlantis Engine

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

You can build the AtlantisBunnyMark project now.

After building a configuration for the first time:

- navigate to `build/\_deps/raylib-build/raylib/Debug(or Release)` and copy the dll to `build/Debug(or Release)`
- Navigate to `build/\_deps/raylib-src/examples/textures/resources` and copy the `wabbit_alpha.png` to the `build/Debug(or Release)/Assets` folder

After every build of AtlantisBunnyMark:

- Go to `build/projects/AtlantisBunnyMark/Debug(or Release)` and copy that dll over to `build/Debug(or Release)/projects/AtlantisBunnyMark`

## Running the example

### Linux

```
cd build
./AtlantisEngine
```

### Windows

You know what to do
