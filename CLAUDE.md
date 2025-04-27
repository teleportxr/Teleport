# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands
- Build PC client: `cmake -B build_pc_client -S . && cmake --build build_pc_client --config Release`
- Build Android client: Use Visual Studio solution in build_android_vs
- Enable tests: Add `-DTELEPORT_TEST=ON` to CMake configuration
- Run tests: `ctest` or `ctest -R test_name` for specific test
- Build docs: Add `-DTELEPORT_BUILD_DOCS=ON` to CMake configuration

## Code Style Guidelines
- C++20 standard
- 4-space indentation
- Braces on new line (Allman style)
- 160 character line limit
- Class names: PascalCase (e.g., `TextureManager`)
- Methods/functions: camelCase (e.g., `loadTexture()`)
- Error handling: Use `TELEPORT_ASSERT`, `TELEPORT_CERR` macros
- Project structure: Components are clearly separated into client/server libraries

## Third-party Dependencies
- Handle with CMake using submodules where possible
- Check README.md for detailed build requirements