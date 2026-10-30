#include "ui/UiViewport.hpp"
#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "buildings/BuildingHotbarLayout.hpp"
#include "buildings/BuildingOrientation.hpp"

#include "localization/Localization.hpp"
#include "ui/UiCString.hpp"
#include "ui/UiText.hpp"
#include "ui/UiLabels.hpp"

#include <nlohmann/json.hpp>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace ian {
using namespace app_detail;
namespace {

struct MenuLayout {
    float scale{};
    Vector2 origin{};

    [[nodiscard]] Rectangle rect(
        float x, float y, float width, float height) const {
        return {
            origin.x + x * scale,
            origin.y + y * scale,
            width * scale,
            height * scale,
        };
    }

    [[nodiscard]] Vector2 point(float x, float y) const {
        return {origin.x + x * scale, origin.y + y * scale};
    }

    [[nodiscard]] Rectangle topRect(
        float x, float y, float width, float height) const {
        return {
            origin.x + x * scale, y * scale,
            width * scale, height * scale,
        };
    }

    [[nodiscard]] Vector2 topPoint(float x, float y) const {
        return {origin.x + x * scale, y * scale};
    }
};

MenuLayout menuLayout(float DesignWidth = 1920.0F, float DesignHeight = 1080.0F) {
    const float screenWidth = static_cast<float>(uiScreenWidth());
    const float screenHeight = static_cast<float>(uiScreenHeight());
    const float scale = std::min(
        screenWidth / DesignWidth,
        screenHeight / DesignHeight);
    return {
        .scale = scale,
        .origin = {
            (screenWidth - DesignWidth * scale) * 0.5F,
            (screenHeight - DesignHeight * scale) * 0.5F,
        },
    };
}

constexpr Color MenuPanelFill{29, 34, 38, 228};
constexpr Color MenuPanelBorder{116, 79, 48, 255};
constexpr Color MenuCream{250, 241, 211, 255};
constexpr Color MenuGold{255, 205, 105, 255};
constexpr const char* MenuBackgroundViewPath =
    "user_settings/menu_background_view.json";

std::span<const BuildingType> menuBackgroundCatalog(int category) {
    if (category == 1) return DefenseBuildingOrder;
    if (category == 2) return ModularBuildingOrder;
    return BaseBuildingOrder;
}

void drawMenuPanel(Rectangle bounds, const MenuLayout& layout) {
    DrawRectangleRounded(
        {bounds.x + 3.0F * layout.scale,
         bounds.y + 5.0F * layout.scale,
         bounds.width, bounds.height},
        0.07F, 8, {9, 12, 15, 135});
    DrawRectangleRounded(bounds, 0.07F, 8, MenuPanelFill);
    DrawRectangleRoundedLinesEx(
        bounds, 0.07F, 8, 3.0F * layout.scale,
        MenuPanelBorder);
}

bool drawMenuButton(
    Rectangle bounds, std::string_view label,
    const MenuLayout& layout, bool primary = false, bool secondary = false) {
    const bool hovered = CheckCollisionPointRec(
        uiMousePosition(), bounds);
    const bool pressed = hovered &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const Color fill = primary
        ? pressed ? Color{174, 92, 35, 255}
                  : hovered ? Color{233, 143, 56, 255}
                            : Color{211, 116, 46, 255}
        : pressed ? Color{54, 65, 70, 255}
                  : hovered ? Color{75, 87, 90, 255}
                            : Color{45, 54, 58, 255};
    const Color edge = primary
        ? Color{103, 50, 27, 255}
        : Color{17, 21, 24, 255};
    DrawRectangleRounded(
        {bounds.x, bounds.y + 4.0F * layout.scale,
         bounds.width, bounds.height},
        0.18F, 8, edge);
    DrawRectangleRounded(
        {bounds.x, bounds.y,
         bounds.width, bounds.height - 3.0F * layout.scale},
        0.18F, 8, fill);
    DrawRectangleRoundedLinesEx(
        bounds, 0.18F, 8, 3.0F * layout.scale,
        primary ? Color{255, 204, 111, 255}
                : Color{129, 101, 72, 255});
    const ScopedUiFontRole font(secondary ? UiFontRole::BodyBold : UiFontRole::Display);
    const std::string localized = localizeText(label);
    const float size = fitUiTextSize(
        label, (secondary ? 17.0F : primary ? 25.0F : 19.0F) * layout.scale,
        11.0F * layout.scale,
        bounds.width - 22.0F * layout.scale,
        bounds.height - 13.0F * layout.scale);
    const Vector2 measured = measureUiText(localized, size);
    drawUiText(
        localized,
        {bounds.x + (bounds.width - measured.x) * 0.5F,
         bounds.y + (bounds.height - measured.y) * 0.5F -
             2.0F * layout.scale},
        size, MenuCream);
    return hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

Vector3 menuViewDirection(float yaw, float pitch) {
    return {
        std::sin(yaw) * std::cos(pitch),
        std::sin(pitch),
        -std::cos(yaw) * std::cos(pitch),
    };
}

void drawOutlinedTitle(
    std::string_view sourceText, const MenuLayout& layout,
    float centerX, float y, float preferredSize,
    float maximumWidth) {
    const std::string text = localizeText(sourceText);
    const float logicalSize = fitUiTextSize(
        sourceText, preferredSize, 28.0F,
        maximumWidth * layout.scale);
    const Vector2 measured = measureUiText(sourceText,logicalSize*layout.scale);
    const Vector2 position{
        layout.origin.x + centerX * layout.scale - measured.x * .5F,
        layout.origin.y + y * layout.scale,
    };
    SdfUiTextStyle style{};
    style.fillColor = {250,245,224,255};
    style.outlineColor = {9,12,15,255};
    style.outlinePixels = 4 * layout.scale;
    style.shadowColor = BLANK;
    drawStyledSdfUiText(text,position,logicalSize*layout.scale,style);
}

} // namespace

bool App::loadMenuBackgroundView() {
    for (const char* path : {
             MenuBackgroundViewPath,
             "assets/data/default_menu_background_view.json"}) {
        try {
        std::ifstream input(path);
        if (!input) continue;
        const nlohmann::json document =
            nlohmann::json::parse(input);
        const auto& position = document.at("position");
        const auto& target = document.at("target");
        if (!position.is_array() || position.size() != 3 ||
            !target.is_array() || target.size() != 3) {
            continue;
        }
        const std::uint32_t seed =
            document.at("terrainSeed").get<std::uint32_t>();
        const Camera3D camera{
            .position = {
                position.at(0).get<float>(),
                position.at(1).get<float>(),
                position.at(2).get<float>(),
            },
            .target = {
                target.at(0).get<float>(),
                target.at(1).get<float>(),
                target.at(2).get<float>(),
            },
            .up = {0.0F, 1.0F, 0.0F},
            .fovy = 61.0F,
            .projection = CAMERA_PERSPECTIVE,
        };
        const auto valid = [](Vector3 value) {
            return std::isfinite(value.x) &&
                std::isfinite(value.y) &&
                std::isfinite(value.z) &&
                std::abs(value.x) < 1000.0F &&
                std::abs(value.y) < 1000.0F &&
                std::abs(value.z) < 1000.0F;
        };
        if (seed == 0U || !valid(camera.position) ||
            !valid(camera.target) ||
            Vector3LengthSqr(Vector3Subtract(
                camera.target, camera.position)) < 0.01F) {
            continue;
        }
        std::optional<MenuBackgroundScene> scene;
        if (document.contains("buildings")) {
            scene.emplace();
            if (!scene->loadJson(document.at("buildings"),
                    simulation_.snapshot().worldLimit)) continue;
        }
        menuBackgroundScene_ = std::move(scene);
        menuBackgroundCamera_ = camera;
        menuBackgroundTerrainSeed_ = seed;
        return true;
        } catch (...) {
        }
    }
    return false;
}

bool App::saveMenuBackgroundView() {
    try {
        const auto path = std::filesystem::path(
            MenuBackgroundViewPath);
        std::filesystem::create_directories(
            path.parent_path());
        const std::uint32_t seed =
            simulation_.terrain().seed();
        nlohmann::json document{
            {"version", 2},
            {"terrainSeed", seed},
            {"position", {
                menuBackgroundCamera_.position.x,
                menuBackgroundCamera_.position.y,
                menuBackgroundCamera_.position.z}},
            {"target", {
                menuBackgroundCamera_.target.x,
                menuBackgroundCamera_.target.y,
                menuBackgroundCamera_.target.z}},
        };
        if (menuBackgroundScene_) {
            document["buildings"] = menuBackgroundScene_->toJson();
        }
        const auto temporary = std::filesystem::path(path.string() + ".tmp");
        std::ofstream output(temporary);
        output << document.dump(2) << '\n';
        output.close();
        if (!output) return false;
        std::filesystem::rename(temporary, path);
        menuBackgroundTerrainSeed_ = seed;
        return true;
    } catch (...) {
        return false;
    }
}

void App::beginMenuBackgroundEdit(
    const SimulationSnapshot& snapshot) {
    menuBackgroundCameraBeforeEdit_ = menuBackgroundCamera_;
    menuBackgroundSceneBeforeEdit_ = menuBackgroundScene_;
    const double ground = simulation_.terrain().getHeight(
        snapshot.playerPosition.x, snapshot.playerPosition.z);
    menuBackgroundCamera_ = {
        .position = {
            static_cast<float>(snapshot.playerPosition.x) + 13.0F,
            static_cast<float>(ground) + 6.0F,
            static_cast<float>(snapshot.playerPosition.z) + 15.0F,
        },
        .target = {
            static_cast<float>(snapshot.playerPosition.x) - 1.0F,
            static_cast<float>(ground) + 1.8F,
            static_cast<float>(snapshot.playerPosition.z) - 8.0F,
        },
        .up = {0.0F, 1.0F, 0.0F},
        .fovy = 61.0F,
        .projection = CAMERA_PERSPECTIVE,
    };
    const bool savedViewMatches = menuBackgroundTerrainSeed_ &&
        *menuBackgroundTerrainSeed_ ==
            simulation_.terrain().seed();
    if (savedViewMatches) {
        static_cast<void>(loadMenuBackgroundView());
    }
    if (!menuBackgroundScene_ || !savedViewMatches) {
        menuBackgroundScene_.emplace();
        static_cast<void>(menuBackgroundScene_->copyBuildings(snapshot.buildings));
    }
    menuBackgroundBuildingCategory_ = std::any_of(
        menuBackgroundScene_->buildings().begin(),
        menuBackgroundScene_->buildings().end(),
        [](const BuildingInstance& building) {
            return building.type == BuildingType::Core;
        }) ? 2 : 0;
    menuBackgroundBuildingSlot_ = 0;
    menuBackgroundBuildingRotation_ = 0U;
    menuBackgroundBuildingEnabled_ = true;
    menuBackgroundMenuPreviewVisible_ = true;
    menuBackgroundBuildingPreview_.reset();
    const Vector3 direction = Vector3Normalize(Vector3Subtract(
        menuBackgroundCamera_.target,
        menuBackgroundCamera_.position));
    menuBackgroundYaw_ = std::atan2(direction.x, -direction.z);
    menuBackgroundPitch_ = std::asin(direction.y);
    menuBackgroundEditing_ = true;
    DisableCursor();
}

void App::processMenuBackgroundInput(
    const SimulationSnapshot& snapshot) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        menuBackgroundEditing_ = false;
        menuBackgroundCamera_ = menuBackgroundCameraBeforeEdit_;
        menuBackgroundScene_ = std::move(menuBackgroundSceneBeforeEdit_);
        menuBackgroundBuildingPreview_.reset();
        EnableCursor();
        audio_.playUiConfirm();
        return;
    }
    if (IsKeyPressed(KEY_ENTER)) {
        if (saveMenuBackgroundView()) {
            menuBackgroundEditing_ = false;
            menuBackgroundSceneBeforeEdit_.reset();
            menuBackgroundBuildingPreview_.reset();
            EnableCursor();
            audio_.playUiConfirm();
        } else {
            audio_.playUiError();
        }
        return;
    }
    if (IsKeyPressed(KEY_H)) {
        menuBackgroundMenuPreviewVisible_ = !menuBackgroundMenuPreviewVisible_;
        audio_.playUiConfirm();
    }
    const float deltaTime = std::clamp(
        GetFrameTime(), 0.0F, 0.05F);
    const Vector2 mouse = GetMouseDelta();
    const float sensitivity = 0.0025F *
        userSettings_.controls.mouseSensitivity;
    menuBackgroundYaw_ += mouse.x * sensitivity;
    menuBackgroundPitch_ = std::clamp(
        menuBackgroundPitch_ - mouse.y * sensitivity,
        -1.4F, 1.4F);
    const Vector3 forward = menuViewDirection(
        menuBackgroundYaw_, menuBackgroundPitch_);
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(
        forward, {0.0F, 1.0F, 0.0F}));
    Vector3 movement{};
    if (IsKeyDown(KEY_W)) movement = Vector3Add(movement, forward);
    if (IsKeyDown(KEY_S)) movement = Vector3Subtract(movement, forward);
    if (IsKeyDown(KEY_D)) movement = Vector3Add(movement, right);
    if (IsKeyDown(KEY_A)) movement = Vector3Subtract(movement, right);
    if (IsKeyDown(KEY_E) || IsKeyDown(KEY_SPACE)) movement.y += 1.0F;
    if (IsKeyDown(KEY_Q) || IsKeyDown(KEY_LEFT_CONTROL) ||
        IsKeyDown(KEY_RIGHT_CONTROL)) movement.y -= 1.0F;
    if (Vector3LengthSqr(movement) > 0.0F) {
        const float speed =
            (IsKeyDown(KEY_LEFT_SHIFT) ||
             IsKeyDown(KEY_RIGHT_SHIFT)) ? 26.0F : 10.0F;
        menuBackgroundCamera_.position = Vector3Add(
            menuBackgroundCamera_.position,
            Vector3Scale(Vector3Normalize(movement),
                         speed * deltaTime));
    }
    const float limit = static_cast<float>(snapshot.worldLimit) - 3.0F;
    menuBackgroundCamera_.position.x = std::clamp(
        menuBackgroundCamera_.position.x, -limit, limit);
    menuBackgroundCamera_.position.z = std::clamp(
        menuBackgroundCamera_.position.z, -limit, limit);
    const float ground = static_cast<float>(
        simulation_.terrain().getHeight(
            menuBackgroundCamera_.position.x,
            menuBackgroundCamera_.position.z));
    menuBackgroundCamera_.position.y = std::clamp(
        menuBackgroundCamera_.position.y,
        ground + 0.12F, ground + 90.0F);
    menuBackgroundCamera_.target = Vector3Add(
        menuBackgroundCamera_.position, forward);

    // The composition preview keeps camera controls, but hides construction
    // tools and ignores clicks so neither menu actions nor unseen edits occur.
    if (menuBackgroundMenuPreviewVisible_) {
        menuBackgroundBuildingPreview_.reset();
        return;
    }

    if (IsKeyPressed(KEY_B)) {
        menuBackgroundBuildingEnabled_ = !menuBackgroundBuildingEnabled_;
    }
    if (IsKeyPressed(KEY_TAB)) {
        menuBackgroundBuildingCategory_ = (menuBackgroundBuildingCategory_ + 1) % 3;
        menuBackgroundBuildingSlot_ = 0;
        menuBackgroundBuildingRotation_ = 0U;
    }
    const auto catalog = menuBackgroundCatalog(menuBackgroundBuildingCategory_);
    const int count = static_cast<int>(catalog.size());
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0F) {
        menuBackgroundBuildingSlot_ =
            (menuBackgroundBuildingSlot_ + (wheel > 0.0F ? -1 : 1) + count) % count;
        menuBackgroundBuildingRotation_ = 0U;
    }
    for (int slot = 0; slot < count; ++slot) {
        if (IsKeyPressed(KEY_ONE + slot)) {
            menuBackgroundBuildingSlot_ = slot;
            menuBackgroundBuildingRotation_ = 0U;
        }
    }
    const BuildingType type = catalog[static_cast<std::size_t>(menuBackgroundBuildingSlot_)];
    if (IsKeyPressed(KEY_R)) {
        menuBackgroundBuildingRotation_ = static_cast<std::uint8_t>(
            (menuBackgroundBuildingRotation_ + 1U) % buildingRotationStepCount(type));
    }
    menuBackgroundBuildingPreview_.reset();
    if (!menuBackgroundBuildingEnabled_ || !menuBackgroundScene_) return;
    const Vec3 origin{
        menuBackgroundCamera_.position.x,
        menuBackgroundCamera_.position.y,
        menuBackgroundCamera_.position.z};
    const Vec3 direction{forward.x, forward.y, forward.z};
    constexpr double MaximumBuildDistance = 120.0;
    const auto terrainHit = simulation_.terrain().raycast(
        origin, direction, MaximumBuildDistance);
    if (terrainHit) {
        menuBackgroundBuildingPreview_ = menuBackgroundScene_->preview(
            type, *terrainHit, menuBackgroundBuildingRotation_,
            simulation_.terrain(), snapshot.worldLimit);
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        const double terrainDistance = terrainHit
            ? std::sqrt((terrainHit->x - origin.x) * (terrainHit->x - origin.x) +
                        (terrainHit->y - origin.y) * (terrainHit->y - origin.y) +
                        (terrainHit->z - origin.z) * (terrainHit->z - origin.z))
            : MaximumBuildDistance;
        if (menuBackgroundScene_->remove(origin, direction, terrainDistance)) {
            audio_.playUiConfirm();
        } else {
            audio_.playUiError();
        }
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (menuBackgroundBuildingPreview_ &&
            menuBackgroundScene_->place(*menuBackgroundBuildingPreview_)) {
            audio_.playUiConfirm();
        } else {
            audio_.playUiError();
        }
    }
    // Revalidate immediately after placement/removal for the same-frame ghost.
    if (terrainHit) {
        menuBackgroundBuildingPreview_ = menuBackgroundScene_->preview(
            type, *terrainHit, menuBackgroundBuildingRotation_,
            simulation_.terrain(), snapshot.worldLimit);
    }
}

void App::drawMainMenuWorld(
    const SimulationSnapshot& sourceSnapshot) {
    auto snapshot = sourceSnapshot;
    snapshot.relicGhosts={};
    snapshot.relicCombatBuildings={};
    if (menuBackgroundScene_ && (menuBackgroundEditing_ ||
            (menuBackgroundTerrainSeed_ &&
             *menuBackgroundTerrainSeed_ == snapshot.terrainSeed))) {
        snapshot.buildings = menuBackgroundScene_->buildings();
    }
    const double terrainY = simulation_.terrain().getHeight(
        snapshot.playerPosition.x, snapshot.playerPosition.z);
    const float drift = static_cast<float>(
        std::sin(GetTime() * 0.12) * 1.6);
    const Camera3D defaultCamera{
        .position = {
            static_cast<float>(snapshot.playerPosition.x) + 13.0F + drift,
            static_cast<float>(terrainY) + 6.0F,
            static_cast<float>(snapshot.playerPosition.z) + 15.0F,
        },
        .target = {
            static_cast<float>(snapshot.playerPosition.x) - 1.0F,
            static_cast<float>(terrainY) + 1.8F,
            static_cast<float>(snapshot.playerPosition.z) - 8.0F,
        },
        .up = {0.0F, 1.0F, 0.0F},
        .fovy = 61.0F,
        .projection = CAMERA_PERSPECTIVE,
    };
    const Camera3D camera =
        (menuBackgroundEditing_ || menuBackgroundTerrainSeed_)
            ? menuBackgroundCamera_ : defaultCamera;

    environment_.setAutomaticTime(0.22F);
    const EnvironmentState environment = environment_.state();
    const Vector3 forward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    const Vector3 cameraRight = Vector3Normalize(
        Vector3CrossProduct(forward, {0.0F, 1.0F, 0.0F}));
    const Vector3 cameraUp = Vector3Normalize(
        Vector3CrossProduct(cameraRight, forward));
    renderer_->setMenuDepthOfField(false);
    const WorldLighting lighting{
        .cameraPosition = camera.position,
        .sunDirection = Vector3Scale(
            environment.celestialDirection, -1.0F),
        .sunColor = environment.sunColor,
        .sunIntensity = environment.sunIntensity,
        .skyAmbientColor = environment.skyAmbientColor,
        .groundAmbientColor = environment.groundAmbientColor,
        .ambientIntensity = environment.ambientIntensity,
        .cloudShadowStrength = 0.12F,
        .fogColor = colorToVector(environment.fogColor),
        .fogStart = environment.fogStart,
        .fogEnd = environment.fogEnd,
        .dayNightTint = environment.dayNightTint,
        .exposure = environment.exposure,
        .saturation = environment.saturation,
    };
    const SkyState sky{
        .cameraForward = forward,
        .cameraRight = cameraRight,
        .cameraUp = cameraUp,
        .verticalFovDegrees = camera.fovy,
        .zenithColor = colorToVector(environment.skyTop),
        .horizonColor = colorToVector(environment.skyHorizon),
        .lowerSkyColor = colorToVector(environment.lowerSky),
        .celestialDirection = environment.celestialDirection,
        .celestialColor = environment.celestialColor,
        .celestialIntensity = environment.sunIntensity,
        .nightAmount = environment.nightFactor,
        .timeSeconds = static_cast<float>(GetTime()),
        .exposure = environment.exposure,
        .saturation = environment.saturation,
    };

    renderer_->setWorldReveal(
        {static_cast<float>(snapshot.playerPosition.x),
         static_cast<float>(snapshot.playerPosition.z)},
        1000.0F);
    drawShadowPass(snapshot, lighting);
    renderer_->beginWorldPass(environment.skyHorizon, camera);
    renderer_->drawSky(sky);
    BeginMode3D(camera);
    renderer_->beginWorldShader(lighting);
    WorldMaterialState terrainMaterial{};
    terrainMaterial.terrainAmount = 1.0F;
    terrainMaterial.bakedAo = 0.9F;
    renderer_->setWorldMaterial(terrainMaterial);
    renderer_->drawTerrain({66, 112, 67, 255}, camera.position);
    drawWorldEntities(
        snapshot, camera, environment.nightFactor, lighting, 1.0F);
    if constexpr (!MinimalWorldVisuals) {
        renderer_->beginWorldShader(lighting);
        WorldMaterialState pondMaterial{};
        pondMaterial.bakedAo = 0.8F;
        pondMaterial.screenAoAmount = 0.0F;
        renderer_->setWorldMaterial(pondMaterial);
        renderer_->drawPondShoreRocks(camera);
        renderer_->drawPondDecor(camera);
        renderer_->endWorldShader();
        renderer_->drawWater(camera.position, lighting);
        renderer_->drawClouds(
            camera, environment.nightFactor, lighting);
        drawBlobShadows(snapshot, camera);
    }
    if (menuBackgroundEditing_ && !menuBackgroundMenuPreviewVisible_ &&
        menuBackgroundBuildingPreview_) {
        const auto& preview = *menuBackgroundBuildingPreview_;
        const Vec3 center = buildingWorldPosition(preview.type, preview.gridPosition);
        renderer_->beginWorldShader(lighting);
        WorldMaterialState ghost{};
        ghost.ghostAmount = 1.0F;
        ghost.ghostTint = preview.placement.valid()
            ? Vector3{0.25F, 0.95F, 0.5F} : Vector3{1.0F, 0.25F, 0.2F};
        ghost.ghostOpacity = 0.35F;
        ghost.screenAoAmount = 0.0F;
        renderer_->setWorldMaterial(ghost);
        rlDisableDepthMask();
        drawPlacementPreviewGeometry(
            *renderer_, preview.type,
            {static_cast<float>(center.x), static_cast<float>(preview.baseHeight),
             static_cast<float>(center.z)},
            static_cast<float>(buildingRotationYaw(preview.type, preview.rotation)),
            preview.rotation,
            wallConnectionMask(snapshot.buildings, preview.gridPosition, preview.baseHeight));
        renderer_->endWorldShader();
        rlEnableDepthMask();
        drawPlacementFootprint(preview,
            {static_cast<float>(center.x), static_cast<float>(center.z)}, 0.0F);
    }
    EndMode3D();
    renderer_->endWorldPass();
    renderer_->setMenuDepthOfField(false);
}

void App::drawMainMenu() {
    const ScopedUiCanvas canvas;
    if (renderer_->graphicsPanelVisible() || classSelectionVisible_ ||
        mainMenuPage_ != MainMenuPage::None || menuBackgroundEditing_ ||
        IsKeyPressed(KEY_ESCAPE)) {
        // Keyboard navigation can leave home while a mouse transition is
        // pending. Do not replay that old click when home becomes visible.
        mainMenuHomeState_.pendingAction = MainMenuHomeAction::None;
        mainMenuHomeState_.pointerWasDown = false;
    }
    if (renderer_->graphicsPanelVisible()) {
        return;
    }
    if (classSelectionVisible_) {
        auto navigation = readMenuNavigationInput();
        if (!navigation.horizontalStep) navigation.horizontalStep = IsKeyPressed(KEY_D) ? 1 : IsKeyPressed(KEY_A) ? -1 : 0;
        const int previousFocus = classSelectionState_.focused;
        const auto result = drawClassSelectionMenu(ui_, metaProgression_, selectedPlayerClass_,
            sandboxClassSelection_, classCollectionOnly_, classSelectionState_,
            {static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())}, navigation);
        pendingStartFromUi_ = pendingStartFromUi_ || result.start;
        if (result.back) {
            classSelectionVisible_ = false;
            classCollectionOnly_ = false;
            sandboxClassSelection_ = false;
            pendingStartFromUi_ = false;
        }
        if (previousFocus != classSelectionState_.focused &&
            (navigation.tabStep || navigation.horizontalStep || navigation.verticalStep)) audio_.playUiSelect();
        if (result.back || (result.interaction &&
            (navigation.click || navigation.activate))) audio_.playUiConfirm();
        return;
    }
    const MenuLayout layout = menuLayout();
    const auto drawHome = [&](MainMenuHomeState& state, const MainMenuHomeInput& input) {
        const auto featured = questSystem_.recommendedStatuses(metaProgression_, MainMenuFeaturedQuestCount);
        const auto progress = questSystem_.categoryProgress(metaProgression_);
        state.totalQuestCount = progress.total;
        state.completedQuestCount = progress.completed;
        state.unclaimedQuestRewards = progress.unclaimedRewards;
        if (featured.empty()) {
            const auto rows = questSystem_.statuses(metaProgression_);
            state.allQuestsCompleted = std::ranges::all_of(
                rows, [](const auto& row) { return row.completed; });
        }
        const bool previousFriends = state.friendsLeaderboardVisible;
        const auto previousFocus = state.selectedButton;
        const auto homeResult = drawMainMenuHome(ui_, metaProgression_, featured,
            suspendedRunAvailable(), state,
            {static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())},
            input);
        if (previousFriends != state.friendsLeaderboardVisible ||
            (previousFocus != state.selectedButton && (input.tabStep || input.horizontalStep || input.verticalStep)))
            audio_.playUiSelect();
        return homeResult;
    };
    if (mainMenuPage_ == MainMenuPage::Quests) {
        const bool reverseTab = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const QuestMenuInput questInput{
            .pointer = uiMousePosition(),
            .wheel = GetMouseWheelMove(),
            .click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
            .pointerDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            .selectionStep = IsKeyPressed(KEY_DOWN) ? 1 : IsKeyPressed(KEY_UP) ? -1 : 0,
            .categoryStep = IsKeyPressed(KEY_TAB) ? reverseTab ? -1 : 1 : 0,
            .deltaTime = GetFrameTime(),
        };
        const auto previousSelection = questMenuState_.selectedId;
        const auto previousCategory = questMenuState_.category;
        const bool previousCompletedLast = questMenuState_.completedLast;
        const auto result = drawQuestMenu(ui_, questSystem_, metaProgression_, questMenuState_,
            {static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())}, questInput);
        if (result.purchase || result.rewardClaimed) persistMetaProgression();
        if (result.close) mainMenuPage_ = MainMenuPage::None;
        if (result.purchase) audio_.playUiPurchase();
        else if (result.rewardClaimed) audio_.playUiReward();
        else if (!result.close && (questInput.click || questInput.selectionStep || questInput.categoryStep) &&
            (previousSelection != questMenuState_.selectedId || previousCategory != questMenuState_.category ||
             previousCompletedLast != questMenuState_.completedLast)) audio_.playUiSelect();
        else if (result.close || (result.interaction && questInput.click)) audio_.playUiConfirm();
        return;
    }

    if (mainMenuPage_ == MainMenuPage::Shop) {
        const ShopMenuInput input{
            .pointer = uiMousePosition(),
            .click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
            .confirm = IsKeyPressed(KEY_ENTER),
            .selectionStep = IsKeyPressed(KEY_RIGHT) ? 1 : IsKeyPressed(KEY_LEFT) ? -1
                : IsKeyPressed(KEY_DOWN) ? MetaShopGridColumns : IsKeyPressed(KEY_UP) ? -MetaShopGridColumns : 0,
        };
        const auto previousSelection = shopMenuState_.selected;
        const auto result = drawShopMenu(ui_, metaProgression_, shopMenuState_,
            {static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())}, input);
        if (result.purchase) persistMetaProgression();
        if (result.close) mainMenuPage_ = MainMenuPage::None;
        if (result.openUnlocks) {
            unlockMenuState_ = {};
            unlockMenuState_.category = UnlockCategory::Meta;
            unlockMenuState_.selectedId = std::string(MetaShopDefinitions[static_cast<std::size_t>(shopMenuState_.selected)].requiredUnlock);
            mainMenuPage_ = MainMenuPage::Unlocks;
        }
        if (result.purchase) audio_.playUiPurchase();
        else if (!result.close && !result.openUnlocks && previousSelection != shopMenuState_.selected &&
            (input.click || input.selectionStep)) audio_.playUiSelect();
        else if (result.close || (result.interaction && (input.click || input.confirm))) audio_.playUiConfirm();
        return;
    }

    if (mainMenuPage_ == MainMenuPage::Unlocks) {
        const bool reverseTab = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const UnlockMenuInput input{
            .pointer = uiMousePosition(),
            .wheel = GetMouseWheelMove(),
            .click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
            .pointerDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            .selectionStep = IsKeyPressed(KEY_RIGHT) ? 1 : IsKeyPressed(KEY_LEFT) ? -1
                : IsKeyPressed(KEY_DOWN) ? 9 : IsKeyPressed(KEY_UP) ? -9 : 0,
            .categoryStep = IsKeyPressed(KEY_TAB) ? reverseTab ? -1 : 1 : 0,
        };
        const auto previousSelection = unlockMenuState_.selectedId;
        const auto previousCategory = unlockMenuState_.category;
        const auto result = drawUnlockMenu(ui_, questSystem_, metaProgression_, unlockMenuState_,
            {static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())}, input);
        if (result.purchase || result.toggled) persistMetaProgression();
        if (result.close) mainMenuPage_ = MainMenuPage::None;
        if (!result.shopUnlockId.empty()) {
            for (const auto& upgrade : MetaShopDefinitions)
                if (upgrade.requiredUnlock == result.shopUnlockId) shopMenuState_.selected = upgrade.upgrade;
            mainMenuPage_ = MainMenuPage::Shop;
        }
        if (!result.questId.empty()) {
            questMenuState_ = {};
            questMenuState_.selectedId = result.questId;
            for (const auto& quest : questSystem_.definitions()) {
                if (quest.id == result.questId) questMenuState_.category = quest.category;
            }
            mainMenuPage_ = MainMenuPage::Quests;
        }
        if (result.purchase) audio_.playUiPurchase();
        else if (!result.close && (input.click || input.selectionStep || input.categoryStep) &&
            (previousSelection != unlockMenuState_.selectedId || previousCategory != unlockMenuState_.category))
            audio_.playUiSelect();
        else if (result.close || (result.interaction && input.click)) audio_.playUiConfirm();
        return;
    }

    if (menuBackgroundEditing_) {
        if (menuBackgroundMenuPreviewVisible_) {
            // Render the same live menu with an isolated state and no input.
            auto previewState = mainMenuHomeState_;
            previewState.pendingAction = MainMenuHomeAction::None;
            previewState.pointerWasDown = false;
            static_cast<void>(drawHome(previewState,
                {.pointer = {-1000.0F, -1000.0F}, .deltaTime = GetFrameTime()}));

            return;
        }
        drawMenuPanel(
            layout.rect(260.0F, 835.0F, 1400.0F, 140.0F),
            layout);
        const std::string title = localizeText(
            currentLanguage() == Language::Russian ? "ФОН МЕНЮ" : "MENU BACKGROUND");
        const Vector2 titleSize = measureUiText(
            title, 22.0F * layout.scale);
        drawUiText(
            title,
            {layout.origin.x + 960.0F * layout.scale -
                 titleSize.x * 0.5F,
             layout.origin.y + 849.0F * layout.scale},
            22.0F * layout.scale, MenuCream);
        const auto catalog = menuBackgroundCatalog(menuBackgroundBuildingCategory_);
        const std::string category = localizeText(
            menuBackgroundBuildingCategory_ == 0 ? "BASE" :
            menuBackgroundBuildingCategory_ == 1 ? "DEFENSE" : "CONSTRUCTION");
        const std::string selection = menuBackgroundBuildingEnabled_
            ? category + "  /  " + localizeText(buildingDisplayName(
                catalog[static_cast<std::size_t>(menuBackgroundBuildingSlot_)]))
            : localizeText("CAMERA ONLY");
        const Vector2 selectionSize = measureUiText(selection, 20.0F * layout.scale);
        drawUiText(selection,
            {layout.point(960.0F, 886.0F).x - selectionSize.x * 0.5F,
             layout.point(960.0F, 886.0F).y},
            20.0F * layout.scale, MenuGold);
        std::string slots;
        for (std::size_t index = 0; index < catalog.size(); ++index) {
            if (!slots.empty()) slots += "   |   ";
            slots += std::to_string(index + 1U) + " " +
                localizeText(buildingDisplayName(catalog[index]));
        }
        const float slotSize = fitUiTextSize(slots,
            16.0F * layout.scale, 12.0F * layout.scale, 1320.0F * layout.scale);
        const Vector2 slotBounds = measureUiText(slots, slotSize);
        drawUiText(slots,
            {layout.point(960.0F, 925.0F).x - slotBounds.x * 0.5F,
             layout.point(960.0F, 925.0F).y}, slotSize, MenuCream);
        DrawCircleV(
            {static_cast<float>(uiScreenWidth()) * 0.5F,
             static_cast<float>(uiScreenHeight()) * 0.5F},
            3.0F * layout.scale, MenuCream);
        return;
    }

    if (classSelectionVisible_ || mainMenuPage_ != MainMenuPage::None) {
        const Texture2D logo = ui_.mainMenuLogoTexture();
        if (IsTextureValid(logo)) {
            DrawTexturePro(logo,
                {0.0F, 0.0F, static_cast<float>(logo.width),
                 static_cast<float>(logo.height)},
                layout.rect(618.0F, 24.0F, 684.0F, 235.0F),
                {}, 0.0F, WHITE);
        } else {
            drawOutlinedTitle("FORTBONK", layout,
                960.0F, 82.0F, 62.0F, 820.0F);
        }

    }

    const std::string silverText = std::to_string(
        std::max(0, metaProgression_.silver));
    const float silverFontSize = 22.0F * layout.scale;
    const Vector2 silverTextSize = measureUiText(
        silverText, silverFontSize);
    constexpr float SilverRight = 1884.0F;
    constexpr float SilverIconSize = 33.0F;
    constexpr float SilverGap = 8.0F;
    const float silverTextX = layout.origin.x +
        SilverRight * layout.scale - silverTextSize.x;
    if (classSelectionVisible_ || mainMenuPage_ != MainMenuPage::None) {
        ui_.drawResourceIcon(
            {silverTextX - (SilverIconSize + SilverGap) * layout.scale,
             25.0F * layout.scale,
             SilverIconSize * layout.scale,
             SilverIconSize * layout.scale},
            UiResourceIcon::Silver);
    }
    SdfUiTextStyle silverStyle{};
    silverStyle.fillColor = {250, 245, 224, 255};
    silverStyle.outlineColor = {34, 35, 38, 255};
    silverStyle.outlinePixels = 3.0F;
    silverStyle.shadowColor = {68, 70, 74, 255};
    silverStyle.shadowOffsetPixels = {0.0F, 2.0F * layout.scale};
    if (classSelectionVisible_ || mainMenuPage_ != MainMenuPage::None) {
        drawStyledSdfUiText(
            silverText,
            {silverTextX,
             27.0F * layout.scale},
            silverFontSize, silverStyle);

    }

    if (mainMenuPage_ != MainMenuPage::None) {
        DrawRectangle(
            0, 0, uiScreenWidth(), uiScreenHeight(),
            {6, 9, 13, 205});
        const Rectangle panel =
            layout.rect(445.0F, 190.0F, 1030.0F, 695.0F);
        drawMenuPanel(panel, layout);
        if (mainMenuPage_ == MainMenuPage::Shop) {
            drawOutlinedTitle(
                "SHOP", layout, 960.0F, 222.0F,
                38.0F, 680.0F);
            const float balanceSize = 27.0F * layout.scale;
            const Vector2 balanceTextSize = measureUiText(
                silverText, balanceSize);
            const float balanceGroupWidth =
                45.0F * layout.scale +
                12.0F * layout.scale + balanceTextSize.x;
            const float balanceX = layout.origin.x +
                960.0F * layout.scale -
                balanceGroupWidth * 0.5F;
            ui_.drawResourceIcon(
                {balanceX, layout.origin.y + 373.0F * layout.scale,
                 45.0F * layout.scale, 45.0F * layout.scale},
                UiResourceIcon::Silver);
            drawStyledSdfUiText(
                silverText,
                {balanceX + 57.0F * layout.scale,
                 layout.origin.y + 382.0F * layout.scale},
                balanceSize, silverStyle);
            const std::string shopMessage =
                "META UPGRADES ARE COMING SOON";
            const float fontSize = 19.0F * layout.scale;
            const Vector2 size = measureUiText(shopMessage, fontSize);
            drawUiText(
                shopMessage,
                {layout.origin.x + 960.0F * layout.scale -
                     size.x * 0.5F,
                 layout.origin.y + 478.0F * layout.scale},
                fontSize, {237, 223, 188, 255});
        } else {
            drawOutlinedTitle(
                "CREDITS", layout, 960.0F, 222.0F,
                38.0F, 680.0F);
            constexpr std::array<std::string_view, 5> Credits{{
                "MADE WITH RAYLIB",
                "ASSETS: KENNEY",
                "ASSETS: KAYKIT",
                "FONT: LILITA ONE",
                "EFFECTS: BRACKEYS",
            }};
            for (std::size_t index = 0;
                 index < Credits.size(); ++index) {
                const float fontSize = 15.0F * layout.scale;
                const Vector2 size = measureUiText(
                    Credits[index], fontSize);
                drawUiText(
                    Credits[index],
                    {layout.origin.x +
                         960.0F * layout.scale - size.x * 0.5F,
                     layout.origin.y +
                         (355.0F + static_cast<float>(index) * 60.0F) *
                             layout.scale},
                    fontSize, {237, 223, 188, 255});
            }
        }
        if (drawMenuButton(
                layout.rect(810.0F, 774.0F, 300.0F, 70.0F),
                "BACK", layout)) {
            mainMenuPage_ = MainMenuPage::None;
            audio_.playUiConfirm();
        }
        return;
    }

    const bool acceptsInput = mainMenuInputBarrier_.allowsInput(
        IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_SPACE));
    if (!acceptsInput) {
        mainMenuHomeState_.pendingAction = MainMenuHomeAction::None;
        mainMenuHomeState_.activationRemaining = 0.0F;
        mainMenuHomeState_.pointerWasDown = false;
    }
    const auto navigation = acceptsInput ? readMenuNavigationInput() : MenuNavigationInput{};
    const auto result = drawHome(mainMenuHomeState_,
        {.pointer = uiMousePosition(),
         .click = acceptsInput && IsMouseButtonReleased(MOUSE_BUTTON_LEFT),
         .pointerDown = acceptsInput && IsMouseButtonDown(MOUSE_BUTTON_LEFT),
         .deltaTime = GetFrameTime(),
         .tabStep = navigation.tabStep,
         .horizontalStep = navigation.horizontalStep,
         .verticalStep = navigation.verticalStep,
         .activate = navigation.activate});
    switch (result.action) {
    case MainMenuHomeAction::None: break;
    case MainMenuHomeAction::Play:
        classSelectionState_ = {};
        for (std::size_t i = 0; i < PlayerClassDefinitions.size(); ++i) {
            if (PlayerClassDefinitions[i].type == selectedPlayerClass_) classSelectionState_.focused = static_cast<int>(i);
        }
        classSelectionVisible_ = true;
        classCollectionOnly_ = false;
        sandboxClassSelection_ = false;
        audio_.playUiConfirm();
        break;
    case MainMenuHomeAction::Continue:
        pendingContinueFromUi_ = true;
        break;
    case MainMenuHomeAction::Unlocks:
        mainMenuPage_ = MainMenuPage::Unlocks;
        audio_.playUiConfirm();
        break;
    case MainMenuHomeAction::Quests:
        if (!result.questId.empty()) {
            questMenuState_ = {};
            questMenuState_.selectedId = result.questId;
            for (const auto& definition : questSystem_.definitions()) {
                if (definition.id == result.questId) {
                    questMenuState_.category = definition.category;
                    break;
                }
            }
        }
        mainMenuPage_ = MainMenuPage::Quests;
        audio_.playUiConfirm();
        break;
    case MainMenuHomeAction::Shop:
        mainMenuPage_ = MainMenuPage::Shop;
        audio_.playUiConfirm();
        break;
    case MainMenuHomeAction::Settings:
        openSettings();
        break;
    case MainMenuHomeAction::Credits:
        mainMenuPage_ = MainMenuPage::Credits;
        audio_.playUiConfirm();
        break;
    case MainMenuHomeAction::Exit:
        exitRequested_ = true;
        break;
    }

}

} // namespace ian
