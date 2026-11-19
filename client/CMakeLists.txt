
cmake_minimum_required(VERSION 3.24)
project(uksw_hero LANGUAGES C CXX)

find_package(CURL REQUIRED)
find_package(nlohmann_json 3.10 REQUIRED)
find_package(Threads REQUIRED)

find_package(raylib 5.5 QUIET)

if(NOT raylib_FOUND)
    include(FetchContent)

    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )

    FetchContent_MakeAvailable(raylib)
endif()

# Game source files
add_executable(uksw_hero
    src/main.cpp

    # Characters
    src/characters/Student.cpp
    src/characters/Goblin.cpp

    # Game systems
    src/systems/NavigationGrid.cpp
    src/systems/Pathfinding.cpp
    src/systems/GoblinSpawner.cpp

    # Server communication
    src/network/ApiClient.cpp
)

target_compile_features(uksw_hero PRIVATE cxx_std_17)

target_link_libraries(uksw_hero PRIVATE raylib CURL::libcurl nlohmann_json::nlohmann_json Threads::Threads)

# Keep runtime assets beside the executable, including multi-config builds.
add_custom_target(client_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:uksw_hero>/assets"
    COMMENT "Copying client assets"
)

add_dependencies(uksw_hero client_assets)
