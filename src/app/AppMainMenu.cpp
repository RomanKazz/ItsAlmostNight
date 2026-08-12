#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"

#include "localization/Localization.hpp"
#include "ui/UiCString.hpp"
#include "ui/UiText.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

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
};

MenuLayout menuLayout() {
    constexpr float DesignWidth = 1920.0F;
    constexpr float DesignHeight = 1080.0F;
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
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

void drawOutlinedTitle(
    std::string_view sourceText, const MenuLayout& layout,
    float centerX, float y, float preferredSize,
    float maximumWidth) {
    const std::string text = localizeText(sourceText);
    const float logicalSize = fitUiTextSize(
        sourceText, preferredSize, 28.0F,
        maximumWidth * layout.scale);
    const float size = logicalSize * 1.68F * layout.scale;
    const Vector2 measured = withNullTerminatedUiText(
        text, [size](const char* value) {
            return MeasureTextEx(
                uiFont(), value, size, size * 0.02F);
        });
    const Vector2 position{
        layout.origin.x + centerX * layout.scale -
            measured.x * 0.5F,
        layout.origin.y + y * layout.scale,
    };
    constexpr std::array<Vector2, 8> OutlineOffsets{{
        {-3.0F, -3.0F}, {0.0F, -4.0F}, {3.0F, -3.0F},
        {-4.0F, 0.0F},                    {4.0F, 0.0F},
        {-3.0F, 3.0F},  {0.0F, 4.0F},  {3.0F, 3.0F},
    }};
    withNullTerminatedUiText(text, [&](const char* value) {
        for (const Vector2 offset : OutlineOffsets) {
            DrawTextEx(
                uiFont(), value,
                {position.x + offset.x * layout.scale,
                 position.y + offset.y * layout.scale},
                size, size * 0.02F, {9, 12, 15, 255});
        }
        DrawTextEx(
            uiFont(), value, position, size,
            size * 0.02F, {250, 245, 224, 255});
    });
}

void drawMenuBadge(
    const MenuLayout& layout, Rectangle button, int amount) {
    if (amount <= 0) {
        return;
    }
    const float radius = 17.0F * layout.scale;
    const Vector2 center{
        button.x + button.width - 16.0F * layout.scale,
        button.y + 12.0F * layout.scale,
    };
    DrawCircleV(center, radius + 3.0F * layout.scale,
                {46, 35, 20, 255});
    DrawCircleV(center, radius, {247, 193, 45, 255});
    const std::string label = amount > 9
        ? "9+" : std::to_string(amount);
    const float fontSize = 12.0F * layout.scale;
    const Vector2 size = measureUiText(label, fontSize);
    drawUiText(
        label,
        {center.x - size.x * 0.5F,
         center.y - size.y * 0.5F - 1.0F * layout.scale},
        fontSize, {51, 35, 13, 255});
}

void drawObjectiveCard(
    const GameUi& ui, const MenuLayout& layout,
    Rectangle bounds, const ObjectiveStatus& objective,
    std::size_t index) {
    ui.drawInsetPanel(bounds, 242);
    const std::array<Color, 4> iconColors{{
        {219, 76, 78, 255}, {91, 167, 222, 255},
        {238, 184, 57, 255}, {116, 190, 103, 255},
    }};
    const float iconSize = 40.0F * layout.scale;
    const Vector2 iconCenter{
        bounds.x + 31.0F * layout.scale,
        bounds.y + 34.0F * layout.scale,
    };
    DrawPoly(
        iconCenter, 6, iconSize * 0.5F, 30.0F,
        {26, 31, 34, 255});
    DrawPoly(
        iconCenter, 6, iconSize * 0.39F, 30.0F,
        iconColors[index % iconColors.size()]);

    const float textX = bounds.x + 62.0F * layout.scale;
    const float textWidth = bounds.width - 78.0F * layout.scale;
    const float titleSize = fitUiTextSize(
        objective.definition.title, 13.0F * layout.scale,
        8.0F * layout.scale, textWidth,
        25.0F * layout.scale);
    drawUiText(
        objective.definition.title,
        {textX, bounds.y + 10.0F * layout.scale},
        titleSize, {250, 216, 100, 255});

    const int progress = static_cast<int>(std::floor(
        std::min(objective.progress, objective.definition.target)));
    const int target = std::max(
        1, static_cast<int>(std::ceil(objective.definition.target)));
    const std::string counter =
        std::to_string(progress) + " / " + std::to_string(target);
    drawUiText(
        counter,
        {textX, bounds.y + 39.0F * layout.scale},
        10.0F * layout.scale, {226, 226, 210, 255});
    const float fraction = objective.completed
        ? 1.0F
        : static_cast<float>(std::clamp(
              objective.progress / objective.definition.target,
              0.0, 1.0));
    ui.drawProgressBar(
        {textX, bounds.y + 65.0F * layout.scale,
         textWidth, 12.0F * layout.scale},
        fraction,
        objective.completed ? UiBarColor::Yellow
                            : UiBarColor::Green);
}

void drawStatRow(
    const MenuLayout& layout, float x, float y,
    float width, std::string_view label,
    std::string value, Color valueColor) {
    DrawRectangleRec(
        layout.rect(x, y, width, 46.0F),
        {25, 31, 31, 178});
    drawUiText(
        label, layout.point(x + 14.0F, y + 10.0F),
        12.0F * layout.scale, {216, 219, 200, 255});
    const Vector2 measured = measureUiText(
        value, 13.0F * layout.scale);
    drawUiText(
        value,
        {layout.origin.x + (x + width - 14.0F) * layout.scale -
             measured.x,
         layout.origin.y + (y + 9.0F) * layout.scale},
        13.0F * layout.scale, valueColor);
}

} // namespace

void App::drawMainMenuWorld(
    const SimulationSnapshot& snapshot) {
    const double terrainY = simulation_.terrain().getHeight(
        snapshot.playerPosition.x, snapshot.playerPosition.z);
    const float drift = static_cast<float>(
        std::sin(GetTime() * 0.12) * 1.6);
    const Camera3D camera{
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

    environment_.setAutomaticTime(0.22F);
    const EnvironmentState environment = environment_.state();
    const Vector3 forward = Vector3Normalize(
        Vector3Subtract(camera.target, camera.position));
    const Vector3 cameraRight = Vector3Normalize(
        Vector3CrossProduct(forward, {0.0F, 1.0F, 0.0F}));
    const Vector3 cameraUp = Vector3Normalize(
        Vector3CrossProduct(cameraRight, forward));
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
    renderer_->beginWorldShader(lighting);
    WorldMaterialState pondMaterial{};
    pondMaterial.bakedAo = 0.8F;
    pondMaterial.screenAoAmount = 0.0F;
    renderer_->setWorldMaterial(pondMaterial);
    renderer_->drawPondShoreRocks();
    renderer_->drawPondDecor();
    renderer_->endWorldShader();
    renderer_->drawWater(camera.position, lighting);
    renderer_->drawClouds(
        camera.position, environment.nightFactor, lighting);
    drawBlobShadows(snapshot, camera);
    EndMode3D();
    renderer_->endWorldPass();

    // Cool veil separates the UI hierarchy while leaving the live world
    // recognizable. Extra edge shading imitates shallow depth-of-field
    // composition without adding a costly blur pass to the menu.
    DrawRectangle(
        0, 0, GetScreenWidth(), GetScreenHeight(),
        {9, 55, 68, 92});
    DrawRectangleGradientH(
        0, 0, GetScreenWidth() / 3, GetScreenHeight(),
        {8, 17, 20, 104}, {8, 17, 20, 0});
    DrawRectangleGradientH(
        GetScreenWidth() * 2 / 3, 0,
        GetScreenWidth() / 3 + 1, GetScreenHeight(),
        {8, 17, 20, 0}, {8, 17, 20, 104});
}

void App::drawMainMenu(const SimulationSnapshot& snapshot) {
    if (renderer_->graphicsPanelVisible() || skillTree_.isOpen()) {
        return;
    }
    const MenuLayout layout = menuLayout();

    drawOutlinedTitle(
        "IT'S ALMOST NIGHT", layout,
        960.0F, 76.0F, 58.0F, 720.0F);
    drawUiText(
        "BUILD BY DAY. SURVIVE THE NIGHT.",
        layout.point(782.0F, 190.0F),
        14.0F * layout.scale, {231, 210, 157, 235});

    std::vector<const ObjectiveStatus*> featured;
    featured.reserve(4);
    for (const int objectiveIndex : snapshot.recommendedObjectives) {
        if (objectiveIndex < 0 ||
            static_cast<std::size_t>(objectiveIndex) >=
                snapshot.objectives.size()) {
            continue;
        }
        featured.push_back(
            &snapshot.objectives[
                static_cast<std::size_t>(objectiveIndex)]);
    }
    for (const ObjectiveStatus& objective : snapshot.objectives) {
        if (featured.size() >= 4U) {
            break;
        }
        if (!objective.active || objective.completed ||
            std::ranges::find(featured, &objective) != featured.end()) {
            continue;
        }
        featured.push_back(&objective);
    }

    drawUiText(
        "ACTIVE OBJECTIVES", layout.point(38.0F, 35.0F),
        16.0F * layout.scale, {250, 224, 151, 255});
    for (std::size_t index = 0; index < featured.size(); ++index) {
        drawObjectiveCard(
            ui_, layout,
            layout.rect(
                30.0F, 72.0F + static_cast<float>(index) * 112.0F,
                382.0F, 98.0F),
            *featured[index], index);
    }

    const Rectangle playButton =
        layout.rect(775.0F, 360.0F, 370.0F, 78.0F);
    pendingStartFromUi_ =
        ui_.drawButton(playButton, "START RUN") ||
        pendingStartFromUi_;
    const Rectangle treeButton =
        layout.rect(775.0F, 458.0F, 370.0F, 70.0F);
    pendingOpenSkillTreeFromUi_ =
        ui_.drawButton(treeButton, "TREE OF KNOWLEDGE") ||
        pendingOpenSkillTreeFromUi_;
    drawMenuBadge(layout, treeButton, snapshot.skillPoints);
    if (ui_.drawButton(
            layout.rect(775.0F, 548.0F, 370.0F, 70.0F),
            "SETTINGS")) {
        renderer_->setGraphicsPanelVisible(true);
    }

    if (ui_.drawButton(
            layout.rect(30.0F, 720.0F, 230.0F, 62.0F),
            std::string("LANGUAGE: ") +
                std::string(languageName(userSettings_.language)))) {
        userSettings_.language =
            userSettings_.language == Language::English
                ? Language::Russian
                : Language::English;
        setLanguage(userSettings_.language);
        persistUserSettings(true);
    }
    if (ui_.drawButton(
            layout.rect(30.0F, 798.0F, 230.0F, 62.0F),
            "EXIT GAME")) {
        exitRequested_ = true;
    }

    const Rectangle records =
        layout.rect(1515.0F, 350.0F, 375.0F, 408.0F);
    ui_.drawPanel(records, 240);
    drawUiText(
        "RUN RECORDS", layout.point(1550.0F, 374.0F),
        19.0F * layout.scale, {250, 224, 151, 255});
    DrawLineEx(
        layout.point(1540.0F, 420.0F),
        layout.point(1865.0F, 420.0F),
        2.0F * layout.scale, {170, 146, 105, 210});
    const int completedObjectives = static_cast<int>(
        std::ranges::count_if(
            snapshot.objectives,
            [](const ObjectiveStatus& objective) {
                return objective.completed;
            }));
    drawStatRow(
        layout, 1540.0F, 444.0F, 325.0F,
        "BEST WAVE", std::to_string(snapshot.bestWave),
        {250, 206, 71, 255});
    drawStatRow(
        layout, 1540.0F, 500.0F, 325.0F,
        "SKILL POINTS", std::to_string(snapshot.skillPoints),
        {188, 150, 255, 255});
    drawStatRow(
        layout, 1540.0F, 556.0F, 325.0F,
        "OBJECTIVES", std::to_string(completedObjectives),
        {121, 213, 124, 255});
    drawStatRow(
        layout, 1540.0F, 612.0F, 325.0F,
        "TOTAL INSIGHT",
        std::to_string(static_cast<int>(
            std::lround(snapshot.totalInsightEarned))),
        {136, 204, 245, 255});
    drawUiText(
        "YOUR PROGRESS IS SAVED BETWEEN RUNS",
        layout.point(1543.0F, 688.0F),
        10.0F * layout.scale, {197, 193, 169, 220});

    drawUiText(
        "ENTER  PLAY     F2  SETTINGS     K  SKILLS",
        layout.point(754.0F, 1018.0F),
        11.0F * layout.scale, {222, 217, 191, 225});
}

} // namespace ian
