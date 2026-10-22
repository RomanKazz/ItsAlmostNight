#include "graphics/MissileLauncherModel.hpp"
#include "graphics/TeslaUpgradeModel.hpp"
#include "graphics/LaserModel.hpp"
#include "graphics/WindBlowerModel.hpp"
#include "graphics/MortarModel.hpp"
#include "buildings/TowerRig.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/PortalArrivalEffect.hpp"
#include "graphics/CoreCrystalModel.hpp"
#include "graphics/GateModel.hpp"
#include "graphics/GunTurretModel.hpp"
#include "graphics/CannonModel.hpp"
#include "graphics/CrossbowModel.hpp"
#include "graphics/FlamethrowerModel.hpp"
#include "graphics/CatapultModel.hpp"
#include "graphics/CrystalMineModel.hpp"
#include "graphics/QuarryModel.hpp"
#include "graphics/SpikeTrapModel.hpp"
#include "graphics/WallUpgradeModel.hpp"
#include "buildings/CannonRig.hpp"
#include "buildings/CatapultRig.hpp"
#include "graphics/RendererModelSupport.hpp"
#include "graphics/WorldTransforms.hpp"

#include "buildings/BuildingSystem.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

namespace ian {
namespace {

void drawPortalMatterPlane(
    Texture2D texture, Vector3 center, float yawRadians,
    float halfExtent, float textureRotationRadians, Color tint) {
    // Portal.glb opens in its local YZ plane. Match that plane after yaw;
    // using the usual local XY billboard axes turns the matter 90 degrees
    // through the stone frame.
    const Vector3 normal{
        std::cos(yawRadians), 0.0F, -std::sin(yawRadians)};
    const Vector3 right{
        std::sin(yawRadians), 0.0F, std::cos(yawRadians)};
    center = Vector3Add(center, Vector3Scale(normal, 0.035F));
    const Vector3 worldUp{0.0F, 1.0F, 0.0F};
    const float cosine = std::cos(textureRotationRadians);
    const float sine = std::sin(textureRotationRadians);
    const Vector3 horizontal = Vector3Scale(
        Vector3Add(
            Vector3Scale(right, cosine),
            Vector3Scale(worldUp, sine)),
        halfExtent);
    const Vector3 vertical = Vector3Scale(
        Vector3Add(
            Vector3Scale(right, -sine),
            Vector3Scale(worldUp, cosine)),
        halfExtent);
    constexpr int SegmentCount = 48;
    rlSetTexture(texture.id);
    rlBegin(RL_TRIANGLES);
    rlNormal3f(normal.x, normal.y, normal.z);
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    for (int segment = 0; segment < SegmentCount; ++segment) {
        const float firstAngle =
            2.0F * PI * static_cast<float>(segment) /
            static_cast<float>(SegmentCount);
        const float secondAngle =
            2.0F * PI * static_cast<float>(segment + 1) /
            static_cast<float>(SegmentCount);
        const auto emit = [&](Vector3 vertex, Vector2 uv) {
            rlTexCoord2f(uv.x, uv.y);
            rlVertex3f(vertex.x, vertex.y, vertex.z);
        };
        emit(center, {0.5F, 0.5F});
        emit(
            Vector3Add(
                center,
                Vector3Add(
                    Vector3Scale(horizontal, std::cos(firstAngle)),
                    Vector3Scale(vertical, std::sin(firstAngle)))),
            {0.5F + 0.5F * std::cos(firstAngle),
             0.5F - 0.5F * std::sin(firstAngle)});
        emit(
            Vector3Add(
                center,
                Vector3Add(
                    Vector3Scale(horizontal, std::cos(secondAngle)),
                    Vector3Scale(vertical, std::sin(secondAngle)))),
            {0.5F + 0.5F * std::cos(secondAngle),
             0.5F - 0.5F * std::sin(secondAngle)});
    }
    rlEnd();
    rlSetTexture(0);
}

void drawPortalGlowRing(
    Vector3 center, float yawRadians, float matterRadius,
    Color tint) {
    const Vector3 normal{
        std::cos(yawRadians), 0.0F, -std::sin(yawRadians)};
    const Vector3 right{
        std::sin(yawRadians), 0.0F, std::cos(yawRadians)};
    const Vector3 worldUp{0.0F, 1.0F, 0.0F};
    center = Vector3Add(center, Vector3Scale(normal, 0.052F));
    constexpr int SegmentCount = 48;
    constexpr std::array<float, 3> RadiusFactors{
        0.90F, 1.01F, 1.15F};
    const std::array<unsigned char, 3> Alphas{
        0U, tint.a, 0U};
    const auto point = [&](float radius, float angle) {
        return Vector3Add(
            center,
            Vector3Add(
                Vector3Scale(right, std::cos(angle) * radius),
                Vector3Scale(worldUp, std::sin(angle) * radius)));
    };
    const auto emit = [&](Vector3 vertex, unsigned char alpha) {
        rlColor4ub(tint.r, tint.g, tint.b, alpha);
        rlVertex3f(vertex.x, vertex.y, vertex.z);
    };
    rlSetTexture(0);
    rlBegin(RL_TRIANGLES);
    rlNormal3f(normal.x, normal.y, normal.z);
    for (int segment = 0; segment < SegmentCount; ++segment) {
        const float firstAngle =
            2.0F * PI * static_cast<float>(segment) /
            static_cast<float>(SegmentCount);
        const float secondAngle =
            2.0F * PI * static_cast<float>(segment + 1) /
            static_cast<float>(SegmentCount);
        for (std::size_t band = 0; band < 2U; ++band) {
            const float innerRadius =
                matterRadius * RadiusFactors[band];
            const float outerRadius =
                matterRadius * RadiusFactors[band + 1U];
            const Vector3 innerFirst = point(innerRadius, firstAngle);
            const Vector3 innerSecond = point(innerRadius, secondAngle);
            const Vector3 outerFirst = point(outerRadius, firstAngle);
            const Vector3 outerSecond = point(outerRadius, secondAngle);
            emit(innerFirst, Alphas[band]);
            emit(outerFirst, Alphas[band + 1U]);
            emit(outerSecond, Alphas[band + 1U]);
            emit(innerFirst, Alphas[band]);
            emit(outerSecond, Alphas[band + 1U]);
            emit(innerSecond, Alphas[band]);
        }
    }
    rlEnd();
}

} // namespace

using namespace renderer_model_detail;

bool Renderer::drawCannon(Vector3 position, float yawRadians,
                          float pitchRadians, Color tint,
                          float scaleFactor, int level) {
    auto& resource = resources_.cannonModel();
    if (!resource.valid()) {
        return false;
    }

    Model& model = resource.get();
    prepareModelShader(model);

    if (model.meshCount < 2) {
        DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                    yawRadians * RAD2DEG,
                    {scaleFactor, scaleFactor, scaleFactor},
                    tint);
        return true;
    }

    const Matrix scale =
        MatrixScale(scaleFactor, scaleFactor, scaleFactor);
    const Matrix yaw = MatrixRotateY(yawRadians);
    const Matrix localPitch = cannon_model_detail::pitchTransform(pitchRadians);
    const Matrix translation =
        MatrixTranslate(position.x, position.y, position.z);
    const Matrix baseTransform =
        MatrixMultiply(MatrixMultiply(scale, yaw), translation);
    const Matrix barrelTransform = MatrixMultiply(
        MatrixMultiply(MatrixMultiply(scale, localPitch), yaw),
        translation);

    const auto drawMesh = [&model, tint](int meshIndex,
                                         Matrix transform) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    };

    // The authored collider is stripped, leaving the pitch-pivot barrel
    // first and the yaw-only body second.
    drawMesh(1, baseTransform);
    drawMesh(0, barrelTransform);
    if (model.meshCount >= cannon_model_detail::MeshCount) {
        cannon_model_detail::forEachUpgradePart(level,
            [&](int meshIndex, bool pitched) {
                drawMesh(meshIndex, pitched ? barrelTransform : baseTransform);
            });
    } else {
        for (int meshIndex = 2; meshIndex < model.meshCount; ++meshIndex) {
            drawMesh(meshIndex, baseTransform);
        }
    }
    return true;
}

bool Renderer::drawCannonball(Vector3 position, Color tint, float scale) {
    auto& resource = resources_.cannonballModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    if (model.meshCount <= 0 || model.meshes == nullptr ||
        model.meshMaterial == nullptr || model.materials == nullptr) {
        return false;
    }
    prepareModelShader(model);
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F}, 0.0F,
                {scale, scale, scale}, tint);
    return true;
}

bool Renderer::drawWindBlower(Vector3 position, float baseYaw, Color tint, float scale, float headYaw, float rotor, int level) {
    auto& resource = resources_.windBlowerModel(level);
    if (!resource.valid()) return false;
    auto& model = resource.get();
    prepareModelShader(model);
    for (int mesh=0; mesh<model.meshCount; ++mesh) {
        auto& material = model.materials[model.meshMaterial[mesh]];
        const auto color = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(color,tint);
        DrawMesh(model.meshes[mesh],material,MatrixMultiply(model.transform,
            wind_blower_model::meshTransform(mesh,position,baseYaw,headYaw,rotor,scale)));
        material.maps[MATERIAL_MAP_DIFFUSE].color = color;
    }
    return true;
}

bool Renderer::drawDecoyBeacon(Vector3 position, float yawRadians, Color tint, float scale, float lightStrength, int level) {
    auto& resource = resources_.decoyBeaconModel(level);
    if (!resource.valid()) return false;
    auto& model = resource.get();
    prepareModelShader(model);
    const auto saved = worldMaterial_;
    const Matrix transform = MatrixMultiply(model.transform, MatrixMultiply(
        MatrixScale(scale, scale, scale), MatrixMultiply(MatrixRotateY(yawRadians),
            MatrixTranslate(position.x, position.y, position.z))));
    for (int mesh=0;mesh<model.meshCount;++mesh) {
        auto state = saved;
        if (mesh==1) {state.unlitAmount=.25F+.75F*std::clamp(lightStrength,0.0F,1.0F);state.screenAoAmount=0;}
        setWorldMaterial(state);
        auto& material=model.materials[model.meshMaterial[mesh]];
        const auto color=material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color=ColorTint(color,tint);
        DrawMesh(model.meshes[mesh],material,transform);
        material.maps[MATERIAL_MAP_DIFFUSE].color=color;
    }
    setWorldMaterial(saved);
    return true;
}

bool Renderer::drawRepairStation(Vector3 position, float yawRadians, Color tint, float scale, int level) {
    auto& resource = resources_.repairStationModel(level);
    if (!resource.valid()) return false;
    auto& model = resource.get();
    prepareModelShader(model);
    DrawModelEx(model, position, {0, 1, 0}, yawRadians * RAD2DEG, {scale, scale, scale}, tint);
    return true;
}

bool Renderer::drawMortar(Vector3 position, float yawRadians, Color tint,
    float scale, float pitchRadians, std::optional<float> headYaw, int level) {
    auto& resource=resources_.mortarModel();
    if (!resource.valid()) return false;
    Model& model=resource.get();prepareModelShader(model);
    const auto saved=worldMaterial_;
    for(int mesh=0;mesh<model.meshCount;++mesh) {
        auto state=saved;setWorldMaterial(state);
        auto& material=model.materials[model.meshMaterial[mesh]];
        const auto original=material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color=ColorTint(original,tint);
        DrawMesh(model.meshes[mesh],material,MatrixMultiply(model.transform,
            mortar_model::meshTransform(mesh,position,yawRadians,headYaw.value_or(yawRadians),pitchRadians,scale)));
        material.maps[MATERIAL_MAP_DIFFUSE].color=original;
    }
    setWorldMaterial(saved);
    auto& upgrades = resources_.mortarUpgradeModel();
    if (upgrades.valid()) {
        Model& extra = upgrades.get();
        prepareModelShader(extra);
        mortar_model::forEachUpgradePart(level, [&](int mesh, int role) {
            if (mesh >= extra.meshCount) return;
            Material& material = extra.materials[extra.meshMaterial[mesh]];
            const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
            material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
            DrawMesh(extra.meshes[mesh], material, MatrixMultiply(extra.transform,
                mortar_model::meshTransform(role, position, yawRadians,
                    headYaw.value_or(yawRadians), pitchRadians, scale)));
            material.maps[MATERIAL_MAP_DIFFUSE].color = original;
        });
    }
    return true;
}

bool Renderer::drawLaserTower(Vector3 position, float yawRadians, Color tint,
    float scale, float pitchRadians, std::optional<float> headYaw, int level) {
    auto& resource=resources_.laserTowerModel(level);
    if (!resource.valid()) return false;
    Model& model=resource.get();prepareModelShader(model);
    const auto saved=worldMaterial_;
    for(int mesh=0;mesh<model.meshCount;++mesh) {
        auto state=saved;if(mesh==1) state.unlitAmount=.85F;setWorldMaterial(state);
        auto& material=model.materials[model.meshMaterial[mesh]];
        const auto original=material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color=ColorTint(original,tint);
        DrawMesh(model.meshes[mesh],material,MatrixMultiply(model.transform,
            laser_model::meshTransform(mesh,position,yawRadians,headYaw.value_or(yawRadians),pitchRadians,scale)));
        material.maps[MATERIAL_MAP_DIFFUSE].color=original;
    }
    setWorldMaterial(saved);return true;
}

bool Renderer::drawTeslaTower(Vector3 position, float yawRadians, Color tint,
    float scale, float /*pitchRadians*/, std::optional<float> /*headYaw*/, int level) {
    auto& resource = resources_.teslaTowerModel();
    if (!resource.valid()) return false;
    Model& model = resource.get();
    prepareModelShader(model);
    const auto savedMaterial=worldMaterial_;
    for (int index = 0; index < model.meshCount; ++index) {
        if(index==1) {auto energy=savedMaterial;energy.unlitAmount=0.85F;setWorldMaterial(energy);}
        Material& material = model.materials[model.meshMaterial[index]];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
        const Matrix pose = missile_launcher_model::meshTransform(index, position,
            yawRadians, yawRadians, 0, scale);
        DrawMesh(model.meshes[index], material, MatrixMultiply(model.transform, pose));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    setWorldMaterial(savedMaterial);
    auto& upgrades = resources_.teslaUpgradeModel();
    const int mesh = tesla_upgrade_model::meshIndex(level);
    if (upgrades.valid() && mesh >= 0 && mesh < upgrades.get().meshCount) {
        Model& extra = upgrades.get();
        prepareModelShader(extra);
        Material& material = extra.materials[extra.meshMaterial[mesh]];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
        const Matrix pose = missile_launcher_model::meshTransform(0, position,
            yawRadians, yawRadians, 0, scale);
        DrawMesh(extra.meshes[mesh], material, MatrixMultiply(extra.transform, pose));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    return true;
}

bool Renderer::drawMissileLauncher(Vector3 position, float yawRadians, Color tint,
    float scale, float pitchRadians, std::optional<float> headYaw, int level) {
    auto& resource = resources_.missileLauncherModel(level);
    if (!resource.valid()) return false;
    Model& model = resource.get();
    prepareModelShader(model);
    for (int index = 0; index < model.meshCount; ++index) {
        Material& material = model.materials[model.meshMaterial[index]];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
        const Matrix pose = missile_launcher_model::meshTransform(index, position,
            yawRadians, headYaw.value_or(yawRadians), pitchRadians, scale);
        DrawMesh(model.meshes[index], material, MatrixMultiply(model.transform, pose));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    return true;
}

bool Renderer::drawFlamethrower(Vector3 position, float yawRadians, Color tint,
    float scale, float pitchRadians, std::optional<float> headYaw, int level) {
    auto& resource = resources_.flamethrowerModel();
    if (!resource.valid()) return false;
    Model& model = resource.get();
    prepareModelShader(model);
    for (int index = 0; index < model.meshCount; ++index) {
        Material& material = model.materials[model.meshMaterial[index]];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
        const Matrix pose = flamethrower_model::meshTransform(index, position,
            yawRadians, headYaw.value_or(yawRadians), pitchRadians, scale);
        DrawMesh(model.meshes[index], material, MatrixMultiply(model.transform, pose));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    auto& upgrades = resources_.flamethrowerUpgradeModel();
    if (upgrades.valid()) {
        Model& extra = upgrades.get();
        prepareModelShader(extra);
        flamethrower_model::forEachUpgradePart(level, [&](int mesh, int role) {
            if (mesh >= extra.meshCount) return;
            Material& material = extra.materials[extra.meshMaterial[mesh]];
            const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
            material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
            DrawMesh(extra.meshes[mesh], material, MatrixMultiply(extra.transform,
                flamethrower_model::meshTransform(role, position, yawRadians,
                    headYaw.value_or(yawRadians), pitchRadians, scale)));
            material.maps[MATERIAL_MAP_DIFFUSE].color = original;
        });
    }
    return true;
}

bool Renderer::drawCatapult(
    Vector3 position, float yawRadians, float armPitchRadians,
    bool loaded, Color tint, float scaleFactor, int level) {
    auto& resource = resources_.catapultModel();
    if (!resource.valid()) return false;

    Model& model = resource.get();
    prepareModelShader(model);

    if (model.meshCount < 2) {
        DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                    yawRadians * RAD2DEG,
                    {scaleFactor, scaleFactor, scaleFactor}, tint);
    } else {
        const Matrix scale =
            MatrixScale(scaleFactor, scaleFactor, scaleFactor);
        const Matrix yaw = MatrixRotateY(yawRadians);
        const Matrix localPitch =
            catapult_model_detail::pitchTransform(armPitchRadians);
        const Matrix translation =
            MatrixTranslate(position.x, position.y, position.z);
        const Matrix bodyTransform =
            MatrixMultiply(MatrixMultiply(scale, yaw), translation);
        const Matrix armTransform = MatrixMultiply(
            MatrixMultiply(MatrixMultiply(scale, localPitch), yaw),
            translation);
        const auto drawMesh = [&model, tint](int meshIndex,
                                             Matrix transform) {
            const int materialIndex = model.meshMaterial[meshIndex];
            Material& material = model.materials[materialIndex];
            const Color original =
                material.maps[MATERIAL_MAP_DIFFUSE].color;
            material.maps[MATERIAL_MAP_DIFFUSE].color =
                ColorTint(original, tint);
            DrawMesh(model.meshes[meshIndex], material,
                     MatrixMultiply(model.transform, transform));
            material.maps[MATERIAL_MAP_DIFFUSE].color = original;
        };
        // The collision mesh is stripped at import: the throwing arm is
        // first, while the remaining body only follows yaw.
        drawMesh(0, armTransform);
        drawMesh(1, bodyTransform);
        if (model.meshCount >= catapult_model_detail::MeshCount) {
            catapult_model_detail::forEachUpgradePart(level,
                [&](int meshIndex, bool pitched) {
                    drawMesh(meshIndex, pitched ? armTransform : bodyTransform);
                });
        } else {
            for (int meshIndex = 2; meshIndex < model.meshCount; ++meshIndex) {
                drawMesh(meshIndex, bodyTransform);
            }
        }
    }

    if (loaded) {
        const Vec3 base{position.x, position.y, position.z};
        Vec3 socket = catapultMuzzleWorldPosition(
            base, yawRadians, armPitchRadians);
        socket.x = base.x + (socket.x - base.x) * scaleFactor;
        socket.y = base.y + (socket.y - base.y) * scaleFactor;
        socket.z = base.z + (socket.z - base.z) * scaleFactor;
        // Shadow and selection passes use the same round silhouette and
        // spawn scale as the shaded body rendered by the world pass.
        DrawSphereEx(
            {static_cast<float>(socket.x), static_cast<float>(socket.y),
             static_cast<float>(socket.z)}, 0.24F * scaleFactor, 12, 16, tint);
    }
    return true;
}

bool Renderer::drawCatapultBall(Vector3 position, Color tint) {
    auto& resource = resources_.catapultBallModel();
    if (!resource.valid()) return false;
    Model& model = resource.get();
    prepareModelShader(model);
    DrawModelEx(model, position, {0.0F, 1.0F, 0.0F}, 0.0F,
                {1.0F, 1.0F, 1.0F}, tint);
    return true;
}

bool Renderer::drawArrow(Vector3 position, Vector3 direction, Color tint) {
    auto& resource = resources_.arrowModel();
    if (!resource.valid() ||
        Vector3LengthSqr(direction) <= 0.000001F) {
        return false;
    }
    Model& model = resource.get();
    prepareModelShader(model);

    const Vector3 forward = Vector3Normalize(direction);
    const float yaw = std::atan2(-forward.x, -forward.z);
    const float horizontal = std::hypot(forward.x, forward.z);
    const float pitch = std::atan2(forward.y, horizontal);

    constexpr float ArrowScale = 1.0F;
    const Matrix transform = MatrixMultiply(
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(ArrowScale, ArrowScale, ArrowScale),
                MatrixRotateX(pitch)),
            MatrixRotateY(yaw)),
        MatrixTranslate(position.x, position.y, position.z));
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    return true;
}

bool Renderer::drawCrossbow(Vector3 position, float yawRadians,
                            Color tint, float scale,
                            float pitchRadians, int level) {
    auto& resource = resources_.crossbowModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    prepareModelShader(model);
    const Matrix localPitch = crossbow_model_detail::pitchTransform(pitchRadians);
    const Matrix yawTransform = MatrixMultiply(
        MatrixMultiply(
            MatrixScale(scale, scale, scale),
            MatrixRotateY(yawRadians)),
        MatrixTranslate(position.x, position.y, position.z));
    const Matrix pitchedTransform = MatrixMultiply(
        MatrixMultiply(
            MatrixMultiply(MatrixScale(scale, scale, scale), localPitch),
            MatrixRotateY(yawRadians)),
        MatrixTranslate(position.x, position.y, position.z));
    const auto drawMesh = [&](int meshIndex, bool pitched) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform,
                     pitched ? pitchedTransform : yawTransform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    };
    if (model.meshCount >= crossbow_model_detail::MeshCount) {
        // Collider removal leaves the complete pitched upper assembly at 0,
        // and the yaw-only carriage at 1. Draw only the selected upgrade pair.
        drawMesh(1, false);
        drawMesh(0, true);
        crossbow_model_detail::forEachUpgradePart(level, drawMesh);
    } else {
        for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
            drawMesh(meshIndex, meshIndex == 0);
        }
    }
    return true;
}

Vec3 Renderer::crossbowMuzzlePosition(
    Vec3 position, float yawRadians, float pitchRadians) const {
    return crossbowMuzzleWorldPosition(position, yawRadians, pitchRadians);
}

bool Renderer::drawGunTurret(Vector3 position, float baseYawRadians,
                             float barrelYawRadians, Color tint,
                             float scale, Vec3 barrelRecoilOffset, int level) {
    auto& resource = resources_.gunTurretModel();
    if (!resource.valid()) return false;
    Model& model = resource.get();
    if (model.meshCount <= 0 || model.meshes == nullptr ||
        model.meshMaterial == nullptr || model.materials == nullptr) {
        return false;
    }
    prepareModelShader(model);
    const auto transformFor = [position, scale](float yaw, Vec3 offset) {
        return MatrixMultiply(
            MatrixMultiply(MatrixScale(scale, scale, scale),
                           MatrixRotateY(yaw)),
            MatrixTranslate(
                position.x + static_cast<float>(offset.x),
                position.y + static_cast<float>(offset.y),
                position.z + static_cast<float>(offset.z)));
    };
    const Matrix baseTransform = transformFor(baseYawRadians, {});
    const Matrix barrelTransform = transformFor(
        barrelYawRadians, barrelRecoilOffset);
    const auto drawPart = [&](int meshIndex, Matrix pose, Color partTint, bool rotating) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original =
            material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint(original, ColorTint(partTint, tint));
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(pose, MatrixMultiply(model.transform,
                     rotating ? barrelTransform : baseTransform)));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    };
    // Keep the authored base/barrels and muzzle locations unchanged.
    for (int meshIndex = 0; meshIndex < std::min(model.meshCount, 2); ++meshIndex)
        drawPart(meshIndex, MatrixIdentity(), WHITE, meshIndex == 1);
    if (model.meshCount >= gun_turret_model_detail::MeshCount)
        gun_turret_model_detail::forEachUpgradePart(level, drawPart);
    return true;
}

Vec3 Renderer::gunTurretMuzzlePosition(
    Vec3 position, float yawRadians, std::size_t muzzleIndex) {
    const auto& sockets =
        resources_.gunTurretModel().collisionAsset().sockets;
    if (sockets.empty()) {
        return {position.x, position.y + 0.7, position.z};
    }
    const Vec3 local = sockets[muzzleIndex % sockets.size()].position;
    const double cosine = std::cos(static_cast<double>(yawRadians));
    const double sine = std::sin(static_cast<double>(yawRadians));
    return {
        position.x + local.x * cosine + local.z * sine,
        position.y + local.y,
        position.z - local.x * sine + local.z * cosine,
    };
}

float Renderer::buildingVisualHeight(BuildingType type, int level) {
    ModelResource* model = nullptr;
    float scale = 1.0F;
    switch (type) {
    case BuildingType::Core: model = &resources_.coreModel(); scale = CoreModelScale; break;
    case BuildingType::Wall: model = &resources_.wallIsolatedModel(); break;
    case BuildingType::Gate: model = &resources_.gateModel(); break;
    case BuildingType::Turret: model = &resources_.crossbowModel(); break;
    case BuildingType::GunTurret: model = &resources_.gunTurretModel(); break;
    case BuildingType::Cannon: model = &resources_.cannonModel(); break;
    case BuildingType::TeslaTower: return 1.44F;
    case BuildingType::LaserTower: return level > 1 ? 1.50F : 1.40F;
    case BuildingType::Mortar: return 1.45F;
    case BuildingType::DecoyBeacon: model = &resources_.decoyBeaconModel(level); break;
    case BuildingType::WindBlower: model = &resources_.windBlowerModel(level); break;
    case BuildingType::RepairStation: model = &resources_.repairStationModel(level); break;
    case BuildingType::MissileLauncher: return 1.54F;
    case BuildingType::Flamethrower: return std::max(
        resources_.flamethrowerModel().valid() ? resources_.flamethrowerModel().visualBounds().max.y : 1.14F,
        flamethrower_model::UpgradeHeights[static_cast<std::size_t>(std::clamp(level, 1, 8) - 1)]);
    case BuildingType::Catapult: model = &resources_.catapultModel(); break;
    case BuildingType::CrystalMine: model = &resources_.mineModel(level); scale = 1.0F; break;
    case BuildingType::LumberMill: model = &resources_.lumberMillModel(level); scale = 1.0F; break;
    case BuildingType::Quarry: model = &resources_.quarryModel(level); scale = 1.0F; break;
    case BuildingType::SpikeTrap: model = &resources_.spikeTrapModel(); scale = 1.2F; break;
    case BuildingType::SlowTrap: return .45F;
    case BuildingType::WoodStorage:
    case BuildingType::StoneStorage:
    case BuildingType::CrystalStorage: return 1.8F;
    }
    if (model && model->valid()) {
        // visualBounds includes every baked upgrade, even hidden levels.
        // Measure only the meshes and poses actually selected for this level.
        const auto bounds = model->meshBounds();
        const int meshCount = model->get().meshCount;
        float height = 0.2F;
        const auto includeMesh = [&](int index, Matrix pose = MatrixIdentity()) {
            if (index < 0 || static_cast<std::size_t>(index) >= bounds.size() ||
                !model->meshValid(static_cast<std::size_t>(index))) return;
            const BoundingBox transformed = world_transforms::transformBounds(
                bounds[static_cast<std::size_t>(index)],
                MatrixMultiply(pose, model->get().transform));
            height = std::max(height, transformed.max.y * scale);
        };
        const auto includePair = [&](int index, bool) { includeMesh(index); };
        if (type == BuildingType::Core && meshCount == core_crystal_model_detail::MeshCount) {
            core_crystal_model_detail::forEachLevelPart(level, 0.0,
                [&](int index, Matrix pose, Color, bool) { includeMesh(index, pose); });
        } else if (type == BuildingType::Wall &&
                   meshCount >= wall_upgrade_model_detail::MeshCount) {
            includeMesh(0);
            includeMesh(wall_upgrade_model_detail::upgradeMeshIndex(level));
        } else if (type == BuildingType::SpikeTrap &&
                   meshCount >= spike_trap_model_detail::MeshCount) {
            includeMesh(0);
            if (level > 1) {
                const int base = spike_trap_model_detail::baseMeshIndex(level);
                includeMesh(base);
                includeMesh(base + 1);
            } else {
                includeMesh(1);
            }
        } else if ((type == BuildingType::Cannon || type == BuildingType::Catapult ||
                    type == BuildingType::Turret || type == BuildingType::GunTurret ||
                    type == BuildingType::Gate) && meshCount >= 16) {
            includeMesh(0);
            includeMesh(1);
            switch (type) {
            case BuildingType::Cannon: cannon_model_detail::forEachUpgradePart(level, includePair); break;
            case BuildingType::Catapult: catapult_model_detail::forEachUpgradePart(level, includePair); break;
            case BuildingType::Turret: crossbow_model_detail::forEachUpgradePart(level, includePair); break;
            case BuildingType::Gate: gate_model_detail::forEachUpgradePart(level, includePair); break;
            case BuildingType::GunTurret:
                gun_turret_model_detail::forEachUpgradePart(level,
                    [&](int index, Matrix pose, Color, bool) { includeMesh(index, pose); });
                break;
            default: break;
            }
        } else {
            for (int index = 0; index < meshCount; ++index) includeMesh(index);
        }
        return height + 0.005F;
    }
    return type == BuildingType::Core ? 2.5F : 2.15F;
}

bool Renderer::drawCore(Vector3 position, float yawRadians,
                        Color tint, float scale, int level) {
    auto& resource = resources_.coreModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    prepareModelShader(model);
    constexpr float GroundOffset = 0.005F;
    position.y += GroundOffset;
    if (model.meshCount == core_crystal_model_detail::MeshCount) {
        const float uniformScale = CoreModelScale * scale;
        const Matrix transform = MatrixMultiply(
            model.transform,
            MatrixMultiply(MatrixScale(uniformScale, uniformScale, uniformScale),
                MatrixMultiply(MatrixRotateY(yawRadians),
                    MatrixTranslate(position.x, position.y, position.z))));
        const double seconds = GetTime();
        core_crystal_model_detail::forEachLevelPart(level, seconds,
            [&](int index, Matrix pose, Color partTint, bool solidColor) {
                Material& material = model.materials[model.meshMaterial[index]];
                MaterialMap& diffuse = material.maps[MATERIAL_MAP_DIFFUSE];
                const Color original = diffuse.color;
                const Texture2D originalTexture = diffuse.texture;
                diffuse.color = ColorTint(
                    solidColor ? partTint : ColorTint(original, partTint), tint);
                if (solidColor) diffuse.texture.id = rlGetTextureIdDefault();
                DrawMesh(model.meshes[index], material, MatrixMultiply(pose, transform));
                diffuse.color = original;
                diffuse.texture = originalTexture;
            });
    } else {
        DrawModelEx(model, position, {0.0F, 1.0F, 0.0F},
                    yawRadians * RAD2DEG,
                    {CoreModelScale * scale, CoreModelScale * scale,
                     CoreModelScale * scale}, tint);
    }
    return true;
}

bool Renderer::drawChallengeColumn(
    Vector3 position, float yawRadians, Color tint, float scale) {
    auto& resource = resources_.challengeColumnModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    prepareModelShader(model);
    const BoundingBox bounds = resource.visualBounds();
    const float authoredHeight = std::max(0.001F, bounds.max.y - bounds.min.y);
    const float modelScale = 3.25F / authoredHeight;
    position.y -= bounds.min.y * modelScale * scale;
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F}, yawRadians * RAD2DEG,
        {modelScale * scale, modelScale * scale, modelScale * scale}, tint);
    return true;
}

bool Renderer::drawChallengeArenaPeg(
    Vector3 position, float yawRadians, Color tint, float scale) {
    auto& resource = resources_.challengeArenaPegModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    prepareModelShader(model, false);
    const BoundingBox bounds = resource.visualBounds();
    const float authoredHeight = std::max(0.001F, bounds.max.y - bounds.min.y);
    constexpr float TargetHeight = 2.25F;
    const float modelScale = TargetHeight / authoredHeight;
    position.y -= bounds.min.y * modelScale * scale;
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F}, yawRadians * RAD2DEG,
        {modelScale * scale, modelScale * scale, modelScale * scale}, tint);
    return true;
}

bool Renderer::drawEnemyPortal(
    Vector3 position, float yawRadians,
    float matterRotationRadians, const WorldLighting& lighting,
    float scale, float arrivalProgress, float openingProgress, bool detailedArrival) {
    ModelResource& portal = resources_.enemyPortalModel();
    TextureResource& matter = resources_.portalMatterTexture();
    if (!portal.valid() || !matter.valid() || scale <= 0.001F) {
        return false;
    }

    constexpr float PortalCenterHeight = 1.74F;
    constexpr float PortalGroundLift = -0.24F;
    constexpr float MatterRadius = 2.13F;
    const float open=fire_wand_effect::ease(std::clamp(openingProgress,0.0F,1.0F));
    const float pulse=arrivalProgress>=0 && arrivalProgress<1 ? std::sin(arrivalProgress*PI)*(1-arrivalProgress) : 0;
    const float aperture=.12F+.88F*open;
    Model& frame = portal.get();
    beginWorldShader(lighting);
    WorldMaterialState stoneMaterial{};
    stoneMaterial.bakedAo = 0.78F;
    stoneMaterial.screenAoAmount = 0.0F;
    setWorldMaterial(stoneMaterial);
    if (resources_.worldShader().valid()) {
        for (int index = 0; index < frame.materialCount; ++index) {
            frame.materials[index].shader = resources_.worldShader().get();
        }
    }
    Vector3 framePosition = position;
    framePosition.y += PortalGroundLift * scale;
    DrawModelEx(
        frame, framePosition,
        {0.0F, 1.0F, 0.0F}, yawRadians * RAD2DEG,
        {scale, scale, scale}, WHITE);
    endWorldShader();

    const Vector3 matterCenter{
        position.x,
        position.y + PortalCenterHeight * scale,
        position.z,
    };
    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    BeginBlendMode(BLEND_ALPHA);
    const bool masksBackgroundInk =
        resources_.portalMatterShader().valid();
    if (masksBackgroundInk) {
        BeginShaderMode(resources_.portalMatterShader().get());
    }
    drawPortalMatterPlane(
        matter.get(), matterCenter, yawRadians,
        MatterRadius * scale * aperture * (1-pulse*.08F), matterRotationRadians+pulse*.28F,
        {255,static_cast<unsigned char>(255-55*pulse),255,255});
    if (masksBackgroundInk) {
        EndShaderMode();
    }
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    drawPortalGlowRing(
        matterCenter, yawRadians, MatterRadius * scale * aperture * (1+pulse*.08F),
        {194, 72, 255, static_cast<unsigned char>(72+100*pulse)});
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    const Vector3 forward{std::cos(yawRadians),0,-std::sin(yawRadians)};
    if(arrivalProgress>=0 && arrivalProgress<1 && Vector3DotProduct(Vector3Subtract(lighting.cameraPosition,matterCenter),forward)>=0) {
        beginWorldShader(lighting);
        WorldMaterialState energy{};energy.unlitAmount=1;energy.screenAoAmount=0;
        energy.inkOutlineEligible=false;energy.inkOccluder=false;
        setWorldMaterial(energy);beginColorOnlyWorldEffects();
        fire_wand_effect::beginBatch(false);
        portal_arrival::gate(matterCenter,{std::cos(yawRadians),0,-std::sin(yawRadians)},
            lighting.cameraPosition,arrivalProgress,settings_.particles && detailedArrival);
        fire_wand_effect::endBatch();endColorOnlyWorldEffects();endWorldShader();
    }
    return true;
}

bool Renderer::drawWorldLandmark(
    std::size_t variant, Vector3 position,
    float yawRadians, Color tint, float scale) {
    auto& resource = resources_.worldLandmarkModel(variant);
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    prepareModelShader(model);
    const BoundingBox bounds = resource.visualBounds();
    const float authoredHeight = std::max(
        0.001F, bounds.max.y - bounds.min.y);
    const float TargetHeight = variant >= 2 ? 2.8F : 6.76F;
    const float modelScale = TargetHeight / authoredHeight;
    position.y -= bounds.min.y * modelScale * scale;
    DrawModelEx(
        model, position, {0.0F, 1.0F, 0.0F},
        yawRadians * RAD2DEG,
        {modelScale * scale, modelScale * scale,
         modelScale * scale},
        tint);
    return true;
}

BoundingBox Renderer::worldLandmarkWorldBounds(
    std::size_t variant, Vector3 position,
    float yawRadians, float scale) {
    ModelResource& resource = resources_.worldLandmarkModel(variant);
    if (!resource.valid() || !world_transforms::finite(position) ||
        !std::isfinite(yawRadians) || !std::isfinite(scale) ||
        scale <= 0.0F) {
        return {};
    }
    const BoundingBox localBounds = resource.visualBounds();
    const float authoredHeight = std::max(
        0.001F, localBounds.max.y - localBounds.min.y);
    const float TargetHeight = variant >= 2 ? 2.8F : 6.76F;
    const float modelScale = TargetHeight / authoredHeight * scale;
    position.y -= localBounds.min.y * modelScale;
    const Matrix transform = MatrixMultiply(
        resource.get().transform,
        MatrixMultiply(
            MatrixMultiply(
                MatrixScale(modelScale, modelScale, modelScale),
                MatrixRotateY(yawRadians)),
            MatrixTranslate(position.x, position.y, position.z)));
    return world_transforms::transformBounds(localBounds, transform);
}

} // namespace ian
