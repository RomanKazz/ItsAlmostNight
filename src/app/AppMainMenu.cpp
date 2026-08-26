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
    // The live landscape is the deliberately soft backdrop. The hero prop
    // is rendered in a separate transparent foreground pass below, so it
    // can never disappear behind random world generation.
    renderer_->setMenuDepthOfField(true, 0.1F, 0.1F, 3.5F);
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
    renderer_->drawPondShoreRocks(camera);
    renderer_->drawPondDecor(camera);
    renderer_->endWorldShader();
    renderer_->drawWater(camera.position, lighting);
    renderer_->drawClouds(
        camera, environment.nightFactor, lighting);
    drawBlobShadows(snapshot, camera);
    EndMode3D();
    renderer_->endWorldPass();
    renderer_->setMenuDepthOfField(false);

    // A restrained cool grade keeps the warm focused prop readable while
    // preserving the live world behind the menu.
    DrawRectangle(
        0, 0, GetScreenWidth(), GetScreenHeight(),
        {8, 43, 52, 46});
    DrawRectangleGradientH(
        0, 0, GetScreenWidth() / 3, GetScreenHeight(),
        {8, 17, 20, 82}, {8, 17, 20, 0});
    DrawRectangleGradientH(
        GetScreenWidth() * 2 / 3, 0,
        GetScreenWidth() / 3 + 1, GetScreenHeight(),
        {8, 17, 20, 0}, {8, 17, 20, 82});

    // Crisp foreground showcase, analogous to Megabonk's microwave. It is
    // composited after background post-processing and owns an independent
    // depth buffer, producing stable framing for every generated world.
    if (renderer_->beginFirstPersonToolPass()) {
        const Camera3D showcaseCamera{
            .position = {5.0F, 4.0F, 8.0F},
            .target = {1.8F, 2.6F, 0.0F},
            .up = {0.0F, 1.0F, 0.0F},
            .fovy = 31.0F,
            .projection = CAMERA_PERSPECTIVE,
        };
        WorldLighting showcaseLighting = lighting;
        showcaseLighting.cameraPosition = showcaseCamera.position;
        showcaseLighting.fogStart = 1000.0F;
        showcaseLighting.fogEnd = 1001.0F;
        BeginMode3D(showcaseCamera);
        if (renderer_->beginBlobShadowBatch(
                showcaseCamera.position)) {
            renderer_->drawBlobShadow(
                {0.0F, 0.025F, 0.0F},
                1.95F, 1.65F, 0.38F, 24);
            renderer_->endBlobShadowBatch();
        }
        renderer_->beginWorldShader(showcaseLighting);
        WorldMaterialState showcaseMaterial{};
        showcaseMaterial.bakedAo = 0.84F;
        showcaseMaterial.screenAoAmount = 0.0F;
        renderer_->setWorldMaterial(showcaseMaterial);
        static_cast<void>(renderer_->drawPlatformFrameModel(
            {0.0F, 0.0F, 0.0F}, WHITE, 1.35F,
            {0.52F, 0.52F, 0.52F, 0.52F}));
        constexpr float ShowcaseYaw = 2.56F;
        const float turretSweep = static_cast<float>(
            std::sin(GetTime() * 0.42) * 0.11);
        static_cast<void>(renderer_->drawGunTurret(
            {0.0F, 0.08F, 0.0F},
            ShowcaseYaw, ShowcaseYaw + turretSweep,
            WHITE, 1.58F));
        renderer_->endWorldShader();
        EndMode3D();
        FirstPersonToolTuning showcaseComposite{};
        showcaseComposite.outlineEnabled = true;
        showcaseComposite.outlineWidth = 2.2F;
        showcaseComposite.outlineStrength = 0.34F;
        showcaseComposite.rimStrength = 0.18F;
        showcaseComposite.brightness = 1.04F;
        showcaseComposite.saturation = 1.0F;
        renderer_->endFirstPersonToolPass(showcaseComposite);
    }
}

void App::drawMainMenu(const SimulationSnapshot& snapshot) {
    if (renderer_->graphicsPanelVisible()) {
        return;
    }
    const MenuLayout layout = menuLayout();

    drawOutlinedTitle(
        "FORTBONK", layout,
        960.0F, 76.0F, 58.0F, 720.0F);
    drawUiText(
        "BUILD. DEFEND. BONK.",
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

    if (classSelectionVisible_) {
        DrawRectangle(
            0, 0, GetScreenWidth(), GetScreenHeight(),
            {6, 9, 13, 224});
        const Rectangle panel =
            layout.rect(100.0F, 110.0F, 1720.0F, 850.0F);
        ui_.drawPanel(panel, 250);
        drawOutlinedTitle(
            classCollectionOnly_
                ? "CLASS COLLECTION"
                : sandboxClassSelection_
                    ? "CHOOSE SANDBOX CLASS"
                    : "CHOOSE YOUR CLASS",
            layout, 960.0F, 142.0F, 34.0F, 900.0F);
        drawUiText(
            classCollectionOnly_
                ? "COMPLETE CHALLENGES TO UNLOCK NEW PLAYSTYLES"
                : "EACH CLASS CHANGES THE WHOLE RUN",
            layout.point(classCollectionOnly_ ? 678.0F : 744.0F, 222.0F),
            12.0F * layout.scale,
            {218, 204, 174, 235});

        constexpr std::array<Color, 8> ClassColors{{
            {86, 174, 225, 255},
            {231, 117, 66, 255},
            {238, 191, 64, 255},
            {104, 199, 112, 255},
            {222, 75, 61, 255},
            {173, 75, 202, 255},
            {76, 201, 168, 255},
            {103, 151, 238, 255},
        }};
        for (std::size_t index = 0;
             index < PlayerClassDefinitions.size(); ++index) {
            const PlayerClassDefinition& definition =
                PlayerClassDefinitions[index];
            const bool unlocked = sandboxClassSelection_ ||
                isPlayerClassUnlocked(
                    definition.type, metaProgression_);
            const float column = static_cast<float>(index % 4U);
            const float row = static_cast<float>(index / 4U);
            const Rectangle card = layout.rect(
                150.0F + column * 405.0F,
                270.0F + row * 238.0F, 375.0F, 214.0F);
            const bool selected =
                selectedPlayerClass_ == definition.type;
            const bool hovered = CheckCollisionPointRec(
                GetMousePosition(), card);
            if (unlocked && hovered &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                selectedPlayerClass_ = definition.type;
                audio_.playUiConfirm();
            }
            DrawRectangleRounded(
                card, 0.09F, 8,
                selected && unlocked
                    ? Color{46, 43, 35, 252}
                    : hovered
                        ? Color{35, 38, 42, 248}
                        : Color{25, 28, 32, 245});
            DrawRectangleRoundedLinesEx(
                card, 0.09F, 8,
                (selected && unlocked ? 4.0F : 2.0F) * layout.scale,
                selected && unlocked
                    ? ClassColors[index]
                    : Color{106, 103, 91, 220});
            const Vector2 emblem{
                card.x + card.width * 0.5F,
                card.y + 39.0F * layout.scale};
            DrawCircleV(
                emblem, 23.0F * layout.scale,
                ColorAlpha(ClassColors[index], 0.24F));
            DrawCircleLinesV(
                emblem, 23.0F * layout.scale,
                ClassColors[index]);
            const std::string initial{
                definition.name.substr(0, 1)};
            const float initialSize = 18.0F * layout.scale;
            const Vector2 initialBounds =
                measureUiText(initial, initialSize);
            drawUiText(
                initial,
                {emblem.x - initialBounds.x * 0.5F,
                 emblem.y - initialBounds.y * 0.5F},
                initialSize, ClassColors[index]);
            const float nameSize = 16.0F * layout.scale;
            const std::string localizedName =
                localizeText(definition.name);
            const Vector2 nameBounds = measureUiText(
                localizedName, nameSize);
            drawUiText(
                localizedName,
                {card.x + (card.width - nameBounds.x) * 0.5F,
                 card.y + 69.0F * layout.scale},
                nameSize, {250, 239, 207, 255});
            const float roleSize = 9.0F * layout.scale;
            const std::string localizedRole =
                localizeText(definition.role);
            const Vector2 roleBounds = measureUiText(
                localizedRole, roleSize);
            drawUiText(
                localizedRole,
                {card.x + (card.width - roleBounds.x) * 0.5F,
                 card.y + 96.0F * layout.scale},
                roleSize, {190, 188, 177, 235});
            for (std::size_t trait = 0;
                 trait < definition.traits.size(); ++trait) {
                const std::string_view text = definition.traits[trait];
                drawUiText(
                    text,
                    {card.x + 35.0F * layout.scale,
                     card.y +
                         (121.0F + static_cast<float>(trait) * 22.0F) *
                             layout.scale},
                    8.5F * layout.scale,
                    text.starts_with('-')
                        ? Color{242, 112, 91, 255}
                        : Color{116, 220, 132, 255});
            }
            const std::string localizedNodes =
                localizeText(definition.startingNodesLabel);
            const float nodesSize = fitUiTextSize(
                localizedNodes,
                7.5F * layout.scale,
                6.0F * layout.scale,
                card.width - 70.0F * layout.scale);
            drawUiText(
                localizedNodes,
                {card.x + 35.0F * layout.scale,
                 card.y + 191.0F * layout.scale},
                nodesSize, ClassColors[index]);
            if (!unlocked) {
                DrawRectangleRounded(
                    card, 0.09F, 8, {7, 9, 12, 218});
                drawUiText(
                    "LOCKED",
                    {card.x + 145.0F * layout.scale,
                     card.y + 76.0F * layout.scale},
                    16.0F * layout.scale, {194, 190, 178, 255});
                const std::string requirement = localizeText(
                    playerClassUnlockRequirement(definition.type));
                const Vector2 requirementSize = measureUiText(
                    requirement, 9.0F * layout.scale);
                drawUiText(
                    requirement,
                    {card.x + (card.width - requirementSize.x) * 0.5F,
                     card.y + 119.0F * layout.scale},
                    9.0F * layout.scale, {238, 190, 83, 255});
                ui_.drawProgressBar(
                    {card.x + 48.0F * layout.scale,
                     card.y + 158.0F * layout.scale,
                     card.width - 96.0F * layout.scale,
                     11.0F * layout.scale},
                    playerClassUnlockProgress(
                        definition.type, metaProgression_),
                    UiBarColor::Yellow);
            }
        }
        if (!classCollectionOnly_) {
            pendingStartFromUi_ = ui_.drawButton(
                layout.rect(690.0F, 776.0F, 540.0F, 72.0F),
                sandboxClassSelection_
                    ? "START SANDBOX"
                    : "START AS SELECTED CLASS") || pendingStartFromUi_;
        }
        drawUiText(
            classCollectionOnly_
                ? "ESC  BACK"
                : "ESC  BACK     LEFT / RIGHT  SELECT     ENTER  START",
            layout.point(classCollectionOnly_ ? 915.0F : 692.0F, 873.0F),
            10.0F * layout.scale, {201, 195, 174, 220});
        return;
    }

    const bool canContinue = suspendedRunAvailable();
    float menuButtonY = canContinue ? 286.0F : 360.0F;
    if (canContinue) {
        pendingContinueFromUi_ = ui_.drawButton(
            layout.rect(775.0F, menuButtonY, 370.0F, 70.0F),
            "CONTINUE RUN") || pendingContinueFromUi_;
        menuButtonY += 84.0F;
    }
    const Rectangle playButton =
        layout.rect(775.0F, menuButtonY, 370.0F, 70.0F);
    if (ui_.drawButton(playButton, "START RUN")) {
        classSelectionVisible_ = true;
        classCollectionOnly_ = false;
        sandboxClassSelection_ = false;
        audio_.playUiConfirm();
    }
    menuButtonY += 84.0F;
    const Rectangle sandboxButton =
        layout.rect(775.0F, menuButtonY, 370.0F, 70.0F);
    if (ui_.drawButton(sandboxButton, "SANDBOX")) {
        classSelectionVisible_ = true;
        classCollectionOnly_ = false;
        sandboxClassSelection_ = true;
        audio_.playUiConfirm();
    }
    menuButtonY += 84.0F;
    const Rectangle collectionButton =
        layout.rect(775.0F, menuButtonY, 370.0F, 70.0F);
    if (ui_.drawButton(collectionButton, "COLLECTION")) {
        classSelectionVisible_ = true;
        classCollectionOnly_ = true;
        sandboxClassSelection_ = false;
        audio_.playUiConfirm();
    }
    if (ui_.drawButton(
            layout.rect(775.0F, menuButtonY + 84.0F, 370.0F, 70.0F),
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
    drawStatRow(
        layout, 1540.0F, 444.0F, 325.0F,
        "BEST WAVE", std::to_string(metaProgression_.bestWave),
        {250, 206, 71, 255});
    drawStatRow(
        layout, 1540.0F, 500.0F, 325.0F,
        "STAGE CLEARS", std::to_string(metaProgression_.stageClears),
        {188, 150, 255, 255});
    drawStatRow(
        layout, 1540.0F, 556.0F, 325.0F,
        "ENEMIES DEFEATED", std::to_string(metaProgression_.enemiesDefeated),
        {121, 213, 124, 255});
    drawStatRow(
        layout, 1540.0F, 612.0F, 325.0F,
        "ITEMS COLLECTED", std::to_string(metaProgression_.lootCollected),
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
