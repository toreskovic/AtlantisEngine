# Atlantis Engine

## What it is
Atlantis Engine is a small toy engine personal project of mine. Consider everything experimental.
Currently there's two demos / simple benchmarks, `AtlantisBunnyMark` and `AtlantisBunnyMarkLua`

## Main features currently existing in some fashion:
- Custom ECS implementation (supporting multithreading & timeslicing)
- Code hot-reloading
- Scripting language integration (lua for now, more planned later)
- Custom reflection and header parser
- Separate render thread (simple proof of concept 2D renderer for now)
- Game state serialization / deserialization (json for now)

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

You should now see your .sln and .vcxproj files inside the build directory.

You can build the AtlantisBunnyMark project now.

## Running the example

### Linux

```
cd build
./AtlantisEngine
```

### Windows

You know what to do
