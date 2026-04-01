cmake_minimum_required(VERSION 3.20)
project(wrongfloor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Raylib via FetchContent ──────────────────────────────
include(FetchContent)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5                # pin to a release tag
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(raylib)

# ── nlohmann/json (header-only, for .tmj parsing) ────────
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(nlohmann_json)

# ── Sources ──────────────────────────────────────────────
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

# ── Executable ───────────────────────────────────────────
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE raylib nlohmann_json::nlohmann_json)

# ── DEV_MODE: on for Debug builds and for the default no-config Ninja build ──
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:DEV_MODE>
    $<$<CONFIG:>:DEV_MODE>
    # In dev builds, load assets directly from source so edits are instant.
    $<$<CONFIG:Debug>:ASSETS_PATH="${CMAKE_SOURCE_DIR}/assets/">
    $<$<CONFIG:>:ASSETS_PATH="${CMAKE_SOURCE_DIR}/assets/">
)

# ── Copy assets next to binary (runs every build, not just when exe changes) ──
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    COMMENT "Copying assets to build directory"
)
add_dependencies(${PROJECT_NAME} copy_assets)