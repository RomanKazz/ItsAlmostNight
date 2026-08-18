#include "app/App.hpp"
#include "app/AppRenderSupport.hpp"
#include "buildings/BuildingOrientation.hpp"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace ian {

using namespace app_detail;

void App::drawSoldBuildingVisuals() {
    BeginBlendMode(BLEND_ALPHA);
    for (const SoldBuildingVisual& visual :
         soldBuildingVisuals_) {
        const float progress = std::clamp(
            static_cast<float>(
                1.0 -
                visual.remaining / visual.duration),
            0.0F, 1.0F);
        const float collapse =
            1.0F - smoothstep(0.08F, 1.0F, progress);
        const float scale =
            collapse *
            (1.0F +
             std::sin(progress * PI) * 0.09F);
        const float sink =
            progress * progress * 0.42F;
        const float fade =
            1.0F - smoothstep(0.45F, 1.0F, progress);
        const auto alpha = static_cast<unsigned char>(
            std::lround(fade * 230.0F));
        const Color tint{255, 222, 174, alpha};
        if (!visual.building) {
            const EntityId id =
                visual.platformFrame
                    ? visual.platformFrame->id
                    : (visual.modularWall
                           ? visual.modularWall->id
                           : visual.ramp->id);
            const std::array<ModularAnimationScale, 1>
                scales{{
                    {
                        .id = id,
                        .scale = scale,
                    },
                }};
            const double cellSize =
                simulation_.terrain().config().cellSize;
            rlPushMatrix();
            rlTranslatef(0.0F, -sink, 0.0F);
            modularBuildingRenderer_.drawWorld(
                {
                    visual.platformFrame
                        ? std::span<
                              const PlatformFrameInstance>{
                              &*visual.platformFrame, 1U}
                        : std::span<
                              const PlatformFrameInstance>{},
                    visual.modularWall
                        ? std::span<const WallInstance>{
                              &*visual.modularWall, 1U}
                        : std::span<const WallInstance>{},
                    visual.ramp
                        ? std::span<const RampInstance>{
                              &*visual.ramp, 1U}
                        : std::span<const RampInstance>{},
                    {},
                    cellSize,
                    std::nullopt,
                    scales,
                    fade,
                },
                {});
            rlPopMatrix();
            continue;
        }
        const BuildingInstance& building =
            *visual.building;
        const Vec3 center = buildingWorldPosition(building);
        const float x = static_cast<float>(center.x);
        const float baseY =
            static_cast<float>(center.y);
        const float z = static_cast<float>(center.z);
        const float yaw = static_cast<float>(
            buildingRotationYaw(
                building.type, building.rotation));
        const auto scaledPosition =
            [x, baseY = static_cast<float>(center.y),
             z, scale, sink](
                float offsetX, float y, float offsetZ) {
                return Vector3{
                    x + offsetX * scale,
                    baseY + y * scale - sink,
                    z + offsetZ * scale,
                };
            };
        const auto drawCube =
            [&scaledPosition, scale, alpha](
                float offsetX, float y, float offsetZ,
                float width, float height, float depth,
                Color color) {
                color.a = alpha;
                DrawCube(
                    scaledPosition(offsetX, y, offsetZ),
                    width * scale, height * scale,
                    depth * scale, color);
            };

        WorldMaterialState material{};
        material.bakedAo = 0.72F;
        renderer_->setWorldMaterial(material);
        if (const auto foundation =
                automaticBuildingFoundation(
                    building.type,
                    building.gridPosition,
                    building.baseHeight,
                    building.foundationBottomHeight,
                    simulation_.terrain()
                        .config().cellSize,
                    building.id)) {
            const std::array<
                ModularAnimationScale, 1>
                foundationScale{{
                    {
                        .id = building.id,
                        .scale = scale,
                    },
                }};
            rlPushMatrix();
            rlTranslatef(0.0F, -sink, 0.0F);
            modularBuildingRenderer_.drawWorld(
                {
                    std::span<
                        const PlatformFrameInstance>{
                        &*foundation, 1U},
                    {}, {}, {},
                    simulation_.terrain()
                        .config().cellSize,
                    std::nullopt,
                    foundationScale,
                    fade,
                },
                {});
            rlPopMatrix();
        }
        if (building.type == BuildingType::Core) {
            if (!renderer_->drawCore(
                    {x, baseY - sink, z},
                    yaw, tint, scale)) {
                drawCube(0.0F, 1.25F, 0.0F, 2.0F,
                         2.5F, 2.0F,
                         {219, 151, 60, alpha});
            }
        } else if (building.type == BuildingType::Turret) {
            if (!renderer_->drawCrossbow(
                    {x, baseY - sink, z},
                    yaw, tint, scale)) {
                drawCube(0.0F, 0.6F, 0.0F, 1.0F,
                         1.2F, 1.0F,
                         {68, 83, 96, alpha});
                DrawCylinder(
                    scaledPosition(0.0F, 1.45F, 0.0F),
                    0.42F * scale, 0.32F * scale,
                    0.7F * scale, 8,
                    {176, 128, 60, alpha});
            }
        } else if (building.type == BuildingType::GunTurret) {
            static_cast<void>(renderer_->drawGunTurret(
                {x, baseY - sink, z}, yaw, yaw, tint, scale));
        } else if (
            building.type == BuildingType::CrystalMine ||
            building.type == BuildingType::LumberMill ||
            building.type == BuildingType::Quarry) {
            if (!renderer_->drawResourceProducer(
                    building.type,
                    {x, baseY - sink, z},
                    yaw, tint, scale)) {
                drawCube(0.0F, 0.275F, 0.0F, 1.0F,
                         0.55F, 1.0F,
                         {82, 101, 142, alpha});
            }
        } else if (building.type == BuildingType::Cannon) {
            if (!renderer_->drawCannon(
                    {x, baseY - sink, z},
                    yaw, 0.0F,
                    tint, scale)) {
                drawCube(0.0F, 0.6F, 0.0F, 1.0F,
                         1.2F, 1.0F,
                         {62, 70, 78, alpha});
                DrawSphere(
                    scaledPosition(0.0F, 1.35F, 0.0F),
                    0.48F * scale,
                    {83, 91, 99, alpha});
            }
        } else if (building.type == BuildingType::Catapult) {
            if (!renderer_->drawCatapult(
                    {x, baseY - sink, z}, yaw, 0.0F,
                    true, tint, scale)) {
                drawCube(0.0F, 0.45F, 0.0F, 1.2F,
                         0.9F, 1.2F, {72, 66, 58, alpha});
            }
        } else if (building.type ==
                   BuildingType::SlowTrap) {
            drawCube(0.0F, 0.08F, 0.0F, 1.0F,
                     0.16F, 1.0F,
                     {76, 110, 132, alpha});
        } else if (building.type == BuildingType::SpikeTrap) {
            if (!renderer_->drawSpikeTrap(
                    {x, baseY - sink, z}, yaw, -1.0F,
                    tint, scale)) {
                drawCube(0.0F, 0.08F, 0.0F, 1.0F,
                         0.16F, 1.0F,
                         {112, 96, 80, alpha});
            }
        } else if (
            building.type == BuildingType::WoodStorage ||
            building.type == BuildingType::StoneStorage ||
            building.type == BuildingType::CrystalStorage) {
            const Color storageColor =
                building.type == BuildingType::WoodStorage
                    ? Color{142, 91, 48, alpha}
                    : building.type == BuildingType::StoneStorage
                        ? Color{104, 112, 122, alpha}
                        : Color{92, 104, 184, alpha};
            drawCube(0.0F, 0.75F, 0.0F, 1.8F,
                     1.5F, 1.8F, storageColor);
            drawCube(0.0F, 1.62F, 0.0F, 1.25F,
                     0.24F, 1.25F,
                     {224, 195, 123, alpha});
        } else if (building.type == BuildingType::Wall) {
            if (!renderer_->drawWall(
                    {x, baseY - sink, z},
                    visual.wallConnections, yaw,
                    {255, 255, 255, alpha}, scale)) {
                drawCube(0.0F, 1.0F, 0.0F, 1.0F,
                         2.0F, 1.0F,
                         {126, 86, 54, alpha});
            }
        } else if ((building.rotation % 2U) == 0U) {
            drawCube(-0.38F, 1.0F, 0.0F, 0.22F,
                     2.0F, 1.0F,
                     {112, 76, 48, alpha});
            drawCube(0.38F, 1.0F, 0.0F, 0.22F,
                     2.0F, 1.0F,
                     {112, 76, 48, alpha});
            if (!building.open) {
                drawCube(0.0F, 1.0F, 0.0F, 0.55F,
                         1.7F, 0.18F,
                         {151, 105, 62, alpha});
            }
        } else {
            drawCube(0.0F, 1.0F, -0.38F, 1.0F,
                     2.0F, 0.22F,
                     {112, 76, 48, alpha});
            drawCube(0.0F, 1.0F, 0.38F, 1.0F,
                     2.0F, 0.22F,
                     {112, 76, 48, alpha});
            if (!building.open) {
                drawCube(0.0F, 1.0F, 0.0F, 0.18F,
                         1.7F, 0.55F,
                         {151, 105, 62, alpha});
            }
        }
    }
    EndBlendMode();
}

void App::drawBlobShadows(
    const SimulationSnapshot& snapshot, const Camera3D& camera) {
    const auto blobShadowStart = PerformanceClock::now();
    performanceStats_.enemyShadowDraws = 0U;
    if (renderer_->beginBlobShadowBatch(camera.position)) {
        const float maximumAoDistance =
            renderer_->settings().shadowDistance + 24.0F;
        const float maximumAoDistanceSquared =
            maximumAoDistance * maximumAoDistance;
        for (const auto& node : snapshot.resourceNodes) {
            if (!node.active) {
                continue;
            }
            const float offsetX =
                static_cast<float>(node.position.x) - camera.position.x;
            const float offsetZ =
                static_cast<float>(node.position.z) - camera.position.z;
            if (offsetX * offsetX + offsetZ * offsetZ >
                maximumAoDistanceSquared) {
                continue;
            }
            float radius =
                std::max(static_cast<float>(node.radius), 0.45F);
            if (node.type == ResourceType::Wood) {
                radius *= 1.28F *
                    static_cast<float>(node.visualScale);
            } else if (node.type == ResourceType::Stone) {
                // The authored rocks are wider than their gameplay radius.
                // Keep a soft rim visible around that footprint instead of
                // hiding the entire contact shadow beneath the mesh.
                radius *= 1.48F;
            } else if (node.type == ResourceType::Crystal) {
                radius *= 1.18F *
                    static_cast<float>(node.visualScale);
            } else if (isDestructibleProp(node.type)) {
                radius *= 0.92F *
                    static_cast<float>(node.visualScale);
            } else {
                radius *= 1.05F;
            }
            const float revealScale =
                renderer_->worldRevealScaleAt({
                    static_cast<float>(node.position.x),
                    static_cast<float>(node.position.z),
                });
            radius *= revealScale;
            if (radius <= 0.001F) {
                continue;
            }
            const float groundY = static_cast<float>(
                node.position.y - node.groundOffset);
            const float outerOpacity =
                node.type == ResourceType::Wood
                    ? 0.30F
                    : node.type == ResourceType::Stone
                        ? 0.22F
                        : node.type == ResourceType::Crystal
                            ? 0.24F
                        : 0.25F;
            const float contactOpacity =
                node.type == ResourceType::Wood
                    ? 0.48F
                    : node.type == ResourceType::Stone
                        ? 0.38F
                        : node.type == ResourceType::Crystal
                            ? 0.42F
                        : 0.40F;
            renderer_->drawBlobShadow(
                {static_cast<float>(node.position.x),
                groundY + 0.018F,
                 static_cast<float>(node.position.z)},
                radius, radius * 0.82F,
                outerOpacity);
            renderer_->drawBlobShadow(
                {static_cast<float>(node.position.x),
                groundY + 0.02F,
                 static_cast<float>(node.position.z)},
                radius *
                    (node.type == ResourceType::Stone ? 0.68F : 0.52F),
                radius *
                    (node.type == ResourceType::Stone ? 0.56F : 0.42F),
                contactOpacity);
        }
        for (const LootChestInstance& chest : snapshot.lootChests) {
            if (chest.looseLoot) {
                continue;
            }
            const float visible = 1.0F - smoothstep(
                0.0F, 1.0F,
                static_cast<float>(
                    chest.disappearanceProgress));
            const float radius = chest.type == LootChestType::Stone
                ? 0.72F
                : 0.62F;
            const Vector3 position{
                static_cast<float>(chest.position.x),
                static_cast<float>(chest.position.y) + 0.018F,
                static_cast<float>(chest.position.z),
            };
            renderer_->drawBlobShadow(
                position, radius*visible,
                radius*0.76F*visible, 0.14F*visible);
            renderer_->drawBlobShadow(
                {position.x, position.y + 0.002F, position.z},
                radius*0.52F*visible,
                radius*0.38F*visible, 0.25F*visible);
        }
        // Decorative rock/bush AO repeats the full decoration grid traversal.
        // Keep it for High quality; gameplay-critical objects retain their
        // contact shadows on every preset.
        if (renderer_->settings().quality != GraphicsQuality::Low) {
            const auto clearAreas =
                activeDecorationClearAreas(snapshot);
            renderer_->drawDecorativeRockAo(
                camera.position,
                static_cast<float>(snapshot.worldLimit),
                clearAreas);
        }
        for (const SharedSupport& support : snapshot.sharedSupports) {
            if (!support.active || support.length <= 0.05) {
                continue;
            }
            renderer_->drawBlobShadow(
                {static_cast<float>(support.bottom.x),
                 static_cast<float>(support.bottom.y) + 0.018F,
                 static_cast<float>(support.bottom.z)},
                0.24F, 0.2F, 0.28F);
        }
        for (const auto& building : snapshot.buildings) {
            const Vec3 center =
                buildingWorldPosition(building);
            const Vec3 impact =
                buildingImpactOffsetAt(building.id);
            const float x =
                static_cast<float>(center.x + impact.x);
            const float z =
                static_cast<float>(center.z + impact.z);
            const float groundY =
                static_cast<float>(center.y);
            float radius = 0.62F;
            float opacity = 0.2F;
            if (building.type == BuildingType::Core) {
                radius = 1.1F;
                opacity = 0.24F;
            } else if (
                building.type == BuildingType::SlowTrap ||
                building.type == BuildingType::SpikeTrap) {
                radius = 0.54F;
                opacity = 0.1F;
            }
            renderer_->drawBlobShadow(
                {x, groundY + 0.018F, z}, radius,
                                      radius * 0.82F, opacity * 0.72F);
            renderer_->drawBlobShadow(
                {x, groundY + 0.02F, z},
                radius * 0.52F,
                radius * 0.42F,
                (building.type == BuildingType::SlowTrap ||
                 building.type == BuildingType::SpikeTrap)
                    ? 0.1F
                    : opacity + 0.04F);
        }
        renderer_->endBlobShadowBatch();
    }
    performanceStats_.blobShadows.sample(
        performanceMilliseconds(blobShadowStart));
}

void App::drawCancelledPlacementPreview(
    const WorldLighting& lighting) {
    if (!cancelledPlacementPreview_) {
        return;
    }
    const auto& preview = *cancelledPlacementPreview_;
    const float progress = std::clamp(
        static_cast<float>(
            1.0 - preview.remaining / preview.duration),
        0.0F, 1.0F);
    const float eased =
        progress * progress * (3.0F - 2.0F * progress);
    const float scale =
        std::max(0.0F, 1.0F - eased);
    const float sink = eased * 0.18F;
    const float x = preview.center.x;
    const float z = preview.center.y;
    const float yaw = preview.yaw;
    const float fade = 1.0F - progress;

    WorldMaterialState material{};
    material.baseColor = {
        0.28F, 0.88F, 0.48F, fade * 0.42F};
    material.bakedAo = 0.85F;
    material.screenAoAmount = 0.0F;
    renderer_->beginWorldShader(lighting);
    renderer_->setWorldMaterial(material);
    const Vector3 modelPosition{x, -sink, z};
    if (preview.type == BuildingType::Core) {
        static_cast<void>(
            renderer_->drawCore(
                modelPosition, yaw, WHITE, scale));
    } else if (preview.type == BuildingType::Turret) {
        static_cast<void>(
            renderer_->drawCrossbow(
                modelPosition, yaw, WHITE, scale));
    } else if (preview.type == BuildingType::GunTurret) {
        static_cast<void>(renderer_->drawGunTurret(
            modelPosition, yaw, yaw, WHITE, scale));
    } else if (preview.type == BuildingType::Cannon) {
        static_cast<void>(
            renderer_->drawCannon(
                modelPosition, yaw, 0.0F, WHITE, scale));
    } else if (preview.type == BuildingType::Catapult) {
        static_cast<void>(renderer_->drawCatapult(
            modelPosition, yaw, 0.0F, true, WHITE, scale));
    } else if (
        preview.type == BuildingType::CrystalMine ||
        preview.type == BuildingType::LumberMill ||
        preview.type == BuildingType::Quarry) {
        static_cast<void>(
            renderer_->drawResourceProducer(
                preview.type, modelPosition, yaw, WHITE,
                scale));
    } else if (preview.type == BuildingType::Wall) {
        static_cast<void>(
            renderer_->drawWall(
                modelPosition, 0U, yaw, WHITE, scale));
    } else {
        const auto scaledPosition =
            [x, z, scale, sink](
                float offsetX, float y, float offsetZ) {
                return Vector3{
                    x + offsetX * scale,
                    y * scale - sink,
                    z + offsetZ * scale,
                };
            };
        const auto drawCube =
            [&scaledPosition, scale](
                float offsetX, float y, float offsetZ,
                float width, float height, float depth) {
                DrawCube(
                    scaledPosition(offsetX, y, offsetZ),
                    width * scale, height * scale,
                    depth * scale, WHITE);
            };
        if (preview.type == BuildingType::SlowTrap) {
            drawCube(
                0.0F, 0.08F, 0.0F, 1.0F, 0.16F, 1.0F);
        } else if (preview.type == BuildingType::SpikeTrap) {
            static_cast<void>(renderer_->drawSpikeTrap(
                {x, -sink, z}, preview.yaw, -1.0F,
                WHITE, scale));
        } else if (
            preview.type == BuildingType::WoodStorage ||
            preview.type == BuildingType::StoneStorage ||
            preview.type == BuildingType::CrystalStorage) {
            drawCube(0.0F, 0.75F, 0.0F, 1.8F,
                     1.5F, 1.8F);
            drawCube(0.0F, 1.62F, 0.0F, 1.25F,
                     0.24F, 1.25F);
        } else if (
            std::abs(std::sin(preview.yaw)) < 0.5F) {
            drawCube(
                -0.38F, 1.0F, 0.0F, 0.22F, 2.0F, 1.0F);
            drawCube(
                0.38F, 1.0F, 0.0F, 0.22F, 2.0F, 1.0F);
            drawCube(
                0.0F, 1.0F, 0.0F, 0.55F, 1.7F, 0.18F);
        } else {
            drawCube(
                0.0F, 1.0F, -0.38F, 1.0F, 2.0F, 0.22F);
            drawCube(
                0.0F, 1.0F, 0.38F, 1.0F, 2.0F, 0.22F);
            drawCube(
                0.0F, 1.0F, 0.0F, 0.18F, 1.7F, 0.55F);
        }
    }
    renderer_->endWorldShader();

    const auto ringAlpha =
        static_cast<unsigned char>(
            std::lround(fade * 150.0F));
    DrawCircle3D(
        {x, 0.07F, z},
        static_cast<float>(
            buildingFootprintHalfExtent(preview.type)) *
            scale,
        {1.0F, 0.0F, 0.0F}, 90.0F,
        {91, 238, 132, ringAlpha});
}


} // namespace ian
