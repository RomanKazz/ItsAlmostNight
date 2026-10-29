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

constexpr Color MenuPanelFill{29, 34, 38, 228};
constexpr Color MenuPanelBorder{116, 79, 48, 255};
constexpr Color MenuCream{250, 241, 211, 255};
constexpr Color MenuGold{255, 205, 105, 255};
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
        GetMousePosition(), bounds);
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

bool drawMainMenuButton(
    Texture2D texture, Rectangle bounds, std::string_view label,
    const MenuLayout& layout, float designFontSize = 56.0F) {
    const bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    const bool pressed = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Rectangle artwork = bounds;
    artwork.y += pressed ? 2.0F * layout.scale : 0.0F;
    if (IsTextureValid(texture)) {
        DrawTexturePro(texture,
            {0.0F, 0.0F, static_cast<float>(texture.width),
             static_cast<float>(texture.height)},
            artwork, {}, 0.0F,
            pressed ? Color{210, 210, 210, 255} : WHITE);
        if (hovered && !pressed) {
            DrawTexturePro(texture,
                {0.0F, 0.0F, static_cast<float>(texture.width),
                 static_cast<float>(texture.height)},
                artwork, {}, 0.0F, {255, 239, 204, 36});
        }
    } else {
        return drawMenuButton(bounds, label, layout);
    }

    const ScopedUiFontRole display(UiFontRole::Display);
    // UiText applies its existing 1.68 scale; Figma specifies actual pixels.
    const float fontSize = fitUiTextSize(label,
        designFontSize / 1.68F * layout.scale,
        14.0F * layout.scale, bounds.width - 44.0F * layout.scale,
        bounds.height - 28.0F * layout.scale);
    const Vector2 measured = measureUiText(label, fontSize);
    SdfUiTextStyle style{};
    style.fillColor = {254, 247, 229, 255};
    style.outlineColor = {20, 20, 16, 255};
    style.outlinePixels = 3.5F * layout.scale;
    style.shadowColor = {8, 9, 10, 230};
    style.shadowOffsetPixels = {0.0F, 2.5F * layout.scale};
    style.softness = 0.6F;
    drawStyledSdfUiText(label,
        {artwork.x + (artwork.width - measured.x) * 0.5F,
         artwork.y + (artwork.height - measured.y) * 0.5F +
             artwork.height * (5.44F / 101.76F)},
        fontSize, style);
    return hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
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

void drawStatRow(
    const MenuLayout& layout, float x, float y,
    float width, std::string_view label,
    std::string value, Color valueColor) {
    const ScopedUiFontRole body(UiFontRole::BodyBold);
    const float valueSize = 18.0F * layout.scale;
    const Vector2 measured = measureUiText(value, valueSize);
    const float labelSize = fitUiTextSize(label, 17.0F * layout.scale,
        12.0F * layout.scale, width * layout.scale - measured.x - 44.0F * layout.scale);
    drawUiText(label, layout.point(x + 8.0F, y + 10.0F),
        labelSize, {216, 219, 200, 255});
    drawUiText(value,
        {layout.point(x + width - 8.0F, y + 9.0F).x - measured.x,
         layout.point(x + width - 8.0F, y + 9.0F).y},
        valueSize, valueColor);
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
    renderer_->setMenuDepthOfField(true, 0.1F, 0.1F, 8.5F);
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
    EndMode3D();
    renderer_->endWorldPass();
    renderer_->setMenuDepthOfField(false);

    // A restrained cool grade keeps the warm focused prop readable while
    // preserving the live world behind the menu.
    DrawRectangle(
        0, 0, GetScreenWidth(), GetScreenHeight(),
        {12, 18, 24, 60});
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
    if (!MinimalWorldVisuals &&
        renderer_->beginFirstPersonToolPass()) {
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

void App::drawMainMenu(const SimulationSnapshot&) {
    if (renderer_->graphicsPanelVisible()) {
        return;
    }
    const MenuLayout layout = menuLayout();
    if (mainMenuPage_ == MainMenuPage::Quests) {
        const bool reverseTab = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const QuestMenuInput questInput{
            .pointer = GetMousePosition(),
            .wheel = GetMouseWheelMove(),
            .click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
            .pointerDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            .selectionStep = IsKeyPressed(KEY_DOWN) ? 1 : IsKeyPressed(KEY_UP) ? -1 : 0,
            .categoryStep = IsKeyPressed(KEY_TAB) ? reverseTab ? -1 : 1 : 0,
            .deltaTime = GetFrameTime(),
        };
        const auto result = drawQuestMenu(ui_, questSystem_, metaProgression_, questMenuState_,
            {static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, questInput);
        if (result.purchase || result.rewardClaimed) persistMetaProgression();
        if (result.close) mainMenuPage_ = MainMenuPage::None;
        if (result.interaction || result.close) audio_.playUiConfirm();
        return;
    }

    if (mainMenuPage_ == MainMenuPage::Shop) {
        const ShopMenuInput input{
            .pointer = GetMousePosition(),
            .click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
            .confirm = IsKeyPressed(KEY_ENTER),
            .selectionStep = IsKeyPressed(KEY_RIGHT) ? 1 : IsKeyPressed(KEY_LEFT) ? -1
                : IsKeyPressed(KEY_DOWN) ? MetaShopGridColumns : IsKeyPressed(KEY_UP) ? -MetaShopGridColumns : 0,
        };
        const auto result = drawShopMenu(ui_, metaProgression_, shopMenuState_,
            {static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())}, input);
        if (result.purchase) persistMetaProgression();
        if (result.close) mainMenuPage_ = MainMenuPage::None;
        if (result.openUnlocks) {
            unlockMenuState_ = {};
            unlockMenuState_.category = UnlockCategory::Meta;
            unlockMenuState_.selectedId = "meta.refresh";
            mainMenuPage_ = MainMenuPage::Unlocks;
        }
        if (result.interaction || result.close) audio_.playUiConfirm();
        return;
    }

    if (mainMenuPage_ == MainMenuPage::Unlocks) {
        const bool reverseTab = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        const UnlockMenuInput input{
            .pointer = GetMousePosition(),
            .wheel = GetMouseWheelMove(),
            .click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
            .pointerDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
            .selectionStep = IsKeyPressed(KEY_RIGHT) ? 1 : IsKeyPressed(KEY_LEFT) ? -1
                : IsKeyPressed(KEY_DOWN) ? 9 : IsKeyPressed(KEY_UP) ? -9 : 0,
            .categoryStep = IsKeyPressed(KEY_TAB) ? reverseTab ? -1 : 1 : 0,
        };
        const auto result = drawUnlockMenu(ui_, questSystem_, metaProgression_, unlockMenuState_,
            {static_cast<float>(uiScreenWidth()), static_cast<float>(uiScreenHeight())}, input);
        if (result.purchase || result.toggled) persistMetaProgression();
        if (result.close) mainMenuPage_ = MainMenuPage::None;
        if (!result.questId.empty()) {
            questMenuState_ = {};
            questMenuState_.selectedId = result.questId;
            for (const auto& quest : questSystem_.definitions()) {
                if (quest.id == result.questId) questMenuState_.category = quest.category;
            }
            mainMenuPage_ = MainMenuPage::Quests;
        }
        if (result.interaction || result.close) audio_.playUiConfirm();
        return;
    }

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

    const auto featured = questSystem_.recommendedStatuses(metaProgression_);

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
            const std::string localizedLoadout =
                localizeText(definition.startingLoadoutLabel);
            const float loadoutSize = fitUiTextSize(
                localizedLoadout,
                7.5F * layout.scale,
                6.0F * layout.scale,
                card.width - 70.0F * layout.scale);
            drawUiText(
                localizedLoadout,
                {card.x + 35.0F * layout.scale,
                 card.y + 191.0F * layout.scale},
                loadoutSize, ClassColors[index]);
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

    const bool acceptsInput = mainMenuInputBarrier_.allowsInput(
        IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsKeyDown(KEY_ENTER) || IsKeyDown(KEY_SPACE));
    const bool canContinue = suspendedRunAvailable();
    for (std::size_t index = 0; index < featured.size(); ++index) {
        if (drawQuestSummaryCard(ui_, featured[index],
                layout.topRect(30.0F, 30.0F + static_cast<float>(index) * 96.0F, 440.0F, 84.0F),
                layout.scale, GetMousePosition(), acceptsInput && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))) {
            questMenuState_ = {};
            questMenuState_.selectedId = featured[index].definition->id;
            mainMenuPage_ = MainMenuPage::Quests;
            audio_.playUiConfirm();
        }
    }
    if (featured.empty()) {
        const ScopedUiFontRole body(UiFontRole::BodyBold);
        const auto rows = questSystem_.statuses(metaProgression_);
        const bool allCompleted = std::ranges::all_of(rows, [](const auto& row) { return row.completed; });
        drawUiText(allCompleted ? "ALL QUESTS COMPLETED" : "NO AVAILABLE QUESTS",
            layout.topPoint(34.0F, 38.0F), 17.0F * layout.scale, MenuCream);
    }

    const Texture2D menuButton = ui_.mainMenuButtonTexture();
    if (canContinue) {
        pendingContinueFromUi_ = (drawMainMenuButton(menuButton,
            layout.rect(815.208F, 320.972F, 285.184F, 81.408F),
            "CONTINUE RUN", layout, 34.0F) && acceptsInput) || pendingContinueFromUi_;
    }
    if (drawMainMenuButton(menuButton,
            layout.rect(781.56F, 414.38F, 356.48F, 101.76F), "PLAY", layout) && acceptsInput) {
        classSelectionVisible_ = true;
        classCollectionOnly_ = false;
        sandboxClassSelection_ = false;
        audio_.playUiConfirm();
    }
    if (drawMainMenuButton(menuButton,
            layout.rect(781.56F, 528.14F, 356.48F, 101.76F), "UNLOCKS", layout) && acceptsInput) {
        mainMenuPage_ = MainMenuPage::Unlocks;
        audio_.playUiConfirm();
    }
    if (drawMainMenuButton(menuButton,
            layout.rect(781.56F, 641.90F, 356.48F, 101.76F), "QUESTS", layout) && acceptsInput) {
        mainMenuPage_ = MainMenuPage::Quests;
        audio_.playUiConfirm();
    }
    if (drawMainMenuButton(menuButton,
            layout.rect(781.56F, 755.66F, 356.48F, 101.76F), "SHOP", layout) && acceptsInput) {
        mainMenuPage_ = MainMenuPage::Shop;
        audio_.playUiConfirm();
    }

    if (drawMainMenuButton(menuButton,
            layout.rect(39.0F, 785.0F, 285.184F, 81.408F), "SETTINGS", layout, 44.8F) && acceptsInput) {
        renderer_->setGraphicsPanelVisible(true);
    }
    if (drawMainMenuButton(menuButton,
            layout.rect(39.0F, 876.008F, 285.184F, 81.408F), "CREDITS", layout, 44.8F) && acceptsInput) {
        mainMenuPage_ = MainMenuPage::Credits;
        audio_.playUiConfirm();
    }
    if (drawMainMenuButton(menuButton,
            layout.rect(39.0F, 967.016F, 285.184F, 81.408F), "EXIT", layout, 44.8F) && acceptsInput) {
        exitRequested_ = true;
    }

    drawMenuPanel(layout.rect(1500.0F, 744.0F, 390.0F, 292.0F), layout);
    drawUiText("RUN RECORDS", layout.point(1524.0F, 765.0F), 16.0F * layout.scale, MenuGold);
    DrawLineEx(layout.point(1524.0F, 803.0F), layout.point(1866.0F, 803.0F),
        1.5F * layout.scale, {155, 116, 75, 180});
    drawStatRow(layout, 1524.0F, 816.0F, 342.0F, "BEST WAVE",
        std::to_string(metaProgression_.bestWave), {250, 206, 71, 255});
    drawStatRow(layout, 1524.0F, 860.0F, 342.0F, "STAGE CLEARS",
        std::to_string(metaProgression_.stageClears), {188, 150, 255, 255});
    drawStatRow(layout, 1524.0F, 904.0F, 342.0F, "ENEMIES DEFEATED",
        std::to_string(metaProgression_.enemiesDefeated), {121, 213, 124, 255});
    drawStatRow(layout, 1524.0F, 948.0F, 342.0F, "ITEMS COLLECTED",
        std::to_string(metaProgression_.lootCollected), {136, 204, 245, 255});

    {
        const ScopedUiFontRole body(UiFontRole::Body);
        const std::string hint = localizeText("ENTER  PLAY     F2  SETTINGS");
        const float fontSize = 15.0F * layout.scale;
        const Vector2 measured = measureUiText(hint, fontSize);
        drawUiText(hint, {layout.point(960.0F, 1032.0F).x - measured.x * 0.5F,
            layout.point(960.0F, 1032.0F).y}, fontSize, {219, 218, 201, 200});
    }

}

} // namespace ian
