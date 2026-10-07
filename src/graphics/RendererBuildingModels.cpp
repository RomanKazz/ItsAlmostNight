#include "graphics/Renderer.hpp"
#include "graphics/CoreCrystalModel.hpp"
#include "graphics/GateModel.hpp"
#include "graphics/GunTurretModel.hpp"
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
                          float scaleFactor) {
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
    const Matrix localPitch = MatrixMultiply(
        MatrixMultiply(
            MatrixTranslate(
                0.0F, -static_cast<float>(CannonPitchPivotY), 0.0F),
            MatrixRotateX(pitchRadians)),
        MatrixTranslate(
            0.0F, static_cast<float>(CannonPitchPivotY), 0.0F));
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
    for (int meshIndex = 2; meshIndex < model.meshCount; ++meshIndex) {
        drawMesh(meshIndex, baseTransform);
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

bool Renderer::drawCatapult(
    Vector3 position, float yawRadians, float armPitchRadians,
    bool loaded, Color tint, float scaleFactor) {
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
        const Matrix localPitch = MatrixMultiply(
            MatrixMultiply(
                MatrixTranslate(
                    0.0F, -static_cast<float>(CatapultPitchPivotY), 0.0F),
                MatrixRotateX(armPitchRadians)),
            MatrixTranslate(
                0.0F, static_cast<float>(CatapultPitchPivotY), 0.0F));
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
        for (int meshIndex = 2; meshIndex < model.meshCount; ++meshIndex) {
            drawMesh(meshIndex, bodyTransform);
        }
    }

    if (loaded) {
        const Vec3 base{position.x, position.y, position.z};
        Vec3 socket = catapultMuzzleWorldPosition(
            base, yawRadians, armPitchRadians);
        socket.x = base.x + (socket.x - base.x) * scaleFactor;
        socket.y = base.y + (socket.y - base.y) * scaleFactor;
        socket.z = base.z + (socket.z - base.z) * scaleFactor;
        static_cast<void>(drawCatapultBall(
            {static_cast<float>(socket.x), static_cast<float>(socket.y),
             static_cast<float>(socket.z)}, tint));
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
                            float pitchRadians) {
    auto& resource = resources_.crossbowModel();
    if (!resource.valid()) {
        return false;
    }
    Model& model = resource.get();
    prepareModelShader(model);
    constexpr float PitchPivotY = 0.14931101F;
    const Matrix localPitch = MatrixMultiply(
        MatrixMultiply(
            MatrixTranslate(0.0F, -PitchPivotY, 0.0F),
            MatrixRotateX(pitchRadians)),
        MatrixTranslate(0.0F, PitchPivotY, 0.0F));
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
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        material.maps[MATERIAL_MAP_DIFFUSE].color = ColorTint(original, tint);
        // After the authored collider is stripped, mesh 0 is the complete
        // upper assembly named "arrow" under pitch_pivot. Mesh 1 is the
        // yaw-only lower weapon body.
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform,
                     meshIndex == 0 ? pitchedTransform : yawTransform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
    }
    return true;
}

Vec3 Renderer::crossbowMuzzlePosition(
    Vec3 position, float yawRadians, float pitchRadians) const {
    constexpr double PivotY = 0.14931100606918335;
    constexpr Vec3 AuthoredMuzzle{
        0.0, 0.7464950680732727, -0.1994807869195938};
    const double relativeY = AuthoredMuzzle.y - PivotY;
    const double cosinePitch = std::cos(static_cast<double>(pitchRadians));
    const double sinePitch = std::sin(static_cast<double>(pitchRadians));
    const double pitchedY =
        PivotY + relativeY * cosinePitch - AuthoredMuzzle.z * sinePitch;
    const double pitchedZ =
        relativeY * sinePitch + AuthoredMuzzle.z * cosinePitch;
    const double cosineYaw = std::cos(static_cast<double>(yawRadians));
    const double sineYaw = std::sin(static_cast<double>(yawRadians));
    return {
        position.x + pitchedZ * sineYaw,
        position.y + pitchedY,
        position.z + pitchedZ * cosineYaw,
    };
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

bool Renderer::drawTurretBullet(Vector3 position, Vector3 direction,
                                Color tint, float scale) {
    auto& resource = resources_.turretBulletModel();
    if (!resource.valid() ||
        Vector3LengthSqr(direction) <= 0.000001F) return false;
    Model& model = resource.get();
    Shader* shader = nullptr;
    if (shadowPassOpen_ && resources_.shadowShader().valid()) {
        shader = &resources_.shadowShader().get();
    } else if (worldShaderActive_ && resources_.worldShader().valid()) {
        shader = &resources_.worldShader().get();
    }
    const WorldMaterialState previousMaterial = worldMaterial_;
    if (worldShaderActive_) {
        WorldMaterialState bulletMaterial{};
        bulletMaterial.baseColor = {1.4F, 1.4F, 1.4F, 1.0F};
        bulletMaterial.unlitAmount = 1.0F;
        bulletMaterial.screenAoAmount = 0.0F;
        bulletMaterial.inkOutlineEligible = false;
        bulletMaterial.inkOccluder = true;
        setWorldMaterial(bulletMaterial);
    }
    const Vector3 forward = Vector3Normalize(direction);
    const float yaw = std::atan2(-forward.x, -forward.z);
    const float horizontal = std::sqrt(
        forward.x * forward.x + forward.z * forward.z);
    const float pitch = std::atan2(forward.y, horizontal);
    const Matrix transform = MatrixMultiply(
        MatrixMultiply(MatrixScale(
                           0.8F * scale,
                           0.8F * scale,
                           0.8F * scale),
            MatrixMultiply(MatrixRotateX(pitch), MatrixRotateY(yaw))),
        MatrixTranslate(position.x, position.y, position.z));
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const int materialIndex = model.meshMaterial[meshIndex];
        Material& material = model.materials[materialIndex];
        const Color original = material.maps[MATERIAL_MAP_DIFFUSE].color;
        const Shader originalShader = material.shader;
        const Texture2D originalTexture =
            material.maps[MATERIAL_MAP_DIFFUSE].texture;
        if (shader) material.shader = *shader;
        if (resources_.fallbackTexture().valid()) {
            material.maps[MATERIAL_MAP_DIFFUSE].texture =
                resources_.fallbackTexture().get();
        }
        material.maps[MATERIAL_MAP_DIFFUSE].color =
            ColorTint({255, 125, 35, 255}, tint);
        DrawMesh(model.meshes[meshIndex], material,
                 MatrixMultiply(model.transform, transform));
        material.maps[MATERIAL_MAP_DIFFUSE].color = original;
        material.maps[MATERIAL_MAP_DIFFUSE].texture = originalTexture;
        material.shader = originalShader;
    }
    if (worldShaderActive_) setWorldMaterial(previousMaterial);
    return true;
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
    if (model.meshCount == 7) {
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
    float scale) {
    ModelResource& portal = resources_.enemyPortalModel();
    TextureResource& matter = resources_.portalMatterTexture();
    if (!portal.valid() || !matter.valid() || scale <= 0.001F) {
        return false;
    }

    constexpr float PortalCenterHeight = 1.74F;
    constexpr float PortalGroundLift = -0.24F;
    constexpr float MatterRadius = 2.13F;
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
        MatterRadius * scale, matterRotationRadians,
        WHITE);
    if (masksBackgroundInk) {
        EndShaderMode();
    }
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ADDITIVE);
    drawPortalGlowRing(
        matterCenter, yawRadians, MatterRadius * scale,
        {194, 72, 255, 72});
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
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
    constexpr float TargetHeight = 6.76F;
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
    constexpr float TargetHeight = 6.76F;
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

bool Renderer::drawMine(Vector3 position, float yawRadians,
                        Color tint, float scale) {
    return drawResourceProducer(
        BuildingType::CrystalMine, position, yawRadians,
        tint, scale);
}

} // namespace ian
