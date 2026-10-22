cmake_minimum_required(VERSION 3.25)

project(ItsAlmostNight VERSION 0.1.0 LANGUAGES CXX)

if(MSVC)
    # Portable Windows ZIP: link the Visual C++ runtime into every target,
    # including FetchContent dependencies, so a fresh machine needs no VC++
    # Redistributable installation to launch the game.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(IAN_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(IAN_ENABLE_PROFILING_SYMBOLS "Keep symbols in optimized profiling builds" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(IAN_ENABLE_PROFILING_SYMBOLS)
    if(MSVC)
        add_compile_options(/Zi)
        add_link_options(/DEBUG)
    else()
        add_compile_options(-g)
    endif()
endif()

include(CTest)
include(FetchContent)

# Homebrew raylib 6.0 is built without SUPPORT_GPU_SKINNING.
# Enemy models require the bone VBOs and animation upload path.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
set(SUPPORT_GPU_SKINNING ON CACHE BOOL "" FORCE)
# App uses raylib's normal EndDrawing() swap/input/frame loop.
# CUSTOMIZE_BUILD otherwise enables manual frame control.
set(SUPPORT_CUSTOM_FRAME_CONTROL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(raylib)

# raylib's native SDF generator defaults to a very short two-source-pixel
# distance range, which is sufficient for antialiasing but clips the 2-4 px
# UI outlines used by the HUD. Keep the native generator and give it enough
# signed-distance range/padding for each scalable 64 px UI atlas.
target_compile_definitions(raylib PRIVATE
    FONT_SDF_CHAR_PADDING=10
    FONT_SDF_PIXEL_DIST_SCALE=16.0f
    # World, postprocess and HUD flush the shared batch several times per
    # frame. Rotate VBOs so uploads do not immediately overwrite the buffer
    # still being consumed by the previous draw (raylib defaults to one).
    RL_DEFAULT_BATCH_BUFFERS=4
)

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
)
FetchContent_MakeAvailable(raygui)

set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LUNASVG_DISABLE_LOAD_SYSTEM_FONTS ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    lunasvg
    GIT_REPOSITORY https://github.com/sammycage/lunasvg.git
    GIT_TAG v3.5.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(lunasvg)

add_library(ian_assets
    src/assets/GlbCollision.cpp
)
target_include_directories(ian_assets PUBLIC src)
target_link_libraries(ian_assets PRIVATE
    nlohmann_json::nlohmann_json)

add_library(ian_progression
    src/progression/InsightSystem.cpp
    src/progression/MetaProgression.cpp
    src/progression/MetaShop.cpp
    src/progression/QuestSystem.cpp
    src/progression/QuestCatalog.cpp
    src/progression/ObjectiveSystem.cpp
)
target_include_directories(ian_progression PUBLIC src)
target_link_libraries(ian_progression PRIVATE nlohmann_json::nlohmann_json)

add_library(ian_simulation
    src/buildings/BuildGrid.cpp
    src/buildings/FoundationSystem.cpp
    src/buildings/FoundationSystemRaycast.cpp
    src/buildings/FoundationSystemStructural.cpp
    src/buildings/FoundationSystemVertical.cpp
    src/buildings/PlacementValidator.cpp
    src/buildings/StructuralSupportGraph.cpp
    src/buildings/SupportSystem.cpp
    src/buildings/BuildingSystem.cpp
    src/buildings/BuildingStats.cpp
    src/combat/AttackProjectile.cpp
    src/combat/TowerSystem.cpp
    src/combat/TrapSystem.cpp
    src/combat/CannonSystem.cpp
    src/combat/BombSystem.cpp
    src/combat/IceWandSystem.cpp
    src/combat/PlayerWeaponSystem.cpp
    src/economy/CrystalMineSystem.cpp
    src/economy/CoinPickupSystem.cpp
    src/enemies/EnemyDecoys.cpp
    src/enemies/EnemyCombat.cpp
    src/enemies/EnemyCollision.cpp
    src/enemies/EnemyBoss.cpp
    src/enemies/EnemyNavigation.cpp
    src/enemies/EnemyAirTank.cpp
    src/enemies/EnemyProjectiles.cpp
    src/enemies/EnemySpawning.cpp
    src/enemies/EnemySystem.cpp
    src/game/Simulation.cpp
    src/game/SimulationCombat.cpp
    src/game/SimulationRepairStations.cpp
    src/game/SimulationCoins.cpp
    src/game/SimulationChallenges.cpp
    src/game/QuestGameplayTracker.cpp
    src/game/LootChestSystem.cpp
    src/game/SimulationBuildingActions.cpp
    src/game/SimulationBuildingCommands.cpp
    src/game/SimulationDebug.cpp
    src/game/SimulationDefenses.cpp
    src/game/SimulationTowerCombat.cpp
    src/game/SimulationClassAbilities.cpp
    src/game/ChestItemCombatSystem.cpp
    src/game/SimulationLoot.cpp
    src/game/SimulationLandmarks.cpp
    src/game/SimulationShrines.cpp
    src/game/SimulationModularPlacement.cpp
    src/game/SimulationPlayerActions.cpp
    src/game/SimulationPlayerLifecycle.cpp
    src/game/SimulationPlayerMovement.cpp
    src/game/SimulationPlacement.cpp
    src/game/SimulationProgression.cpp
    src/game/SimulationRunShop.cpp
    src/game/SimulationSnapshot.cpp
    src/game/SimulationSuspendedRun.cpp
    src/game/SimulationWaves.cpp
    src/game/SimulationWorldSync.cpp
    src/game/ModularCombat.cpp
    src/game/ResourceWorld.cpp
    src/game/GameBalance.cpp
    src/navigation/FlowField.cpp
    src/resources/ResourceSystem.cpp
    src/world/CollisionWorld.cpp
    src/world/MapDefinition.cpp
    src/world/PondDecorationLayout.cpp
    src/world/SpatialHash.cpp
    src/world/TerrainHeightfield.cpp
    src/world/WorldConfig.cpp
    src/world/WorldScenery.cpp
    src/world/WorldSceneryLoot.cpp
    src/waves/WaveDirector.cpp
)
target_include_directories(ian_simulation PUBLIC src)
target_link_libraries(ian_simulation PRIVATE
    nlohmann_json::nlohmann_json ian_assets ian_progression)

add_library(ian_text
    src/localization/Localization.cpp
    src/ui/UiLabels.cpp
    src/ui/UiText.cpp
)
target_include_directories(ian_text PUBLIC src)
target_include_directories(ian_text SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
target_link_libraries(ian_text PUBLIC raylib
    PRIVATE nlohmann_json::nlohmann_json)

add_library(ian_audio
    src/audio/AudioSystem.cpp
)
target_include_directories(ian_audio PUBLIC src)
target_include_directories(ian_audio SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
target_link_libraries(ian_audio PUBLIC raylib)

add_library(ian_settings
    src/app/UserSettings.cpp
)
target_include_directories(ian_settings PUBLIC src)
target_include_directories(ian_settings SYSTEM PRIVATE
    "${raylib_SOURCE_DIR}/src")
target_link_libraries(ian_settings
    PUBLIC raylib
    PRIVATE nlohmann_json::nlohmann_json)

add_library(ian_presentation
    src/presentation/PresentationEffectQueries.cpp
)
target_include_directories(ian_presentation PUBLIC src)

add_library(ian_graphics
    src/graphics/BuildingFeedbackEffect.cpp
    src/graphics/DecorationExclusionMap.cpp
    src/graphics/EnvironmentSystem.cpp
    src/graphics/GlbNodeAnimation.cpp
    src/graphics/GraphicsResourceAccessors.cpp
    src/graphics/GraphicsResources.cpp
    src/graphics/InstanceBufferPool.cpp
    src/graphics/ModularBuildingRenderer.cpp
    src/graphics/Renderer.cpp
    src/graphics/RendererBuildingModels.cpp
    src/graphics/RendererDecorations.cpp
    src/graphics/RendererEnemyModels.cpp
    src/graphics/RendererEnvironmentModels.cpp
    src/graphics/RendererFramePasses.cpp
    src/graphics/RendererLootModels.cpp
    src/graphics/RendererModelRaycasts.cpp
    src/graphics/RendererModels.cpp
    src/graphics/RendererToolModels.cpp
    src/graphics/RendererSettings.cpp
    src/graphics/RendererShaders.cpp
    src/graphics/RendererTerrain.cpp
    src/graphics/TerrainRenderer.cpp
)
target_include_directories(ian_graphics PUBLIC src)
target_include_directories(ian_graphics SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
target_link_libraries(ian_graphics PUBLIC raylib ian_text
    PRIVATE nlohmann_json::nlohmann_json ian_assets)

add_library(ian_ui
    src/ui/GameUi.cpp
    src/ui/QuestMenu.cpp
    src/ui/UnlockMenu.cpp
    src/ui/ShopMenu.cpp
    src/ui/RunMenus.cpp
    src/ui/SettingsMenu.cpp
    src/ui/MainMenuHome.cpp
    src/ui/HudBuildingContextCard.cpp
    src/ui/HudHotbar.cpp
    src/ui/HudRenderer.cpp
    src/ui/HudRunStateOverlay.cpp
    src/ui/HudMinimap.cpp
    src/ui/MinimapLandscape.cpp
    src/ui/MinimapMarkers.cpp
    src/ui/InputKeycap.cpp
    src/ui/InteractionPrompt.cpp
    src/ui/TargetHealthBar.cpp
    src/ui/WorldBillboard.cpp
)
target_include_directories(ian_ui PUBLIC src)
target_compile_definitions(ian_ui PRIVATE IAN_GAME_VERSION="${PROJECT_VERSION}")
target_include_directories(ian_ui SYSTEM PRIVATE
    "${raylib_SOURCE_DIR}/src"
    "${raygui_SOURCE_DIR}/src")
target_link_libraries(ian_ui PUBLIC raylib ian_text ian_progression ian_settings
    PRIVATE nlohmann_json::nlohmann_json lunasvg::lunasvg ian_simulation)

add_executable(its_almost_night
    src/core/PerformanceRecorder.cpp
    src/app/App.cpp
    src/app/AppRunSave.cpp
    src/app/AppRunLifecycle.cpp
    src/app/AppActionModes.cpp
    src/app/AppCoreDefenseMenu.cpp
    src/app/AppEffects.cpp
    src/app/AppAtmosphereEffects.cpp
    src/app/AppEnemySpawnMenu.cpp
    src/app/AppEventPresentation.cpp
    src/app/AppGraphicsPanel.cpp
    src/app/AppSettingsMenu.cpp
    src/app/AppGameplayInput.cpp
    src/app/AppGameplayRender.cpp
    src/app/AppGameplayUi.cpp
    src/app/AppInteractionPrompt.cpp
    src/app/AppMainMenu.cpp
    src/app/MenuBackgroundScene.cpp
    src/app/AppInput.cpp
    src/app/AppItemGrantMenu.cpp
    src/app/AppModularPlacement.cpp
    src/app/AppModularPlacementCommit.cpp
    src/app/AppObjectiveDebugMenu.cpp
    src/app/AppPresentationState.cpp
    src/app/AppPresentationEffects.cpp
    src/app/AppRunUpgradeOverlay.cpp
    src/app/AppRenderPasses.cpp
    src/app/AppRender.cpp
    src/app/AppRenderSupport.cpp
    src/app/AppUpdate.cpp
    src/app/AppWorldOverlayEffects.cpp
    src/app/AppWorldEnemies.cpp
    src/app/AppChallengeFence.cpp
    src/app/AppWorldRender.cpp
    src/app/AppWorldOverlays.cpp
    src/app/main.cpp
)
target_link_libraries(its_almost_night PRIVATE
    ian_graphics ian_ui ian_simulation ian_audio
    ian_presentation ian_settings nlohmann_json::nlohmann_json
)
target_include_directories(its_almost_night SYSTEM PRIVATE
    "${raylib_SOURCE_DIR}/src")

file(GLOB_RECURSE IAN_RUNTIME_ASSET_FILES
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES false
    "${CMAKE_SOURCE_DIR}/assets/*"
)
set(IAN_RUNTIME_ASSET_STAMP
    "${CMAKE_CURRENT_BINARY_DIR}/ian_runtime_assets_$<CONFIG>.stamp")
add_custom_command(
    OUTPUT "${IAN_RUNTIME_ASSET_STAMP}"
    COMMAND "${CMAKE_COMMAND}" -E remove_directory
            "$<TARGET_FILE_DIR:its_almost_night>/assets"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:its_almost_night>/assets"
    COMMAND "${CMAKE_COMMAND}" -E touch
            "${IAN_RUNTIME_ASSET_STAMP}"
    DEPENDS ${IAN_RUNTIME_ASSET_FILES}
    COMMENT "Copying runtime assets"
    VERBATIM
)
add_custom_target(ian_runtime_assets
    DEPENDS "${IAN_RUNTIME_ASSET_STAMP}")
add_dependencies(its_almost_night ian_runtime_assets)

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
ian_set_warnings(ian_assets)
ian_set_warnings(ian_text)
ian_set_warnings(ian_audio)
ian_set_warnings(ian_settings)
ian_set_warnings(ian_presentation)
ian_set_warnings(ian_progression)
ian_set_warnings(ian_graphics)
ian_set_warnings(ian_ui)
ian_set_warnings(its_almost_night)
if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
    foreach(target ian_assets ian_simulation ian_text ian_audio ian_settings ian_presentation ian_progression ian_graphics ian_ui its_almost_night)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endforeach()
endif()

if(BUILD_TESTING)
    add_executable(ian_air_tank_tests tests/AirTankTests.cpp)
    target_link_libraries(ian_air_tank_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_air_tank_tests)
    if(IAN_ENABLE_SANITIZERS)
        target_compile_options(ian_air_tank_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_air_tank_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_air_tank_tests COMMAND ian_air_tank_tests)
    set_tests_properties(ian_air_tank_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 60)

    add_executable(ian_air_tank_graphics_tests tests/AirTankGraphicsTests.cpp)
    target_link_libraries(ian_air_tank_graphics_tests PRIVATE ian_ui ian_graphics ian_simulation)
    target_include_directories(ian_air_tank_graphics_tests SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_air_tank_graphics_tests)
    if(IAN_ENABLE_SANITIZERS)
        target_compile_options(ian_air_tank_graphics_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_air_tank_graphics_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_air_tank_graphics_tests COMMAND ian_air_tank_graphics_tests)
    set_tests_properties(ian_air_tank_graphics_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 60 SKIP_RETURN_CODE 77 RUN_SERIAL TRUE LABELS "graphics;integration")

    add_executable(ian_bee_graphics_tests tests/BeeGraphicsTests.cpp)
    target_link_libraries(ian_bee_graphics_tests PRIVATE ian_graphics ian_simulation)
    target_include_directories(ian_bee_graphics_tests SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_bee_graphics_tests)
    if(IAN_ENABLE_SANITIZERS)
        target_compile_options(ian_bee_graphics_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_bee_graphics_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_bee_graphics_tests COMMAND ian_bee_graphics_tests)
    set_tests_properties(ian_bee_graphics_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 60 SKIP_RETURN_CODE 77 RUN_SERIAL TRUE LABELS "graphics;integration")

    add_executable(ian_bee_tests tests/BeeTests.cpp)
    target_link_libraries(ian_bee_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_bee_tests)
    if(IAN_ENABLE_SANITIZERS)
        target_compile_options(ian_bee_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_bee_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_bee_tests COMMAND ian_bee_tests)
    set_tests_properties(ian_bee_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 60)

    add_executable(ian_balance_lab tools/BalanceLab.cpp)
    target_link_libraries(ian_balance_lab PRIVATE ian_simulation)
    ian_set_warnings(ian_balance_lab)
    if(IAN_ENABLE_SANITIZERS)
        target_compile_options(ian_balance_lab PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_balance_lab PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_balance_lab_checks COMMAND ian_balance_lab --self-test)
    set_tests_properties(ian_balance_lab_checks PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 60)
    add_executable(ian_defense_balance tools/DefenseBalance.cpp)
    target_link_libraries(ian_defense_balance PRIVATE ian_simulation)
    ian_set_warnings(ian_defense_balance)
    if(IAN_ENABLE_SANITIZERS)
        target_compile_options(ian_defense_balance PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_defense_balance PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_defense_balance_checks COMMAND ian_defense_balance --self-test)
    set_tests_properties(ian_defense_balance_checks PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 60)
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME ian_balance_report_checks COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_balance_lab_report.py")
        add_test(NAME ian_defense_balance_report_checks COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_defense_balance_report.py")
        set_tests_properties(ian_defense_balance_report_checks PROPERTIES ENVIRONMENT "IAN_DEFENSE_BALANCE_BINARY=$<TARGET_FILE:ian_defense_balance>")
        add_test(NAME ian_defense_balance_compare_checks COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_defense_balance_compare.py")
    endif()

    add_executable(ian_render_performance_smoke tests/RenderPerformanceSmokeTests.cpp)
    target_link_libraries(ian_render_performance_smoke PRIVATE ian_graphics ian_ui ian_simulation)
    target_include_directories(ian_render_performance_smoke SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_render_performance_smoke)
    if(IAN_ENABLE_SANITIZERS)
        target_compile_options(ian_render_performance_smoke PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_render_performance_smoke PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_render_performance_smoke COMMAND ian_render_performance_smoke)
    set_tests_properties(ian_render_performance_smoke PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" SKIP_RETURN_CODE 77 RUN_SERIAL TRUE)
    add_executable(ian_defense_tests tests/DefenseSystemTests.cpp
        tests/TowerSystemTests.cpp tests/CannonSystemTests.cpp tests/TrapSystemTests.cpp)
    target_link_libraries(ian_defense_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_defense_tests)
    add_test(NAME ian_defense_tests COMMAND ian_defense_tests)
    set_tests_properties(ian_defense_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)

    add_executable(ian_attack_tests tests/AttackSystemTests.cpp)
    target_link_libraries(ian_attack_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_attack_tests)
    add_test(NAME ian_attack_tests COMMAND ian_attack_tests)
    set_tests_properties(ian_attack_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)

    add_executable(ian_flamethrower_tests tests/FlamethrowerAttackTests.cpp)
    target_link_libraries(ian_flamethrower_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_flamethrower_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_flamethrower_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_flamethrower_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_flamethrower_tests COMMAND ian_flamethrower_tests)
    set_tests_properties(ian_flamethrower_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)

    target_compile_definitions(its_almost_night PRIVATE IAN_PRESENTATION_SMOKE_TEST)
    target_sources(its_almost_night PRIVATE tests/AppPresentationEventsSmoke.cpp tools/FrameBenchmark.cpp)
    add_test(NAME ian_presentation_events_smoke COMMAND its_almost_night --test-presentation-events)
    set_tests_properties(ian_presentation_events_smoke PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" SKIP_RETURN_CODE 77 RUN_SERIAL TRUE LABELS "graphics;integration" TIMEOUT 60)

    add_executable(ian_enemy_contact_tests tests/EnemyContactTests.cpp)
    target_link_libraries(ian_enemy_contact_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_enemy_contact_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_enemy_contact_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_enemy_contact_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_enemy_contact_tests COMMAND ian_enemy_contact_tests)
    set_tests_properties(ian_enemy_contact_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)

    add_executable(ian_wind_blower_tests tests/WindBlowerTests.cpp)
    target_link_libraries(ian_wind_blower_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_wind_blower_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_wind_blower_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_wind_blower_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_wind_blower_tests COMMAND ian_wind_blower_tests)
    set_tests_properties(ian_wind_blower_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)

    add_executable(ian_decoy_tests tests/DecoyBeaconTests.cpp)
    target_link_libraries(ian_decoy_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_decoy_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_decoy_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_decoy_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_decoy_tests COMMAND ian_decoy_tests)
    set_tests_properties(ian_decoy_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)

    add_executable(ian_missile_tests tests/MissileLauncherTests.cpp)
    target_link_libraries(ian_missile_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_missile_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_missile_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_missile_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_missile_tests COMMAND ian_missile_tests)
    set_tests_properties(ian_missile_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)

    add_executable(ian_tesla_tests tests/TeslaTowerTests.cpp)
    target_link_libraries(ian_tesla_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_tesla_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_tesla_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_tesla_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_tesla_tests COMMAND ian_tesla_tests)
    set_tests_properties(ian_tesla_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)

    add_executable(ian_laser_tests tests/LaserTowerTests.cpp)
    target_link_libraries(ian_laser_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_laser_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_laser_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_laser_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_laser_tests COMMAND ian_laser_tests)
    set_tests_properties(ian_laser_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)
    add_executable(ian_mortar_tests tests/MortarTests.cpp)
    target_link_libraries(ian_mortar_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_mortar_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_mortar_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_mortar_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_mortar_tests COMMAND ian_mortar_tests)
    set_tests_properties(ian_mortar_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)

    add_executable(ian_enemy_pressure_tests tests/EnemyPressureTests.cpp)
    target_link_libraries(ian_enemy_pressure_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_enemy_pressure_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_enemy_pressure_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_enemy_pressure_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_enemy_pressure_tests COMMAND ian_enemy_pressure_tests)
    set_tests_properties(ian_enemy_pressure_tests PROPERTIES TIMEOUT 60)

    add_executable(ian_night_encounter_tests tests/NightEncounterTests.cpp)
    target_link_libraries(ian_night_encounter_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_night_encounter_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_night_encounter_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_night_encounter_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_night_encounter_tests COMMAND ian_night_encounter_tests)
    set_tests_properties(ian_night_encounter_tests PROPERTIES TIMEOUT 60)

    add_executable(ian_repair_station_tests tests/RepairStationTests.cpp)
    target_link_libraries(ian_repair_station_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_repair_station_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_repair_station_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_repair_station_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_repair_station_tests COMMAND ian_repair_station_tests)
    set_tests_properties(ian_repair_station_tests PROPERTIES TIMEOUT 60)

    add_executable(ian_weapon_arsenal_tests tests/WeaponArsenalTests.cpp)
    target_link_libraries(ian_weapon_arsenal_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_weapon_arsenal_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_weapon_arsenal_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_weapon_arsenal_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_weapon_arsenal_tests COMMAND ian_weapon_arsenal_tests)
    set_tests_properties(ian_weapon_arsenal_tests PROPERTIES TIMEOUT 60)

    add_executable(ian_tests
        tests/FixedStepTests.cpp
        tests/GeometryTests.cpp
        tests/RendererMathTests.cpp
        tests/ResourceCostTests.cpp
        tests/SaturatingArithmeticTests.cpp
        tests/DeterministicRandomTests.cpp
        tests/DecorationExclusionMapTests.cpp
        tests/BuildingSystemTests.cpp
        tests/MenuBackgroundSceneTests.cpp
        src/app/MenuBackgroundScene.cpp
        tests/BuildingStatsTests.cpp
        tests/BuildGridTests.cpp
        tests/FoundationSystemTests.cpp
        tests/ModularCombatTests.cpp
        tests/PresentationEffectQueryTests.cpp
        tests/PresentationTimelineTests.cpp
        tests/UiLabelsTests.cpp
        tests/ResourceWorldTests.cpp
        tests/PlacementValidatorTests.cpp
        tests/PlacementLineTests.cpp
        tests/BombSystemTests.cpp
        tests/IceWandSystemTests.cpp
        tests/ChestItemCombatTests.cpp
        tests/LootChestSystemTests.cpp
        tests/CannonSystemTests.cpp
        tests/CollisionWorldTests.cpp
        tests/EnemySystemTests.cpp
        tests/EnvironmentSystemTests.cpp
        tests/FlowFieldTests.cpp
        tests/CrystalMineSystemTests.cpp
        tests/CoinPickupSystemTests.cpp
        tests/PlayerWeaponSystemTests.cpp
        tests/ResourceSystemTests.cpp
        tests/SimulationTests.cpp
        tests/RunShopTests.cpp
        tests/PlayerClassAbilityTests.cpp
        tests/RunUpgradeTests.cpp
        tests/PowerupTests.cpp
        tests/GameBalanceTests.cpp
        tests/MapDefinitionTests.cpp
        tests/GlbCollisionTests.cpp
        tests/GlbNodeAnimationTests.cpp
        tests/TerrainHeightfieldTests.cpp
        tests/WorldSceneryTests.cpp
        tests/SpatialHashTests.cpp
        tests/InsightSystemTests.cpp
        tests/ObjectiveSystemTests.cpp
        tests/QuestSystemTests.cpp
        tests/UserSettingsTests.cpp
        tests/TowerSystemTests.cpp
        tests/TrapSystemTests.cpp
        tests/WaveDirectorTests.cpp
    )
    target_link_libraries(ian_tests PRIVATE
        ian_simulation ian_graphics ian_presentation
        ian_progression ian_settings)
    target_link_libraries(ian_tests PRIVATE nlohmann_json::nlohmann_json)
    target_compile_definitions(ian_tests PRIVATE
        IAN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_include_directories(ian_tests SYSTEM PRIVATE
        "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_tests)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        foreach(target ian_tests ian_attack_tests ian_defense_tests)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endforeach()
    endif()
    add_executable(ian_chest_item_tests tests/ChestItemTestMain.cpp tests/ChestItemCombatTests.cpp)
    target_link_libraries(ian_chest_item_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_chest_item_tests)
    add_test(NAME ian_chest_item_tests COMMAND ian_chest_item_tests)
    set_tests_properties(ian_chest_item_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 120)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_chest_item_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_chest_item_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_executable(ian_shrine_tests tests/ShrineTests.cpp)
    target_link_libraries(ian_shrine_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_shrine_tests)
    add_test(NAME ian_shrine_tests COMMAND ian_shrine_tests)
    set_tests_properties(ian_shrine_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 120)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_shrine_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_shrine_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_executable(ian_weapon_progression_tests tests/WeaponCardProgressionTests.cpp)
    target_link_libraries(ian_weapon_progression_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_weapon_progression_tests)
    add_test(NAME ian_weapon_progression_tests COMMAND ian_weapon_progression_tests)
    set_tests_properties(ian_weapon_progression_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 120)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_weapon_progression_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_weapon_progression_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_executable(ian_run_shop_tests tests/RunShopTests.cpp)
    target_compile_definitions(ian_run_shop_tests PRIVATE IAN_RUN_SHOP_TEST_MAIN=1)
    target_link_libraries(ian_run_shop_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_run_shop_tests)
    if(IAN_ENABLE_SANITIZERS)
        target_compile_options(ian_run_shop_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(ian_run_shop_tests PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_run_shop_tests COMMAND ian_run_shop_tests)
    set_tests_properties(ian_run_shop_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)
    add_test(NAME ian_tests COMMAND ian_tests)

    add_executable(ian_graphics_smoke
        tests/GraphicsResourceSmokeTests.cpp
        src/app/AppRenderSupport.cpp
    )
    target_link_libraries(ian_graphics_smoke PRIVATE
        ian_graphics ian_simulation ian_ui nlohmann_json::nlohmann_json)
    target_include_directories(ian_graphics_smoke SYSTEM PRIVATE
        "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_graphics_smoke)
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(ian_graphics_smoke PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer)
        target_link_options(ian_graphics_smoke PRIVATE
            -fsanitize=address,undefined)
    endif()
    add_test(NAME ian_graphics_smoke
        COMMAND ian_graphics_smoke)
    set_tests_properties(ian_graphics_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77
        TIMEOUT 90
        LABELS "graphics;integration")
    if(IAN_ENABLE_SANITIZERS)
        # Instrumented model loading and teardown exceed 90s on macOS; GPU
        # performance has a separate smoke test with its own assertions.
        set_tests_properties(ian_graphics_smoke PROPERTIES TIMEOUT 180)
    endif()

    add_executable(ian_ui_text_sdf_smoke
        tests/UiTextSdfSmokeTests.cpp
    )
    target_link_libraries(ian_ui_text_sdf_smoke PRIVATE
        ian_text raylib)
    target_include_directories(ian_ui_text_sdf_smoke SYSTEM PRIVATE
        "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_ui_text_sdf_smoke)
    add_test(NAME ian_ui_text_sdf_smoke
        COMMAND ian_ui_text_sdf_smoke)
    set_tests_properties(ian_ui_text_sdf_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77
        TIMEOUT 30
        LABELS "graphics;ui;integration")

    add_executable(ian_run_upgrade_ui_smoke
        tests/RunUpgradeOverlaySmokeTests.cpp
        src/app/AppRunUpgradeOverlay.cpp)
    target_link_libraries(ian_run_upgrade_ui_smoke PRIVATE ian_ui)
    target_include_directories(ian_run_upgrade_ui_smoke SYSTEM PRIVATE
        "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_run_upgrade_ui_smoke)
    add_test(NAME ian_run_upgrade_ui_smoke COMMAND ian_run_upgrade_ui_smoke)
    set_tests_properties(ian_run_upgrade_ui_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77 TIMEOUT 30 LABELS "graphics;ui;integration")
    add_executable(ian_quest_menu_smoke tests/QuestMenuSmokeTests.cpp)
    add_executable(ian_ui_viewport_smoke tests/UiViewportSmokeTests.cpp)
    add_executable(ian_ui_solid_shapes_smoke tests/UiSolidShapesSmokeTests.cpp)
    target_link_libraries(ian_ui_solid_shapes_smoke PRIVATE ian_ui)
    ian_set_warnings(ian_ui_solid_shapes_smoke)
    add_test(NAME ian_ui_solid_shapes_smoke COMMAND ian_ui_solid_shapes_smoke)
    set_tests_properties(ian_ui_solid_shapes_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77 TIMEOUT 45 LABELS "graphics;ui;integration")
    target_link_libraries(ian_ui_viewport_smoke PRIVATE ian_ui ian_settings)
    ian_set_warnings(ian_ui_viewport_smoke)
    add_test(NAME ian_ui_viewport_smoke COMMAND ian_ui_viewport_smoke)
    set_tests_properties(ian_ui_viewport_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77 TIMEOUT 45 LABELS "graphics;ui;integration")
    add_executable(ian_main_menu_home_smoke tests/MainMenuHomeSmokeTests.cpp)
    add_executable(ian_settings_menu_smoke tests/SettingsMenuSmokeTests.cpp)
    add_executable(ian_run_menus_smoke tests/RunMenusSmokeTests.cpp)
    target_link_libraries(ian_run_menus_smoke PRIVATE ian_ui)
    ian_set_warnings(ian_run_menus_smoke)
    add_test(NAME ian_run_menus_smoke COMMAND ian_run_menus_smoke)
    set_tests_properties(ian_run_menus_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77 TIMEOUT 45 LABELS "graphics;ui;integration")
    target_link_libraries(ian_settings_menu_smoke PRIVATE ian_ui)
    ian_set_warnings(ian_settings_menu_smoke)
    add_test(NAME ian_settings_menu_smoke COMMAND ian_settings_menu_smoke)
    set_tests_properties(ian_settings_menu_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77 TIMEOUT 45 LABELS "graphics;ui;integration")
    target_link_libraries(ian_main_menu_home_smoke PRIVATE ian_ui)
    target_include_directories(ian_main_menu_home_smoke SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_main_menu_home_smoke)
    add_test(NAME ian_main_menu_home_smoke COMMAND ian_main_menu_home_smoke)
    set_tests_properties(ian_main_menu_home_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77 TIMEOUT 45 LABELS "graphics;ui;integration")
    target_link_libraries(ian_quest_menu_smoke PRIVATE ian_ui)
    target_include_directories(ian_quest_menu_smoke SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_quest_menu_smoke)
    add_test(NAME ian_quest_menu_smoke COMMAND ian_quest_menu_smoke)
    set_tests_properties(ian_quest_menu_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77 TIMEOUT 45 LABELS "graphics;ui;integration")
    add_executable(ian_audio_smoke tests/AudioSystemSmokeTests.cpp)
    target_link_libraries(ian_audio_smoke PRIVATE ian_audio nlohmann_json::nlohmann_json)
    target_include_directories(ian_audio_smoke SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_audio_smoke)
    add_test(NAME ian_audio_smoke COMMAND ian_audio_smoke)
    set_tests_properties(ian_audio_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77 TIMEOUT 45 LABELS "audio;integration")
    add_executable(ian_meta_shop_tests tests/MetaShopTests.cpp)
    target_link_libraries(ian_meta_shop_tests PRIVATE ian_simulation ian_progression nlohmann_json::nlohmann_json)
    ian_set_warnings(ian_meta_shop_tests)
    add_test(NAME ian_meta_shop_tests COMMAND ian_meta_shop_tests)
    set_tests_properties(ian_meta_shop_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 60)
    add_executable(ian_shop_menu_smoke tests/ShopMenuSmokeTests.cpp)
    target_link_libraries(ian_shop_menu_smoke PRIVATE ian_ui)
    target_include_directories(ian_shop_menu_smoke SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_shop_menu_smoke)
    add_test(NAME ian_shop_menu_smoke COMMAND ian_shop_menu_smoke)
    set_tests_properties(ian_shop_menu_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77 TIMEOUT 45 LABELS "graphics;ui;integration")
    add_executable(ian_player_respawn_tests tests/PlayerRespawnTests.cpp)
    target_link_libraries(ian_player_respawn_tests PRIVATE ian_simulation)
    ian_set_warnings(ian_player_respawn_tests)
    add_test(NAME ian_player_respawn_tests COMMAND ian_player_respawn_tests)
    set_tests_properties(ian_player_respawn_tests PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" TIMEOUT 90)
    add_executable(ian_player_death_ui_smoke tests/PlayerDeathOverlaySmokeTests.cpp)
    target_link_libraries(ian_player_death_ui_smoke PRIVATE ian_ui)
    ian_set_warnings(ian_player_death_ui_smoke)
    add_test(NAME ian_player_death_ui_smoke COMMAND ian_player_death_ui_smoke)
    set_tests_properties(ian_player_death_ui_smoke PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" SKIP_RETURN_CODE 77 TIMEOUT 45 LABELS "graphics;ui;integration")
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        foreach(target ian_player_respawn_tests ian_player_death_ui_smoke)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endforeach()
    endif()
    add_executable(ian_unlock_menu_smoke tests/UnlockMenuSmokeTests.cpp)
    target_link_libraries(ian_unlock_menu_smoke PRIVATE ian_ui)
    target_include_directories(ian_unlock_menu_smoke SYSTEM PRIVATE "${raylib_SOURCE_DIR}/src")
    ian_set_warnings(ian_unlock_menu_smoke)
    add_test(NAME ian_unlock_menu_smoke COMMAND ian_unlock_menu_smoke)
    set_tests_properties(ian_unlock_menu_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        SKIP_RETURN_CODE 77 TIMEOUT 45 LABELS "graphics;ui;integration")
    if(IAN_ENABLE_SANITIZERS AND NOT MSVC)
        foreach(target ian_audio_smoke ian_ui_text_sdf_smoke ian_ui_viewport_smoke ian_ui_solid_shapes_smoke ian_main_menu_home_smoke ian_settings_menu_smoke ian_run_menus_smoke ian_run_upgrade_ui_smoke ian_quest_menu_smoke ian_unlock_menu_smoke ian_shop_menu_smoke ian_meta_shop_tests)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endforeach()
    endif()
endif()
