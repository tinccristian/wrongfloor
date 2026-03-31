You are working on "wrongfloor" — a game project using raylib 5.5 and C++17, built with CMake + Ninja on Windows.

## Project structure
- src/          → .cpp source files (auto-globbed by CMake)
- include/      → project headers
- assets/       → textures, sounds, maps, etc.
- external/     → reserved for future third-party deps
- build/        → CMake build output (gitignored)
- CMakeLists.txt at project root, raylib fetched via FetchContent

## Code standards
- C++17. Keep it simple — use C++ features where they help, not to show off.
- Every module gets a header in include/ and implementation in src/
- snake_case for functions and variables, PascalCase for types/structs/classes
- Use #pragma once for header guards
- Prefer structs with methods over deep class hierarchies
- Use const, references, and std::string_view where appropriate
- Use RAII for resource management — no raw new/delete
- Avoid heavy template metaprogramming
- Keep files focused and short — one system/module per file (e.g. player.cpp/player.h, camera.cpp/camera.h, tilemap.cpp/tilemap.h)
- No god files. main.cpp should only init, run the game loop, and cleanup.
- Document public-facing functions with a brief comment in the header

## Architecture pattern
Use a simple module pattern:
- Game state lives in a central GameState struct passed around by reference
- Each system (rendering, input, physics, etc.) is its own module with init/update/draw/cleanup functions
- Game loop in main.cpp calls systems in order: input → update → draw

## Build & run
- Configure: cmake -B build -G Ninja
- Build: cmake --build build
- Run: ./build/wrongfloor.exe
- Always verify the project builds and runs after making changes

## When adding features
1. Create the header in include/ first, define the interface
2. Implement in src/
3. Wire it into the game loop in main.cpp
4. Keep the build working at every step — never leave it broken

## What NOT to do
- Don't reorganize the folder structure
- Don't replace FetchContent with git submodules or vendored source
- Don't add dependencies without asking first
- Don't use GLOB without CONFIGURE_DEPENDS (already set up correctly)
