cmake_minimum_required(VERSION 3.25)

project(ItsAlmostNight VERSION 0.1.0 LANGUAGES CXX)

option(IAN_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(FetchContent)

find_package(raylib 6.0 EXACT CONFIG QUIET)
if(NOT raylib_FOUND)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 6.0
        GIT_SHALLOW TRUE
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

find_package(nlohmann_json 3.11.3 CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG 4f917532aacdb5518569b1c00265c3dea502c452
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raygui)

add_library(ian_simulation
    src/buildings/BuildingSystem.cpp
    src/combat/TowerSystem.cpp
    src/combat/TrapSystem.cpp
    src/combat/CannonSystem.cpp
    src/combat/BombSystem.cpp
    src/combat/PlayerWeaponSystem.cpp
    src/economy/GoldMineSystem.cpp
    src/enemies/EnemySystem.cpp
    src/game/Simulation.cpp
    src/game/GameBalance.cpp
    src/navigation/FlowField.cpp
    src/resources/ResourceSystem.cpp
    src/world/CollisionWorld.cpp
    src/world/MapDefinition.cpp
    src/world/SpatialHash.cpp
    src/waves/WaveDirector.cpp
)
target_include_directories(ian_simulation PUBLIC src)
target_link_libraries(ian_simulation PRIVATE nlohmann_json::nlohmann_json)

add_library(ian_graphics
    src/graphics/EnvironmentSystem.cpp
    src/graphics/GraphicsResources.cpp
    src/graphics/Renderer.cpp
)
target_include_directories(ian_graphics PUBLIC src)
target_link_libraries(ian_graphics PUBLIC raylib PRIVATE nlohmann_json::nlohmann_json)

add_library(ian_ui
    src/ui/GameUi.cpp
)
target_include_directories(ian_ui PUBLIC src)
target_include_directories(ian_ui SYSTEM PRIVATE "${raygui_SOURCE_DIR}/src")
target_link_libraries(ian_ui PUBLIC raylib)

file(COPY assets DESTINATION "${CMAKE_BINARY_DIR}")

add_executable(its_almost_night
    src/app/App.cpp
    src/app/main.cpp
)
target_link_libraries(its_almost_night PRIVATE ian_simulation ian_graphics ian_ui)

function(ian_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
        )
    endif()
endfunction()

ian_set_warnings(ian_simulation)
ian_set_warnings(ian_graphics)
ian_set_warnings(ian_ui)
ian_set_warnings(its_almost_night)

if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
    foreach(target ian_simulation ian_graphics ian_ui its_almost_night)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

if(BUILD_TESTING)
    add_executable(ian_tests
        tests/FixedStepTests.cpp
        tests/BuildingSystemTests.cpp
        tests/BombSystemTests.cpp
        tests/CannonSystemTests.cpp
        tests/CollisionWorldTests.cpp
        tests/EnemySystemTests.cpp
        tests/EnvironmentSystemTests.cpp
        tests/FlowFieldTests.cpp
        tests/GoldMineSystemTests.cpp
        tests/PlayerWeaponSystemTests.cpp
        tests/ResourceSystemTests.cpp
        tests/SimulationTests.cpp
        tests/GameBalanceTests.cpp
        tests/MapDefinitionTests.cpp
        tests/SpatialHashTests.cpp
        tests/TowerSystemTests.cpp
        tests/TrapSystemTests.cpp
        tests/WaveDirectorTests.cpp
    )
    target_link_libraries(ian_tests PRIVATE ian_simulation ian_graphics)
    ian_set_warnings(ian_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_tests COMMAND ian_tests)
endif()
