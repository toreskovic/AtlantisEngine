# Atlantis Engine

This provides a base project template which builds with [CMake](https://cmake.org).

## Prerequisites
- CMake (https://cmake.org/)
- Python 3.9 (but other versions might work)
- libclang python library (https://pypi.org/project/libclang/)

## Compiling the example

```
cmake -B build -D CMAKE_BUILD_TYPE=Release
python set_active_project.py AtlantisBunnyMark
cmake --build build --target AtlantisEngine
cmake --build build --target AtlantisBunnyMark
```

## Running the example
```
cd build
./AtlantisEngine
```
